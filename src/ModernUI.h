#pragma once

#include "Texture.h"
#include "InventoryState.h"
#include "SpriteFont.h"

#include <filesystem>
#include <array>
#include <cstdint>
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
    void UpdateMapTexture();
    void SeedVisualFixture();
    void SeedBattleFixture();
    void SeedInventoryFixture();
    void HandleEvent(const HookEvent& event);
    void ResetText();
    void Render(unsigned int appleFramebufferTexture, bool showAppleVideo, bool paused,
                MapViewMode mapViewMode, bool englishNames, bool battle, bool inventory,
                bool loading, bool gameOver);
    void RenderSpellWindow(bool* open);
    void RenderLogWindow(bool* open);
    bool ConsumeInventoryChanged();

private:
    Texture background_;
    Texture backgroundTop_;
    Texture noMap_;
    Texture tilesOverland_;
    Texture tilesDungeon_;
    Texture monsters_;
    Texture animatedElements_;
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
