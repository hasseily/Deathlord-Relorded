#include "Frame.h"

#include "Emulator/CardManager.h"
#include "Emulator/Core.h"
#include "Emulator/Interface.h"
#include "Emulator/Memory.h"
#include "Emulator/RGBMonitor.h"
#include "Emulator/Video.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>

int main()
{
    dlrl::Frame::SetResourceDir(std::filesystem::path(DLRL_RESOURCE_DIR));

    g_nAppMode = MODE_RUNNING;
    SetApple2Type(A2TYPE_APPLE2EENHANCED);
    RGB_SetVideocard(Video7_SL7, 15, 0);

    Video& video = GetVideo();
    video.SetVideoType(VT_COLOR_IDEALIZED);
    video.SetVideoStyle(VS_COLOR_VERTICAL_BLEND);
    video.SetVideoRefreshRate(VR_60HZ);
    SetCurrentCLK6502();

    GetCardMgr().Insert(SLOT7, CT_GenericHDD, false);
    GetFrame().Initialize(true);
    MemInitialize();
    GetCardMgr().Reset(true);
    GetFrame().VideoRedrawScreen();

    const bool memoryReady = MemGetMainPtr(0) != nullptr && MemGetAuxPtr(0) != nullptr;
    const bool videoReady = video.GetFrameBuffer() != nullptr
                         && video.GetFrameBufferWidth() > 0
                         && video.GetFrameBufferHeight() > 0;

    std::printf("Apple //e core ready: RAM=%s framebuffer=%ux%u\n",
                memoryReady ? "yes" : "no",
                video.GetFrameBufferWidth(), video.GetFrameBufferHeight());

    MemDestroy();
    GetFrame().Destroy();

    return memoryReady && videoReady ? 0 : 1;
}
