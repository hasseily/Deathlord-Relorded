#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

namespace dlrl
{

std::uint8_t SdlKeyToAscii(SDL_Keycode key, SDL_Keymod mod);
bool IsPauseShortcut(SDL_Keycode key, SDL_Keymod mod);

} // namespace dlrl
