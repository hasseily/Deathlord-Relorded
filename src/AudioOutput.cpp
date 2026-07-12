#include "AudioOutput.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace dlrl
{

AudioOutput::AudioOutput(uint32_t bufferBytes, uint32_t sampleRate, int channels, const char* voiceName)
    : LinuxSoundBuffer(bufferBytes, sampleRate, channels, voiceName)
{
    m_spec.freq     = (int)mySampleRate;
    m_spec.format   = SDL_AUDIO_S16LE;
    m_spec.channels = (int)myChannels;

    m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                         &m_spec, &AudioOutput::StreamCallback, this);
    if (!m_stream)
    {
        std::fprintf(stderr, "AudioOutput[%s]: SDL_OpenAudioDeviceStream failed: %s\n",
                     voiceName, SDL_GetError());
        return;
    }
    m_device  = SDL_GetAudioStreamDevice(m_stream);
    m_silence = SDL_GetSilenceValueForFormat(m_spec.format);

    // SDL audio devices open paused. Resume immediately so the callback
    // starts pumping; the ring will be silence until the emulator's
    // Speaker / Mockingboard / SSI263 modules call Lock/Unlock to fill it.
    SDL_ResumeAudioStreamDevice(m_stream);
}

AudioOutput::~AudioOutput()
{
    if (m_stream)
        SDL_DestroyAudioStream(m_stream);
}

HRESULT AudioOutput::Stop()
{
    const HRESULT res = LinuxSoundBuffer::Stop();
    if (m_device) SDL_PauseAudioDevice(m_device);
    return res;
}

HRESULT AudioOutput::Play(DWORD r1, DWORD r2, DWORD flags)
{
    const HRESULT res = LinuxSoundBuffer::Play(r1, r2, flags);
    if (m_device) SDL_ResumeAudioDevice(m_device);
    return res;
}

void AudioOutput::Pull(uint8_t* dst, int len)
{
    LPVOID p1 = nullptr, p2 = nullptr;
    DWORD  n1 = 0,        n2 = 0;
    const size_t got = Read(len, &p1, &n1, &p2, &n2);

    // Apply the buffer's logarithmic volume by mixing into a zeroed dst.
    std::vector<uint8_t> staged(got);
    uint8_t* w = staged.data();
    if (p1 && n1) { std::memcpy(w, p1, n1); w += n1; }
    if (p2 && n2) { std::memcpy(w, p2, n2); w += n2; }

    std::memset(dst, 0, len);
    if (got)
    {
        const double logVol = GetLogarithmicVolume();
        const double linVol = (logVol > 0.99) ? 1.0 : -std::log(1.0 - logVol) / std::log(100.0);
        const float  vol    = std::clamp((float)linVol, 0.0f, 1.0f);
        SDL_MixAudio(dst, staged.data(), m_spec.format, (Uint32)got, vol);
    }

    if ((int)got < len)
        std::memset(dst + got, m_silence, (size_t)len - got);
}

void SDLCALL AudioOutput::StreamCallback(void* userdata,
                                         SDL_AudioStream* stream,
                                         int additional, int /*total*/)
{
    if (additional <= 0) return;
    auto* self = static_cast<AudioOutput*>(userdata);
    Uint8* buf = (Uint8*)SDL_malloc((size_t)additional);
    if (!buf) return;
    self->Pull(buf, additional);
    SDL_PutAudioStreamData(stream, buf, additional);
    SDL_free(buf);
}

std::shared_ptr<SoundBuffer> AudioOutput::Create(uint32_t bufferBytes,
                                                 uint32_t sampleRate,
                                                 int channels,
                                                 const char* voiceName)
{
    auto buf = std::make_shared<AudioOutput>(bufferBytes, sampleRate, channels, voiceName);
    if (!buf->m_stream) return nullptr;
    return buf;
}

} // namespace dlrl
