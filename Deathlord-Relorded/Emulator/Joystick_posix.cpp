// Non-Windows joystick stubs. Upstream Joystick.cpp builds against
// joyGetPos / JOYINFO / JOY_BUTTON* — Win32-only multimedia APIs the
// libwindows shim doesn't cover. NAC's roadmap explicitly defers
// gamepad / joystick support, so on Linux /
// macOS we just make every paddle read float around the centre and
// every button report "up". The CPU's $C064-$C067 / $C070 paths still
// link, they just always see "no joystick".

#include "StdAfx.h"
#include "Joystick.h"

void  JoyInitialize() {}
BOOL  JoyProcessKey(int, bool, bool, bool)                               { return FALSE; }
void  JoyReset() {}
void  JoySetButton(eBUTTON, eBUTTONSTATE) {}
BOOL  JoySetEmulationType(HWND, uint32_t, int, const bool)               { return TRUE; }
void  JoySetPosition(int, int, int, int) {}
BOOL  JoyUsingMouse()                                                    { return FALSE; }
BOOL  JoyUsingKeyboard()                                                 { return FALSE; }
BOOL  JoyUsingKeyboardCursors()                                          { return FALSE; }
BOOL  JoyUsingKeyboardNumpad()                                           { return FALSE; }
void  JoyDisableUsingMouse() {}
void  JoySetJoyType(UINT, uint32_t) {}
uint32_t JoyGetJoyType(UINT)                                             { return 0; }
void  JoySetTrim(short, bool) {}
short JoyGetTrim(bool)                                                   { return 0; }
void  JoyportControl(const UINT) {}
void  JoySetHookAltKeys(bool) {}
void  JoySetButtonVirtualKey(UINT, UINT) {}
int   GetJoystick1(void)                                                 { return 0; }
int   GetJoystick2(void)                                                 { return 0; }
void  JoySaveSnapshot(class YamlSaveHelper&) {}
void  JoyLoadSnapshot(class YamlLoadHelper&, UINT) {}

// $C061..$C063 — button N down? Always report up.
BYTE __stdcall JoyReadButton(WORD, WORD addr, BYTE, BYTE, ULONG)
{
    return 0;
}

// $C064..$C067 — paddle N timed read. Returning 0 means the discharge
// timer has already elapsed, which is the right answer when no joystick
// is connected.
BYTE __stdcall JoyReadPosition(WORD, WORD, BYTE, BYTE, ULONG)
{
    return 0;
}

// $C070 — reset the paddle discharge timers. No-op without paddles.
void JoyResetPosition(ULONG) {}
