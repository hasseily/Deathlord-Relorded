#include "KeyboardInput.h"

namespace dlrl
{

std::uint8_t SdlKeyToAscii(SDL_Keycode key, SDL_Keymod mod)
{
    // SDL key-down events carry letters plus modifiers, not ASCII control
    // characters. Deathlord expects Ctrl-P (pitch camp) as $10.
    if ((mod & SDL_KMOD_CTRL) && key >= SDLK_A && key <= SDLK_Z)
        return static_cast<std::uint8_t>(key - SDLK_A + 1);

    const bool shift = (mod & SDL_KMOD_SHIFT) != 0;
    switch (key)
    {
    case SDLK_RETURN: case SDLK_KP_ENTER: return 0x0D;
    case SDLK_BACKSPACE: return 0x08;
    case SDLK_TAB: return 0x09;
    case SDLK_ESCAPE: return 0x1B;
    case SDLK_SPACE: return ' ';
    case SDLK_MINUS: return shift ? '_' : '-';
    case SDLK_EQUALS: return shift ? '+' : '=';
    case SDLK_LEFTBRACKET: return shift ? '{' : '[';
    case SDLK_RIGHTBRACKET: return shift ? '}' : ']';
    case SDLK_BACKSLASH: return shift ? '|' : '\\';
    case SDLK_SEMICOLON: return shift ? ':' : ';';
    case SDLK_APOSTROPHE: return shift ? '"' : '\'';
    case SDLK_COMMA: return shift ? '<' : ',';
    case SDLK_PERIOD: return shift ? '>' : '.';
    case SDLK_SLASH: return shift ? '?' : '/';
    default: break;
    }
    if (key >= 'a' && key <= 'z') return shift ? key - 'a' + 'A' : key;
    if (key >= '0' && key <= '9')
    {
        if (shift)
        {
            static const char shifted[] = ")!@#$%^&*(";
            return shifted[key - '0'];
        }
        return key;
    }
    return 0;
}

bool IsPauseShortcut(SDL_Keycode key, SDL_Keymod mod)
{
    return key == SDLK_P && (mod & SDL_KMOD_ALT)
        && !(mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI));
}

} // namespace dlrl
