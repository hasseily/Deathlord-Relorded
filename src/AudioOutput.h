#pragma once

#include "Emulator/StdAfx.h"
#include "Emulator/linuxsoundbuffer.h"

#include <SDL3/SDL_audio.h>

#include <memory>

namespace dlrl
{

// SDL3-backed SoundBuffer. Inherits LinuxSoundBuffer (a platform-neutral
// ring buffer that the emulator's Speaker / Mockingboard / SSI263 modules
// write into via Lock/Unlock); we open an SDL_AudioStream pointed at the
// default playback device, and the SDL audio thread pulls from the ring
// via the staticAudioCallback.
class AudioOutput : public LinuxSoundBuffer
{
public:
    AudioOutput(uint32_t bufferBytes, uint32_t sampleRate, int channels, const char* voiceName);
    ~AudioOutput() override;

    HRESULT Stop() override;
    HRESULT Play(DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags) override;

    static std::shared_ptr<SoundBuffer> Create(uint32_t bufferBytes,
                                               uint32_t sampleRate,
                                               int channels,
                                               const char* voiceName);

private:
    static void SDLCALL StreamCallback(void* userdata,
                                       SDL_AudioStream* stream,
                                       int additional, int total);
    void Pull(uint8_t* dst, int len);

    SDL_AudioStream* m_stream    = nullptr;
    SDL_AudioDeviceID m_device   = 0;
    SDL_AudioSpec    m_spec      = {};
    int              m_silence   = 0;
};

} // namespace dlrl
