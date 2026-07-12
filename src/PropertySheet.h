#pragma once

#include "Emulator/Configuration/IPropertySheet.h"

namespace dlrl
{

// All-defaults IPropertySheet — every getter returns 0/false, every setter
// is a no-op. The emulator only calls these for advisory configuration
// (joystick autofire / mouse crosshair / etc.); none of them gate boot.
class PropertySheet : public IPropertySheet
{
public:
    void Init(void) override {}
    uint32_t GetVolumeMax(void) override { return 99; }
    bool SaveStateSelectImage(HWND, bool) override { return false; }
    void ResetAllToDefault() override {}
    void ApplyConfigAfterClose(UINT) override {}
    void ApplyNewConfigFromSnapshot() override {}
    void ConfigSaveApple2Type(eApple2Type) override {}

    UINT GetScrollLockToggle(void) override { return 0; }
    void SetScrollLockToggle(UINT) override {}
    UINT GetJoystickCursorControl(void) override { return 0; }
    void SetJoystickCursorControl(UINT) override {}
    UINT GetJoystickCenteringControl(void) override { return 0; }
    void SetJoystickCenteringControl(UINT) override {}
    UINT GetAutofire(UINT) override { return 0; }
    UINT GetAutofire(void) override { return 0; }
    void SetAutofire(UINT) override {}
    bool GetButtonsSwapState(void) override { return false; }
    void SetButtonsSwapState(bool) override {}
    UINT GetMouseShowCrosshair(void) override { return 0; }
    void SetMouseShowCrosshair(UINT) override {}
    UINT GetMouseRestrictToWindow(void) override { return 0; }
    void SetMouseRestrictToWindow(UINT) override {}
    UINT GetTheFreezesF8Rom(void) override { return 0; }
    void SetTheFreezesF8Rom(UINT) override {}
};

} // namespace dlrl
