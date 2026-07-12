// Non-Windows replacement for the upstream Keyboard.cpp. The upstream
// file is built around Win32-only APIs (GetKeyState, the clipboard
// surface, AltGr WM_CHAR re-posting, the AKD / TK3000 / Pravets clone
// paths) that the libwindows shim doesn't cover and that NAC has no
// use for: keyboard state and clipboard arrive from SDL3 in
// src/main.cpp, and we only emulate the Apple //e Enhanced.
//
// What main.cpp actually calls on us:
//   * KeybQueueKeypress(ascii, ASCII)   — printable + Return/Backspace/Tab/Esc
//   * KeybQueueKeypress(vk,    NOT_ASCII) — VK_LEFT/RIGHT/UP/DOWN/DELETE etc.
//   * KeybSetCapsLock(false) once at boot, KeybToggleCapsLock on Caps press
//   * KeybUpdateCtrlShiftStatus (called every key event — stub here)
// The CPU side calls KeybReadData / KeybReadFlag / KeybClearStrobe via
// the $C000 / $C010 IO handlers in Memory.cpp.

#include "StdAfx.h"
#include "Keyboard.h"

#include <queue>

namespace
{
    bool  g_bCapsLock = true;
    BYTE  keycode     = 0;
    bool  bKeyWasRead = false;
    std::queue<BYTE> keys;

    // Apple //e Enhanced arrow / delete keycodes, indexed by
    // (virtualKey - 0x25 = VK_LEFT). 0 entries are dropped.
    constexpr BYTE kAppleArrow[10] = {
        0x08, 0x0B, 0x15, 0x0A, 0x00,   // LEFT, UP, RIGHT, DOWN, SELECT
        0x00, 0x00, 0x00, 0x00, 0x7F    // PRINT, EXEC, SNAPSHOT, INSERT, DELETE
    };
}

void KeybReset()
{
    keycode = 0;
    bKeyWasRead = false;
    std::queue<BYTE>().swap(keys);
}

void KeybSetCapsLock(bool state)            { g_bCapsLock = state; }
bool KeybGetCapsStatus()                    { return g_bCapsLock; }
void KeybToggleCapsLock()                   { g_bCapsLock = !g_bCapsLock; }
void KeybToggleP8ACapsLock()                { /* Pravets-only — no-op */ }

bool KeybGetShiftStatus()                   { return false; }
bool KeybGetCtrlStatus()                    { return false; }
bool KeybGetAltStatus()                     { return false; }
void KeybUpdateCtrlShiftStatus()            { /* SDL3 path tracks modifiers itself */ }
void KeybSetAltGrSendsWM_CHAR(bool /*s*/)   {}
void ClipboardInitiatePaste()               { /* no clipboard paste on this frontend */ }
void KeybAnyKeyDown(UINT, WPARAM, bool)     {}

BYTE KeybGetKeycode()                       { return keycode; }

void KeybQueueKeypress(WPARAM key, Keystroke_e bASCII)
{
    if (bASCII == ASCII)
    {
        // Caps adjust mirrors the //e Enhanced behaviour of the upstream
        // KeybQueueKeypress ASCII branch; clones / TK3000 paths are gone.
        BYTE ch = (BYTE)key;
        if (g_bCapsLock && ch >= 'a' && ch <= 'z')
            ch -= ('a' - 'A');
        keycode = ch;
    }
    else
    {
        // NOT_ASCII: only arrow keys + DEL map to //e keycodes; ignore
        // anything outside that range so HOME/END/PgUp/PgDn/F-keys don't
        // generate spurious latch hits.
        if (key < 0x25 || key > 0x2E) return;
        BYTE n = kAppleArrow[key - 0x25];
        if (n == 0) return;
        keycode = n;
    }

    keys.push(keycode);
    bKeyWasRead = false;
}

// $C000 read — return the latch byte with the strobe bit (high bit) set
// while a key is waiting.
BYTE KeybReadData()
{
    if (!keys.empty())
    {
        keycode = keys.front();
        bKeyWasRead = true;
        return keycode | 0x80;
    }
    return keycode;
}

// $C010 read — return the AKD-style "any key down" flag combined with
// the current keycode. Mirrors upstream Linux duplicates.
BYTE KeybReadFlag()
{
    BYTE flag = keys.empty() ? 0 : 0x80;
    if (!keys.empty())
    {
        keycode = keys.front();
        keys.pop();
        bKeyWasRead = false;
    }
    return keycode | flag;
}

// $C010 write — clear the strobe bit by popping any queued key.
BYTE KeybClearStrobe()
{
    if (keys.empty()) return 0;
    keycode = keys.front();
    keys.pop();
    bKeyWasRead = false;
    return keycode | 0x80;
}

// Snapshots are NAC-irrelevant (no save-state UI), but the symbols are
// linked from SaveState.cpp so they need to exist.
void KeybSaveSnapshot(class YamlSaveHelper&) {}
void KeybLoadSnapshot(class YamlLoadHelper&, UINT) {}
