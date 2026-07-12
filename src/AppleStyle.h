#pragma once

struct ImFont;

namespace dlrl
{

// Stored in nac.json (key "interface_color"). Order is locked — these
// are written as plain ints.
enum InterfaceColor
{
    InterfaceColor_White = 0,
    InterfaceColor_Green = 1,
    InterfaceColor_Amber = 2,
};

// Configures the active ImGui context to look like an Apple //e text
// terminal: Berkelium II HGR pixel font + chosen phosphor palette +
// zero rounding / single-pixel borders. Pure paint job — no effect on
// layout or behaviour. Safe to call repeatedly; the font is loaded on
// the first call only (subsequent calls just swap the palette so the
// View → Interface Color submenu can re-apply live).
//
// The font lives at <exe-dir>/assets/BerkeliumIIHGR.ttf (staged from
// assets/pp/assets/ by the CMake build). Falls back to ImGui's
// default font if the file isn't found.
void ApplyAppleStyle(InterfaceColor color);

// Secondary smaller monospace font (ProggyTiny). Used by panels that
// want denser, non-decorative text — e.g. the memory hex viewer.
// Returns nullptr if the font wasn't loadable, in which case the
// caller should just render with the default font.
::ImFont* MonoFont();

} // namespace dlrl
