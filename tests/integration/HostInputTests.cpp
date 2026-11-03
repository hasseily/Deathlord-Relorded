#include "Frame.h"
#include "KeyboardInput.h"

#include "Emulator/Core.h"
#include "Emulator/Keyboard.h"

#include <cstdio>

int main()
{
    int failures = 0;
    const auto check = [&](bool okay, const char* description) {
        if (!okay) { ++failures; std::fprintf(stderr, "FAIL: %s\n", description); }
    };

    SetApple2Type(A2TYPE_APPLE2EENHANCED);
    for (bool capsLock : {false, true})
    {
        KeybSetCapsLock(capsLock);
        for (SDL_Keymod control : {SDL_KMOD_LCTRL, SDL_KMOD_RCTRL})
        {
            check(!dlrl::IsPauseShortcut(SDLK_P, control), "Ctrl-P belongs to Deathlord");
            for (SDL_Keymod shift : {SDL_KMOD_NONE, SDL_KMOD_LSHIFT})
            {
                KeybReset();
                KeybQueueKeypress(dlrl::SdlKeyToAscii(SDLK_P, control | shift), ASCII);
                check(KeybReadData() == 0x90, "Ctrl-P reaches the Apple II latch as $10 with strobe");
                KeybClearStrobe();
                check((KeybReadData() & 0x80) == 0, "the camp key strobe clears normally");
            }
        }
    }
    check(dlrl::SdlKeyToAscii(SDLK_A, SDL_KMOD_CTRL) == 1, "Ctrl-A is a control character");
    check(dlrl::SdlKeyToAscii(SDLK_Z, SDL_KMOD_CTRL) == 26, "Ctrl-Z is a control character");
    check(dlrl::SdlKeyToAscii(SDLK_P, SDL_KMOD_NONE) == 'p', "plain P remains printable");
    check(dlrl::SdlKeyToAscii(SDLK_P, SDL_KMOD_SHIFT) == 'P', "Shift-P remains printable");
    check(dlrl::SdlKeyToAscii(SDLK_6, SDL_KMOD_SHIFT) == '^', "Shift-6 still climbs out of pits");
    check(dlrl::SdlKeyToAscii(SDLK_ESCAPE, SDL_KMOD_NONE) == 0x1B, "Escape still cancels input");
    check(dlrl::IsPauseShortcut(SDLK_P, SDL_KMOD_LALT), "left Alt-P pauses");
    check(dlrl::IsPauseShortcut(SDLK_P, SDL_KMOD_RALT), "right Alt-P pauses");
    check(!dlrl::IsPauseShortcut(SDLK_P, SDL_KMOD_NONE), "plain P does not pause");
    check(!dlrl::IsPauseShortcut(SDLK_P, SDL_KMOD_GUI), "Command-P does not pause");
    check(!dlrl::IsPauseShortcut(SDLK_P, SDL_KMOD_CTRL | SDL_KMOD_ALT), "AltGr-P does not pause");
    if (!failures) std::puts("Host input passed: Ctrl-P camp latch, Alt-P pause, printable and pit-exit keys");
    return failures ? 1 : 0;
}
