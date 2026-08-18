#pragma once

#include "Texture.h"
#include "InventoryState.h"
#include "SpriteFont.h"

#include <filesystem>
#include <array>
#include <cstdint>
#include <map>
#include <vector>
#include <deque>
#include <string>

namespace dlrl
{

struct HookEvent;
class InventoryRules;

enum class MapViewMode
{
    Full = 0,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    FollowPlayer = 99,
};

class ModernUI
{
public:
    bool Initialize(const std::filesystem::path& assetsDir,
                    const std::filesystem::path& portableAssetsDir,
                    const InventoryRules* inventoryRules);
    void Shutdown();
    void UpdateMapTexture(bool inBattle, bool inTransition,
                          bool extraRaceAndClassBonuses);
    void SetFogOfWarPath(const std::filesystem::path& path);
    void SaveFogOfWar();
    void ResetFogOfWar();
    void SeedVisualFixture();
    void SeedBattleFixture();
    void SeedInventoryFixture();
    void HandleEvent(const HookEvent& event);
    void ResetText();
    void Render(unsigned int appleFramebufferTexture, bool showAppleVideo,
                int originalInterfaceOpacity, bool paused,
                MapViewMode mapViewMode, bool englishNames, bool battle, bool inventory,
                bool loading, bool loadingReady, bool gameOver);
    void RenderSpellWindow(bool* open);
    void RenderLogWindow(bool* open);
    void RenderHostHint(const std::string& text, float centerX, float y,
                        float maximumWidth) const;
    bool ConsumeInventoryChanged();

    // Deathlord-styled host chrome: wooden slate panels with charset
    // headings, shared by every host window, dialog, and the menu bar.
    // Same contract as ImGui::Begin — always pair with EndPanel; only add
    // content when it returns true. Popups follow the ImGui popup contract:
    // call EndPanelPopup only when BeginPanelPopup returned true.
    bool BeginPanel(const char* title, bool* open, float defaultWidth,
                    float defaultHeight, int extraWindowFlags = 0);
    void EndPanel();
    bool BeginPanelPopup(const char* title);
    void EndPanelPopup();
    bool PanelButton(const char* label);

private:
    int LosRadius(bool extraRaceAndClassBonuses) const;
    void CalculateLos();
    void DrawPanelChrome(const char* title, bool* open);

    Texture background_;
    Texture backgroundTop_;
    Texture noMap_;
    Texture tilesOverland_;
    Texture tilesDungeon_;
    Texture monsters_;
    Texture animatedElements_;
    Texture autoMapSprites_;
    Texture mapTexture_;
    Texture minimapSprites_;
    Texture daytimeSprites_;
    Texture battleSprites_;
    Texture inventorySprites_;
    Texture spellList_;
    Texture loadingScreen_;
    Texture gameOver_;
    Texture portraitsMale_;
    Texture portraitsFemale_;
    Texture deathlordCharset_;
    SpriteFont appleFont_;
    std::vector<std::uint8_t> mapPixels_;
    std::uint64_t mapSignature_ = 0;
    std::array<bool, 256> sectorsSeen_{};
    // v2 fog of war: per-map seen/footstep markers plus a live line-of-sight
    // pass recomputed whenever the avatar, radius, or map content changes.
    std::filesystem::path fogPath_;
    std::map<std::string, std::vector<std::uint8_t>> fogStore_;
    std::vector<std::uint8_t> fogSeen_ = std::vector<std::uint8_t>(64 * 64, 0);
    std::vector<std::uint8_t> losVisible_ = std::vector<std::uint8_t>(64 * 64, 0);
    std::string fogMapName_;
    std::uint64_t fogEpoch_ = 0;
    std::uint64_t mapContentHash_ = 0;
    int fogAvatarX_ = -1;
    int fogAvatarY_ = -1;
    int losRadius_ = 0;
    bool fogDisabled_ = false;
    // Canvas transform captured each Render so overlays drawn after the
    // canvas (log, spells) share its coordinate space and frame validity.
    float canvasOriginX_ = 0.0f;
    float canvasOriginY_ = 0.0f;
    float canvasScale_ = 1.0f;
    int canvasFrame_ = -1;
    bool panelContentVisible_ = false;
    float logScrollFromBottom_ = 0.0f;
    struct TextLine
    {
        std::string text;
        bool inverse = false;
    };
    std::deque<TextLine> log_;
    std::deque<std::string> longLog_;
    std::array<TextLine, 8> billboard_{};
    std::string module_;
    std::string keypress_;
    int battleEnemyType_ = 0;
    int battleActiveActor_ = -1;
    const InventoryRules* inventoryRules_ = nullptr;
    int inventorySlot_ = 0;
    InventoryState inventoryState_;
    bool inventoryChanged_ = false;
    bool ready_ = false;
};

} // namespace dlrl
