#include "Frame.h"
#include "DlrlHooks.h"
#include "MapTravel.h"
#include "HackingChecks.h"

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

#include <algorithm>
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
    std::filesystem::path memoryOutput;
    std::string teleport;
    bool testMapTravel = false;
    bool testMapExits = false;
    bool testHacking = false;
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
        else if (arg == "--memory-output" && i + 1 < argc) options.memoryOutput = argv[++i];
        else if (arg == "--teleport" && i + 1 < argc) options.teleport = argv[++i];
        else if (arg == "--test-map-travel") options.testMapTravel = true;
        else if (arg == "--test-map-exits") options.testMapExits = true;
        else if (arg == "--test-hacking") options.testHacking = true;
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
    dlrl::MapTravel travel;
    std::vector<dlrl::TeleportRequest> trips;
    std::size_t nextTrip = 0;
    bool traveling = false;
    bool exiting = false;
    int travelStarted = 0;
    bool travelFailed = false;
    bool hackingVerified = !options.testHacking;
    if (options.testMapTravel || options.testMapExits || !options.teleport.empty())
    {
        std::string error;
        if (!travel.Load(workingHdv,error))
        { std::fprintf(stderr,"Map catalog: %s\n",error.c_str()); return 6; }
        for (const auto& map : travel.Destinations())
        {
            if (options.testMapExits && map.type != 2) continue;
            if (!options.testMapTravel && !options.testMapExits && map.name != options.teleport) continue;
            for (int floor=1;floor<=(options.testMapExits ? 1 : map.floors);++floor)
            {
                std::array<std::uint8_t,4096> tiles{};
                if (!travel.Preview(map,floor,tiles,error))
                { std::fprintf(stderr,"Preview %s: %s\n",map.name.c_str(),error.c_str()); return 6; }
                // Choose plain terrain, away from doors, stairs, and monsters.
                const int size = dlrl::MapTravel::Size(map);
                int x = 16, y = 1;
                for (int p=size+1;p<size*(size-1);++p)
                    if (tiles[p] == (map.type==1 ? 0x34 : 0x2E))
                    { x=p%size; y=p/size; break; }
                if (options.testMapExits)
                {
                    const auto end = tiles.begin() + size * size;
                    const auto stairs = std::find_if(tiles.begin(), end,
                        [](auto tile) { return tile % 80 == 4; });
                    if (stairs == end || stairs - tiles.begin() >= size * (size - 1))
                    { std::fprintf(stderr,"No usable exit stairs: %s\n",map.name.c_str()); return 6; }
                    x = (stairs - tiles.begin()) % size;
                    y = (stairs - tiles.begin()) / size + 1;
                }
                trips.push_back({map,floor,x,y});
            }
        }
        if (trips.empty()) { std::fprintf(stderr,"No matching teleport destination\n"); return 6; }
    }
    const uint32_t cyclesPerBatch = static_cast<uint32_t>(g_fCurrentCLK6502 * 1e-3);
    for (int frame = 0; frame < options.frames; ++frame)
    {
        if (options.testHacking && hooks.CanTeleport())
        {
            hackingVerified = dlrl::RunHackingChecks(hooks);
            break;
        }
        if (exiting)
        {
            const auto& map = trips[nextTrip-1].destination;
            const int expectedType = map.insideTown ? 0 : 1;
            if (MemGetMainPtr(dlrl::deathlord::MapType)[0] == expectedType && hooks.CanTeleport())
            {
                const bool correct = MemGetMainPtr(dlrl::deathlord::MapOverlandX)[0] == map.worldX
                    && MemGetMainPtr(dlrl::deathlord::MapOverlandY)[0] == map.worldY
                    && (!map.insideTown || (MemGetMainPtr(0xFC51)[0] == map.parentTown.track
                                           && MemGetMainPtr(0xFC52)[0] == map.parentTown.sector));
                std::printf("Exit %s: %s\n",correct ? "PASS" : "FAIL",map.name.c_str());
                if (!correct) { travelFailed=true; break; }
                exiting = false;
                if (nextTrip==trips.size())
                { std::printf("Map exits verified: %zu dungeons\n",trips.size()); break; }
            }
            else if (frame-travelStarted>1500)
            { std::fprintf(stderr,"Exit timed out: %s at PC $%04X\n",map.name.c_str(),regs.pc); travelFailed=true; break; }
        }
        if (traveling && !hooks.TeleportPending())
        {
            using namespace dlrl::deathlord;
            const auto& trip = trips[nextTrip-1];
            const int expectedX = trip.x + (trip.destination.type==2 ? ((trip.floor-1)&1)*32 : 0);
            const int expectedY = trip.y + (trip.destination.type==2 ? (((trip.floor-1)&3)/2)*32 : 0);
            std::array<std::uint8_t,4096> preview{};
            std::string error;
            bool terrainOkay = travel.Preview(trip.destination,trip.floor,preview,error);
            const int size = dlrl::MapTravel::Size(trip.destination);
            int matchingTiles = 0;
            for (int y=0;y<size;++y)
                for (int x=0;x<size;++x)
                {
                    const int mapX=x+expectedX-trip.x, mapY=y+expectedY-trip.y;
                    if (MemGetMainPtr(GameMap)[mapY*64+mapX]%80==preview[y*size+x]%80) ++matchingTiles;
                }
            terrainOkay = terrainOkay && matchingTiles > size*size*9/10;
            const bool okay = terrainOkay && MemGetMainPtr(MapType)[0]==trip.destination.type
                && MemGetMainPtr(MapX)[0]==expectedX && MemGetMainPtr(MapY)[0]==expectedY
                && MemGetMainPtr(MapFloor)[0]==trip.floor;
            std::printf("Travel %s: %s floor %d at %d,%d (expected %d,%d), type %d, matching tiles %d/%d\n",
                okay?"PASS":"FAIL",trip.destination.name.c_str(),MemGetMainPtr(MapFloor)[0],
                MemGetMainPtr(MapX)[0],MemGetMainPtr(MapY)[0],expectedX,expectedY,MemGetMainPtr(MapType)[0],matchingTiles,size*size);
            std::fflush(stdout);
            traveling = false;
            if (!okay) { travelFailed=true; break; }
            if (options.testMapExits)
            {
                KeybQueueKeypress('I',ASCII);
                exiting = true;
                travelStarted = frame;
            }
            else if (nextTrip == trips.size())
            { std::printf("Map travel verified: %zu destinations and floors\n",trips.size()); break; }
        }
        if (traveling && frame-travelStarted>1500)
        { std::fprintf(stderr,"Teleport timed out at PC $%04X\n",regs.pc); travelFailed=true; break; }
        if (!traveling && !exiting && nextTrip<trips.size() && hooks.CanTeleport())
        {
            std::string error;
            if (!hooks.QueueTeleport(trips[nextTrip],error))
            { std::fprintf(stderr,"Teleport failed: %s\n",error.c_str()); travelFailed=true; break; }
            std::printf("Travel begin: %s floor %d\n",trips[nextTrip].destination.name.c_str(),trips[nextTrip].floor);
            std::fflush(stdout);
            ++nextTrip;
            traveling = true;
            travelStarted = frame;
        }
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
    if (!options.memoryOutput.empty())
    {
        std::ofstream memory(options.memoryOutput, std::ios::binary);
        memory.write(reinterpret_cast<const char*>(MemGetMainPtr(0)), 0x10000);
    }
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

    return !hackingVerified || travelFailed || traveling || exiting || nextTrip<trips.size() ? 6 : wrote ? 0 : 5;
}
