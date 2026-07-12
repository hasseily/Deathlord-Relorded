#pragma once

// Pull in BYTE/WORD/DWORD/HWND etc. that FrameBase's signatures use.
#include "Emulator/StdAfx.h"

#include "Emulator/FrameBase.h"

#include <filesystem>
#include <vector>

namespace dlrl
{

// Minimal FrameBase implementation. The vendored emulator core calls into
// it via GetFrame(); for now most methods are no-ops. The two real jobs:
//   * own the BGRA framebuffer that Video writes into each frame
//   * load ROMs / firmware blobs from the filesystem (Resources/ next to
//     the exe)
class Frame : public FrameBase
{
public:
    Frame();
    ~Frame() override;

    static void SetResourceDir(std::filesystem::path dir);

    void Initialize(bool resetVideoState) override;
    void Destroy(void) override;

    void FrameDrawDiskLEDS() override {}
    void FrameDrawDiskStatus() override {}
    void FrameRefreshStatus(int /*drawflags*/) override {}
    void FrameUpdateApple2Type() override {}
    void FrameSetCursorPosByMousePos() override {}

    bool GetFullScreenShowSubunitStatus() override { return false; }
    void SetFullScreenShowSubunitStatus(bool) override {}
    void SetWindowedModeShowDiskiiStatus(bool) override {}
    bool GetBestDisplayResolutionForFullScreen(UINT&, UINT&, UINT, UINT) override { return false; }
    int  SetViewportScale(int n, bool) override { return n; }
    void SetAltEnterToggleFullScreen(bool) override {}

    void SetLoadedSaveStateFlag(const bool) override {}

    void VideoPresentScreen(void) override {}
    void ResizeWindow(void) override {}

    int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT) override;
    void GetBitmap(WORD id, LONG cb, LPVOID lpvBits) override;

    std::shared_ptr<NetworkBackend> CreateNetworkBackend(const std::string&) override { return nullptr; }
    std::shared_ptr<SoundBuffer> CreateSoundBuffer(uint32_t bufferBytes, uint32_t sampleRate,
                                                   int channels, const char* voiceName) override;

    BYTE* GetResource(WORD id, LPCSTR lpType, uint32_t expectedSize) override;
    void Restart() override {}

    std::string Video_GetScreenShotFolder() const override { return {}; }

private:
    std::vector<BYTE>            m_resourceBuf;  // owns the most-recently-loaded resource
    std::vector<BYTE>            m_framebufferBuf;
    static std::filesystem::path s_resourceDir;
};

} // namespace dlrl
