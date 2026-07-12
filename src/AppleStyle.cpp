#include "AppleStyle.h"

#include <SDL3/SDL_filesystem.h>
#include <imgui.h>

#include <filesystem>

namespace dlrl
{

namespace
{

// Walk up from the executable looking for assets/<name> (CMake
// stages the assets next to the binary; macOS has them in the
// bundle's Resources). Same pattern as FindResourcesDir /
// FindAssetsDir in main.cpp — kept local here because AppleStyle
// runs at Renderer init time, before main has resolved its own
// paths.
std::filesystem::path FindAssetPath(const char* name)
{
    const char* base = SDL_GetBasePath();
    std::filesystem::path dir = base ? base : ".";
    for (int i = 0; i < 5; ++i)
    {
        const auto candidate = dir / "assets" / name;
        if (std::filesystem::exists(candidate)) return candidate;
        // Fall-through to the source-tree layout (running from build-win/).
        const auto srcCandidate = dir / "assets" / "pp" / "assets" / name;
        if (std::filesystem::exists(srcCandidate)) return srcCandidate;
        dir = dir.parent_path();
        if (dir.empty()) break;
    }
    return {};
}

ImFont* g_monoFont = nullptr;

struct Palette
{
    ImVec4 fg;     // primary phosphor (text, highlights)
    ImVec4 dim;    // dim phosphor    (borders, sliders, separators)
    ImVec4 dimer;  // dimmer phosphor (disabled text, hovered fill)
    ImVec4 fill;   // dark "framed" fill (button / header background)
};

constexpr Palette kPalettes[] = {
    // White
    { ImVec4(0.95f, 0.95f, 0.95f, 1.00f),
      ImVec4(0.65f, 0.65f, 0.65f, 1.00f),
      ImVec4(0.40f, 0.40f, 0.40f, 1.00f),
      ImVec4(0.10f, 0.10f, 0.10f, 1.00f) },
    // Green
    { ImVec4(0.20f, 1.00f, 0.40f, 1.00f),
      ImVec4(0.10f, 0.65f, 0.25f, 1.00f),
      ImVec4(0.05f, 0.40f, 0.15f, 1.00f),
      ImVec4(0.02f, 0.12f, 0.05f, 1.00f) },
    // Amber
    { ImVec4(1.00f, 0.69f, 0.20f, 1.00f),
      ImVec4(0.70f, 0.45f, 0.10f, 1.00f),
      ImVec4(0.45f, 0.27f, 0.05f, 1.00f),
      ImVec4(0.18f, 0.10f, 0.02f, 1.00f) },
};

const Palette& PaletteFor(InterfaceColor c)
{
    const int i = (c >= 0 && c <= InterfaceColor_Amber) ? (int)c : (int)InterfaceColor_Green;
    return kPalettes[i];
}

}

void ApplyAppleStyle(InterfaceColor color)
{
    static bool s_fontLoaded = false;
    if (!s_fontLoaded)
    {
        ImGuiIO& io = ImGui::GetIO();
        const auto berkelium  = FindAssetPath("BerkeliumIIHGR.ttf");
        const auto proggyTiny = FindAssetPath("ProggyTiny.ttf");
        if (!berkelium.empty())
        {
            // 16px is a good readable size for Berkelium II HGR — the
            // glyphs are inherently 7×8 so anything smaller blurs even
            // with NEAREST sampling. First-loaded font is the default.
            io.Fonts->Clear();
            io.Fonts->AddFontFromFileTTF(berkelium.string().c_str(), 16.0f);
        }
        if (!proggyTiny.empty())
        {
            // ProggyTiny is a 10px monospace bitmap font — much denser
            // than Berkelium and rasterises crisply at its native size.
            // Used by panels (memory hex viewer) that need columnar
            // alignment + lots of rows on screen.
            g_monoFont = io.Fonts->AddFontFromFileTTF(
                proggyTiny.string().c_str(), 10.0f);
        }
        s_fontLoaded = true;
    }

    const Palette& p = PaletteFor(color);
    constexpr ImVec4 kBlack       = ImVec4(0.00f, 0.00f, 0.00f, 0.94f);
    constexpr ImVec4 kBlackOpaque = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = p.fg;
    c[ImGuiCol_TextDisabled]          = p.dimer;
    c[ImGuiCol_TextSelectedBg]        = p.dim;
    c[ImGuiCol_WindowBg]              = kBlack;
    c[ImGuiCol_ChildBg]               = kBlackOpaque;
    c[ImGuiCol_PopupBg]               = kBlack;
    c[ImGuiCol_Border]                = p.dim;
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = kBlackOpaque;
    c[ImGuiCol_FrameBgHovered]        = p.fill;
    c[ImGuiCol_FrameBgActive]         = p.dimer;
    // Active title bar gets a clearly-visible phosphor accent so the
    // focused window stands out from the unfocused (pure-black) ones.
    c[ImGuiCol_TitleBg]               = kBlackOpaque;
    c[ImGuiCol_TitleBgActive]         = p.dim;
    c[ImGuiCol_TitleBgCollapsed]      = kBlackOpaque;
    c[ImGuiCol_MenuBarBg]             = kBlackOpaque;
    c[ImGuiCol_ScrollbarBg]           = kBlackOpaque;
    c[ImGuiCol_ScrollbarGrab]         = p.dimer;
    c[ImGuiCol_ScrollbarGrabHovered]  = p.dim;
    c[ImGuiCol_ScrollbarGrabActive]   = p.fg;
    c[ImGuiCol_CheckMark]             = p.fg;
    c[ImGuiCol_SliderGrab]            = p.dim;
    c[ImGuiCol_SliderGrabActive]      = p.fg;
    c[ImGuiCol_Button]                = p.fill;
    c[ImGuiCol_ButtonHovered]         = p.dimer;
    c[ImGuiCol_ButtonActive]          = p.dim;
    c[ImGuiCol_Header]                = p.fill;
    c[ImGuiCol_HeaderHovered]         = p.dimer;
    c[ImGuiCol_HeaderActive]          = p.dim;
    c[ImGuiCol_Separator]             = p.dim;
    c[ImGuiCol_SeparatorHovered]      = p.fg;
    c[ImGuiCol_SeparatorActive]       = p.fg;
    c[ImGuiCol_ResizeGrip]            = p.dimer;
    c[ImGuiCol_ResizeGripHovered]     = p.dim;
    c[ImGuiCol_ResizeGripActive]      = p.fg;
    c[ImGuiCol_Tab]                   = p.fill;
    c[ImGuiCol_TabHovered]            = p.dim;
    c[ImGuiCol_TabActive]             = p.dimer;
    c[ImGuiCol_TabUnfocused]          = kBlackOpaque;
    c[ImGuiCol_TabUnfocusedActive]    = p.fill;
    c[ImGuiCol_DockingPreview]        = p.dim;
    c[ImGuiCol_DockingEmptyBg]        = kBlackOpaque;
    c[ImGuiCol_NavHighlight]          = p.fg;

    // Sharp edges, single-pixel borders, snug padding — the Apple //e
    // never had rounded anything.
    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 0.0f;
    s.FrameRounding     = 0.0f;
    s.PopupRounding     = 0.0f;
    s.ScrollbarRounding = 0.0f;
    s.GrabRounding      = 0.0f;
    s.TabRounding       = 0.0f;

    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.FrameBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;
    s.TabBorderSize     = 0.0f;

    s.WindowPadding     = ImVec2(8, 6);
    s.FramePadding      = ImVec2(6, 3);
    s.ItemSpacing       = ImVec2(8, 4);
    s.ItemInnerSpacing  = ImVec2(6, 4);
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 12.0f;
}

ImFont* MonoFont() { return g_monoFont; }

} // namespace dlrl
