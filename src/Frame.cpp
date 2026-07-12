// StdAfx pulls in Windows.h (and the cross-platform shim on POSIX) so that
// BYTE/WORD/DWORD/HANDLE etc. used throughout FrameBase and Video resolve.
#include "Emulator/StdAfx.h"

#include "Frame.h"
#include "PropertySheet.h"
#include "AudioOutput.h"

#include "Emulator/Interface.h"
#include "Emulator/Video.h"
#include "Emulator/Configuration/IPropertySheet.h"
#include "Emulator/ResourceIds.h"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace dlrl
{

std::filesystem::path Frame::s_resourceDir;

void Frame::SetResourceDir(std::filesystem::path dir)
{
    s_resourceDir = std::move(dir);
}

Frame::Frame() = default;
Frame::~Frame() = default;

void Frame::Initialize(bool resetVideoState)
{
    Video& video = GetVideo();
    const size_t pixels = static_cast<size_t>(video.GetFrameBufferWidth())
                        * static_cast<size_t>(video.GetFrameBufferHeight());
    m_framebufferBuf.assign(pixels * 4, 0);
    video.Initialize(m_framebufferBuf.data(), resetVideoState);
}

void Frame::Destroy(void)
{
    GetVideo().Destroy();
    m_framebufferBuf.clear();
}

int Frame::FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT /*uType*/)
{
    std::fprintf(stderr, "[%s] %s\n",
                 lpCaption ? lpCaption : "Deathlord Relorded",
                 lpText ? lpText : "");
    return 0;
}

namespace
{

const char* ResourceFilename(WORD id)
{
    switch (id)
    {
    case IDR_APPLE2E_ENHANCED_ROM:       return "Apple2e_Enhanced.rom";
    case IDR_APPLE2E_ENHANCED_VIDEO_ROM: return "Apple2e_Enhanced_Video.rom";
    case IDR_HDDRVR_FW:                  return "Hddrvr.bin";
    case IDR_HDC_SMARTPORT_FW:           return "HDC-SmartPort.bin";
    case IDR_MOCKINGBOARD_D_FW:          return "Mockingboard-D.rom";
    case IDR_TKCLOCK_FW:                 return "TKClock.rom";
    default:                             return nullptr;
    }
}

bool LoadFile(const std::filesystem::path& path, std::vector<BYTE>& out)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto size = f.tellg();
    if (size <= 0) return false;
    out.resize(static_cast<size_t>(size));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(out.data()), out.size());
    return f.good() || f.eof();
}

// Minimal Windows BMP parser — extracts the raw pixel data given an
// already-loaded .bmp blob. Returns nullptr on malformed input. Mirrors
// upstream linux/bitmap.cpp::extractBitmapData. Output rows are flipped
// into top-down order so they match what the caller (NTSC_CharSet) expects.
bool ExtractBitmapPixels(const std::vector<BYTE>& bmp, std::vector<BYTE>& outPixels)
{
    if (bmp.size() < 54) return false;
    if (bmp[0] != 0x42 || bmp[1] != 0x4D) return false;

    auto u32 = [&](size_t off) -> uint32_t {
        return uint32_t(bmp[off]) | (uint32_t(bmp[off+1]) << 8) |
               (uint32_t(bmp[off+2]) << 16) | (uint32_t(bmp[off+3]) << 24);
    };
    auto i32 = [&](size_t off) -> int32_t { return static_cast<int32_t>(u32(off)); };

    if (u32(2) != bmp.size())          return false;
    if (u32(14) != 40)                 return false;     // BITMAPINFOHEADER

    const uint32_t pixelOff = u32(10);
    const int32_t  width    = i32(18);
    const int32_t  height   = i32(22);
    const uint32_t imgSize  = u32(34);

    if (width <= 0 || height <= 0)     return false;
    if (pixelOff + imgSize > bmp.size()) return false;

    const size_t rowBytes = imgSize / static_cast<size_t>(height);
    outPixels.resize(imgSize);
    for (int32_t row = 0; row < height; ++row)
    {
        // BMPs are stored bottom-up.
        std::memcpy(outPixels.data() + (height - row - 1) * rowBytes,
                    bmp.data() + pixelOff + row * rowBytes,
                    rowBytes);
    }
    return true;
}

} // namespace

BYTE* Frame::GetResource(WORD id, LPCSTR /*lpType*/, uint32_t expectedSize)
{
    const char* filename = ResourceFilename(id);
    if (!filename) return nullptr;

    const std::filesystem::path path = s_resourceDir / filename;
    if (!LoadFile(path, m_resourceBuf))
    {
        std::fprintf(stderr, "Frame::GetResource: failed to load %s\n", path.string().c_str());
        return nullptr;
    }
    if (m_resourceBuf.size() != expectedSize)
    {
        std::fprintf(stderr, "Frame::GetResource: %s size mismatch (got %zu, want %u)\n",
                     filename, m_resourceBuf.size(), expectedSize);
        return nullptr;
    }
    return m_resourceBuf.data();
}

void Frame::GetBitmap(WORD id, LONG cb, LPVOID lpvBits)
{
    const char* filename = ResourceFilename(id);
    if (!filename)
    {
        std::memset(lpvBits, 0, cb);
        return;
    }

    std::vector<BYTE> bmp;
    if (!LoadFile(s_resourceDir / filename, bmp))
    {
        std::fprintf(stderr, "Frame::GetBitmap: failed to load %s\n", filename);
        std::memset(lpvBits, 0, cb);
        return;
    }

    std::vector<BYTE> pixels;
    if (!ExtractBitmapPixels(bmp, pixels))
    {
        std::fprintf(stderr, "Frame::GetBitmap: malformed BMP %s\n", filename);
        std::memset(lpvBits, 0, cb);
        return;
    }

    const size_t copy = (static_cast<size_t>(cb) < pixels.size()) ? static_cast<size_t>(cb) : pixels.size();
    std::memcpy(lpvBits, pixels.data(), copy);
    if (copy < static_cast<size_t>(cb))
    {
        std::memset(static_cast<BYTE*>(lpvBits) + copy, 0, cb - copy);
    }
}

std::shared_ptr<SoundBuffer> Frame::CreateSoundBuffer(uint32_t bufferBytes, uint32_t sampleRate,
                                                      int channels, const char* voiceName)
{
    return AudioOutput::Create(bufferBytes, sampleRate, channels, voiceName);
}

} // namespace dlrl

// DSAvailable() lives in linuxsoundbuffer.cpp (vendored alongside the
// LinuxSoundBuffer ring) and unconditionally returns true. NAC's actual
// audio backend is the AudioOutput SDL3 wrapper above.

// ---------------------------------------------------------------------------
// Interface.h singletons. Each lives in static storage; the emulator core
// reaches all three through these accessors.
// ---------------------------------------------------------------------------
FrameBase& GetFrame(void)
{
    static dlrl::Frame s_frame;
    return s_frame;
}

Video& GetVideo(void)
{
    static Video s_video;
    return s_video;
}

IPropertySheet& GetPropertySheet(void)
{
    static dlrl::PropertySheet s_propertySheet;
    return s_propertySheet;
}
