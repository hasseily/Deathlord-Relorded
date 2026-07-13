#include "Frame.h"
#include "DlrlHooks.h"

#include "Emulator/CardManager.h"
#include "Emulator/Core.h"
#include "Emulator/CPU.h"
#include "Emulator/Harddisk.h"
#include "Emulator/Interface.h"
#include "Emulator/Keyboard.h"
#include "Emulator/Memory.h"
#include "Emulator/NTSC.h"
#include "Emulator/RGBMonitor.h"
#include "Emulator/Speaker.h"
#include "Emulator/Video.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

struct Options
{
    struct KeyEvent { int frame; BYTE key; };
    std::filesystem::path hdv;
    std::filesystem::path output = "dlrl-boot.bmp";
    int frames = 600;
    int keyAtFrame = -1;
    BYTE key = ' ';
    std::vector<KeyEvent> keyEvents;
};

bool ParseOptions(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--hdv" && i + 1 < argc) options.hdv = argv[++i];
        else if (arg == "--output" && i + 1 < argc) options.output = argv[++i];
        else if (arg == "--frames" && i + 1 < argc) options.frames = std::stoi(argv[++i]);
        else if (arg == "--key-at" && i + 1 < argc) options.keyAtFrame = std::stoi(argv[++i]);
        else if (arg == "--key" && i + 1 < argc) options.key = static_cast<BYTE>(argv[++i][0]);
        else if (arg == "--key-event" && i + 1 < argc)
        {
            const std::string event = argv[++i];
            const auto separator = event.find(':');
            if (separator == std::string::npos || separator + 1 >= event.size()) return false;
            options.keyEvents.push_back({std::stoi(event.substr(0, separator)),
                                         static_cast<BYTE>(event[separator + 1])});
        }
        else return false;
    }
    return !options.hdv.empty() && options.frames > 0;
}

void Put16(std::ofstream& out, uint16_t value)
{
    const char bytes[] = {
        static_cast<char>(value), static_cast<char>(value >> 8)
    };
    out.write(bytes, sizeof(bytes));
}

void Put32(std::ofstream& out, uint32_t value)
{
    const char bytes[] = {
        static_cast<char>(value), static_cast<char>(value >> 8),
        static_cast<char>(value >> 16), static_cast<char>(value >> 24)
    };
    out.write(bytes, sizeof(bytes));
}

bool WriteFramebufferBmp(const std::filesystem::path& path)
{
    Video& video = GetVideo();
    const uint32_t width = video.GetFrameBufferWidth();
    const uint32_t height = video.GetFrameBufferHeight();
    const uint32_t pixelBytes = width * height * 4;

    std::filesystem::create_directories(path.parent_path().empty()
                                            ? std::filesystem::path(".")
                                            : path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out.write("BM", 2);
    Put32(out, 14 + 40 + pixelBytes);
    Put16(out, 0);
    Put16(out, 0);
    Put32(out, 14 + 40);

    Put32(out, 40);
    Put32(out, width);
    Put32(out, height);
    Put16(out, 1);
    Put16(out, 32);
    Put32(out, 0);
    Put32(out, pixelBytes);
    Put32(out, 2835);
    Put32(out, 2835);
    Put32(out, 0);
    Put32(out, 0);

    out.write(reinterpret_cast<const char*>(video.GetFrameBuffer()), pixelBytes);
    return out.good();
}

uint64_t FramebufferHash()
{
    Video& video = GetVideo();
    const auto* pixels = video.GetFrameBuffer();
    const size_t size = static_cast<size_t>(video.GetFrameBufferWidth())
                      * static_cast<size_t>(video.GetFrameBufferHeight()) * 4;
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= pixels[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

std::filesystem::path MakeWorkingCopy(const std::filesystem::path& source)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path()
                    / ("dlrl-capture-" + std::to_string(stamp) + ".hdv");
    std::filesystem::copy_file(source, path, std::filesystem::copy_options::overwrite_existing);
    return path;
}

} // namespace

int main(int argc, char** argv)
{
    dlrl::DlrlHooks hooks;
    Options options;
    if (!ParseOptions(argc, argv, options))
    {
        std::fprintf(stderr, "Usage: dlrl_capture --hdv PATH [--frames N] [--output PATH]\n");
        return 2;
    }
    if (!std::filesystem::exists(options.hdv))
    {
        std::fprintf(stderr, "SKIP: test HDV not found: %s\n", options.hdv.string().c_str());
        return 77;
    }

    const std::filesystem::path workingHdv = MakeWorkingCopy(options.hdv);
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_AUDIO))
    {
        std::fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
        return 3;
    }

    dlrl::Frame::SetResourceDir(std::filesystem::path(DLRL_RESOURCE_DIR));
    g_nAppMode = MODE_RUNNING;
    SpkrInitialize();
    SetApple2Type(A2TYPE_APPLE2EENHANCED);
    RGB_SetVideocard(Video7_SL7, 15, 0);

    Video& video = GetVideo();
    video.SetVideoType(VT_COLOR_IDEALIZED);
    video.SetVideoStyle(VS_COLOR_VERTICAL_BLEND);
    video.SetVideoRefreshRate(VR_60HZ);
    SetCurrentCLK6502();

    GetCardMgr().Insert(SLOT7, CT_GenericHDD, false);
    GetCardMgr().Insert(SLOT4, CT_MockingboardC, false);
    GetFrame().Initialize(true);
    MemInitialize();
    GetCardMgr().Reset(true);
    hooks.Attach();

    auto* hdc = static_cast<HarddiskInterfaceCard*>(GetCardMgr().GetObj(SLOT7));
    if (!hdc || !hdc->Insert(HARDDISK_1, workingHdv.string()))
    {
        std::fprintf(stderr, "Unable to insert working HDV copy\n");
        return 4;
    }

    const uint32_t cyclesPerFrame = NTSC_GetCyclesPerFrame();
    const uint32_t cyclesPerBatch = static_cast<uint32_t>(g_fCurrentCLK6502 * 1e-3);
    for (int frame = 0; frame < options.frames; ++frame)
    {
        if (frame == options.keyAtFrame)
            KeybQueueKeypress(options.key, ASCII);
        for (const Options::KeyEvent& event : options.keyEvents)
            if (frame == event.frame) KeybQueueKeypress(event.key, ASCII);
        uint32_t frameCycles = 0;
        while (frameCycles < cyclesPerFrame)
        {
            const uint32_t remaining = cyclesPerFrame - frameCycles;
            const uint32_t requested = remaining < cyclesPerBatch ? remaining : cyclesPerBatch;
            const uint32_t executed = CpuExecute(requested, true);
            frameCycles += executed;
            GetCardMgr().Update(executed);
            SpkrUpdate(executed);
        }
        g_dwCyclesThisFrame = 0;
        GetFrame().VideoRedrawScreen();
    }

    const uint64_t hash = FramebufferHash();
    const bool wrote = WriteFramebufferBmp(options.output);
    std::printf("DLRL HDV capture: frames=%d pc=$%04X framebuffer=%ux%u fnv1a64=%016llx output=%s\n",
                options.frames, regs.pc,
                video.GetFrameBufferWidth(), video.GetFrameBufferHeight(),
                static_cast<unsigned long long>(hash), options.output.string().c_str());

    hdc->Unplug(HARDDISK_1);
    GetCardMgr().Destroy();
    SpkrDestroy();
    hooks.Detach();
    MemDestroy();
    GetFrame().Destroy();
    SDL_Quit();
    std::error_code ignored;
    std::filesystem::remove(workingHdv, ignored);

    return wrote ? 0 : 5;
}
