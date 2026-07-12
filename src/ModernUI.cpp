#include "Emulator/StdAfx.h"

#include "ModernUI.h"

#include "DlrlData.h"
#include "DlrlHooks.h"
#include "InventoryRules.h"
#include "Emulator/Memory.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <string>

namespace dlrl
{
namespace
{

using namespace deathlord;

constexpr float CanvasWidth = 1904.0f;
constexpr float CanvasHeight = 1041.0f;
constexpr std::array<float, 6> PartyX = {17, 17, 17, 1570, 1570, 1570};
constexpr std::array<float, 6> PartyY = {386, 606, 826, 386, 606, 826};
constexpr std::array<const char*, 16> ClassNames = {
    "FIGHTER", "PALADIN", "RANGER", "BARBARIAN", "BERZERKER", "SAMURAI",
    "DARK KNIGHT", "THIEF", "ASSASSIN", "NINJA", "MONK", "PRIEST",
    "DRUID", "MAGICIAN", "ILLUSIONIST", "PEASANT"
};
constexpr std::array<const char*, 8> RaceNames = {
    "HUMAN", "ELF", "HALF-ELF", "DWARF", "GNOME", "DARK ELF", "ORC", "HALF-ORC"
};

BYTE Party(std::uint16_t address, int character)
{
    return MemGetMainPtr(address)[character];
}

int PartyWord(std::uint16_t low, std::uint16_t high, int character)
{
    return Party(low, character) | (Party(high, character) << 8);
}

std::string CharacterName(int character)
{
    std::string name;
    const BYTE* source = MemGetMainPtr(static_cast<WORD>(PartyName + character * 9));
    for (int index = 0; index < 9; ++index)
    {
        const BYTE encoded = source[index];
        char decoded = static_cast<char>((encoded & 0x7F) ^ 0x65);
        if (decoded < 32 || decoded > 126) decoded = '?';
        name.push_back(decoded);
        if (encoded & 0x80) break;
    }
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
        name.pop_back();
    return name.empty() ? "UNKNOWN" : name;
}

std::string DeathlordString(std::uint16_t address, int maximumLength)
{
    std::string value;
    const BYTE* source = MemGetMainPtr(address);
    for (int index = 0; index < maximumLength; ++index)
    {
        const BYTE encoded = source[index];
        char decoded = static_cast<char>((encoded & 0x7F) ^ 0x65);
        if (decoded < 32 || decoded > 126) decoded = ' ';
        value.push_back(decoded);
        if (encoded & 0x80) break;
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

ImVec2 Point(const ImVec2& origin, float scale, float x, float y)
{
    return ImVec2(origin.x + x * scale, origin.y + y * scale);
}

// DirectXTK's v2 dlfont-12pt.spritefont advances every glyph by 14 pixels
// (x-offset + source width + x-advance) on a 16-pixel line. Preserve those
// metrics when sampling the original bitmap atlas.
constexpr float DeathlordGlyphWidth = 14.0f;
constexpr float DeathlordGlyphHeight = 16.0f;

float DeathlordTextWidth(const std::string& text, float logicalSize = 16.0f)
{
    return static_cast<float>(text.size()) * DeathlordGlyphWidth
         * (logicalSize / DeathlordGlyphHeight);
}

void AddDeathlordText(ImDrawList* draw, const Texture& charset,
                      const ImVec2& origin, float scale, float x, float y,
                      ImU32 color, const std::string& text,
                      float logicalSize = 16.0f, bool inverse = false)
{
    if (!charset.IsValid() || text.empty()) return;

    const float fontScale = logicalSize / DeathlordGlyphHeight;
    const float glyphWidth = DeathlordGlyphWidth * fontScale;
    const float glyphHeight = DeathlordGlyphHeight * fontScale;
    const ImU32 glyphColor = inverse ? IM_COL32(5, 7, 5, 255) : color;
    if (inverse)
    {
        draw->AddRectFilled(Point(origin, scale, x, y),
                            Point(origin, scale, x + DeathlordTextWidth(text, logicalSize),
                                  y + glyphHeight), color);
    }

    // Deathlord Charset.png is the original 96-glyph game font: character
    // codes $20-$7F, 16 columns by 6 rows, with 16x16 source cells. This is
    // deliberately not converted to ASCII/Unicode: $5B is the game's solid
    // rule glyph, $7B is its diamond, and $7E/$7F are its visible brackets.
    constexpr float atlasWidth = 256.0f;
    constexpr float atlasHeight = 96.0f;
    constexpr float sourceCell = 16.0f;
    constexpr float sourceInsetX = 1.0f;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        unsigned int code = static_cast<unsigned char>(text[index]) & 0x7F;
        if (code < 0x20) code = 0x20;
        const unsigned int glyph = code - 0x20;
        const float column = static_cast<float>(glyph % 16);
        const float row = static_cast<float>(glyph / 16);
        // The PNG stores each 14x16 Deathlord cell in a 16x16 slot with a
        // one-pixel gutter on either side. Crop the gutters instead of
        // squeezing all 16 source pixels into the 14-pixel v2 advance.
        const ImVec2 uv0((column * sourceCell + sourceInsetX) / atlasWidth,
                         (row * sourceCell) / atlasHeight);
        const ImVec2 uv1(((column + 1.0f) * sourceCell - sourceInsetX) / atlasWidth,
                         ((row + 1.0f) * sourceCell) / atlasHeight);
        const float glyphX = x + static_cast<float>(index) * glyphWidth;
        draw->AddImage(static_cast<ImTextureID>(charset.Id()),
                       Point(origin, scale, glyphX, y),
                       Point(origin, scale, glyphX + glyphWidth, y + glyphHeight),
                       uv0, uv1, glyphColor);
    }
}

constexpr float AppleGlyphWidth = 7.0f;
constexpr float AppleGlyphHeight = 16.0f;

float AppleTextWidth(const std::string& text)
{
    return static_cast<float>(text.size()) * AppleGlyphWidth;
}

void AddAppleText(ImDrawList* draw, const SpriteFont& font,
                  const ImVec2& origin, float scale, float x, float y,
                  ImU32 color, const std::string& text)
{
    const Texture& charset = font.Atlas();
    if (!charset.IsValid()) return;
    float penX = 0;
    for (const unsigned char character : text)
    {
        const SpriteFont::Glyph* glyph = font.Find(character);
        if (!glyph) continue;
        penX += glyph->xOffset;
        const float width = static_cast<float>(glyph->right - glyph->left);
        const float height = static_cast<float>(glyph->bottom - glyph->top);
        if (character != ' ' || width > 1 || height > 1)
        {
            const ImVec2 uv0(static_cast<float>(glyph->left) / charset.Width(),
                             static_cast<float>(glyph->top) / charset.Height());
            const ImVec2 uv1(static_cast<float>(glyph->right) / charset.Width(),
                             static_cast<float>(glyph->bottom) / charset.Height());
            draw->AddImage(static_cast<ImTextureID>(charset.Id()),
                           Point(origin, scale, x + penX, y + glyph->yOffset),
                           Point(origin, scale, x + penX + width,
                                 y + glyph->yOffset + height),
                           uv0, uv1, color);
        }
        penX += width + glyph->xAdvance;
    }
}

} // namespace

bool ModernUI::Initialize(const std::filesystem::path& assetsDir,
                          const std::filesystem::path& portableAssetsDir,
                          const InventoryRules* inventoryRules)
{
    Shutdown();
    const bool background = background_.Load(assetsDir / "Background_Relorded.png");
    const bool backgroundTop = backgroundTop_.Load(
        assetsDir / "Background_Relorded_LayerTop.png");
    const bool noMap = noMap_.Load(assetsDir / "Background_NoMap.png");
    const bool overland = tilesOverland_.Load(
        assetsDir / "Tileset_Relorded_Overland.png", true, true);
    const bool dungeon = tilesDungeon_.Load(
        assetsDir / "Tileset_Relorded_Dungeon.png", true, true);
    const bool monsters = monsters_.Load(
        assetsDir / "Tileset_Relorded_Monsters.png", true, true);
    const bool elements = animatedElements_.Load(
        assetsDir / "Tileset_Elements_Animated.png", true, true);
    const bool autoMapSprites = autoMapSprites_.Load(
        assetsDir / "SpriteSheet.png", true, true);
    const bool minimap = minimapSprites_.Load(assetsDir / "MinimapSpriteSheet.png", true);
    const bool daytime = daytimeSprites_.Load(
        assetsDir / "Tileset_Relorded_MoonPhases.png", true);
    const bool battle = battleSprites_.Load(assetsDir / "BattleOverlaySpriteSheet.png", true);
    const bool inventory = inventorySprites_.Load(assetsDir / "InvOverlaySpriteSheet.png", true);
    const bool spells = spellList_.Load(assetsDir / "SpellList.bmp", true);
    const bool loading = loadingScreen_.Load(assetsDir / "DLRL_Loading_Screen.png");
    const bool gameOver = gameOver_.Load(assetsDir / "GameOver.png");
    const bool male = portraitsMale_.Load(assetsDir / "Spritesheet_Portraits_Male.jpg");
    const bool female = portraitsFemale_.Load(assetsDir / "Spritesheet_Portraits_Female.jpg");
    const bool charset = deathlordCharset_.Load(
        assetsDir / "Deathlord Charset.png", true, true);
    (void)portableAssetsDir;
    const bool appleCharset = appleFont_.Load(assetsDir / "a2-12pt.spritefont");
    if (charset)
    {
        // The source atlas predates alpha textures and has an opaque black
        // background. Convert its luminance to coverage so ImGui can tint the
        // authentic white glyphs without drawing black character-cell boxes.
        std::vector<std::uint8_t> pixels = deathlordCharset_.Pixels();
        for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4)
        {
            const std::uint8_t coverage = std::max({pixels[offset], pixels[offset + 1],
                                                    pixels[offset + 2]});
            pixels[offset] = pixels[offset + 1] = pixels[offset + 2] = 255;
            pixels[offset + 3] = coverage;
        }
        deathlordCharset_.Upload(pixels.data(), deathlordCharset_.Width(),
                                 deathlordCharset_.Height(), true);
    }
    ready_ = background && backgroundTop && noMap && overland && dungeon && monsters && elements
          && autoMapSprites
          && minimap && daytime && battle && inventory && spells
          && loading && gameOver && male && female && charset && appleCharset;
    inventoryRules_ = inventoryRules;
    return ready_;
}

void ModernUI::Shutdown()
{
    appleFont_.Reset();
    deathlordCharset_.Reset();
    portraitsFemale_.Reset();
    portraitsMale_.Reset();
    gameOver_.Reset();
    loadingScreen_.Reset();
    battleSprites_.Reset();
    inventorySprites_.Reset();
    spellList_.Reset();
    mapTexture_.Reset();
    daytimeSprites_.Reset();
    minimapSprites_.Reset();
    monsters_.Reset();
    autoMapSprites_.Reset();
    animatedElements_.Reset();
    tilesDungeon_.Reset();
    tilesOverland_.Reset();
    noMap_.Reset();
    background_.Reset();
    backgroundTop_.Reset();
    mapPixels_.clear();
    mapSignature_ = 0;
    sectorsSeen_.fill(false);
    ResetText();
    inventoryRules_ = nullptr;
    ready_ = false;
}

void ModernUI::ResetText()
{
    log_.clear();
    longLog_.clear();
    log_.push_front({std::string(18, ' '), false});
    for (TextLine& line : billboard_) line = {std::string(18, ' '), false};
    module_ = std::string(10, ' ');
    keypress_ = std::string(7, ' ');
    battleEnemyType_ = 0;
    battleActiveActor_ = -1;
    inventorySlot_ = 0;
}

bool ModernUI::ConsumeInventoryChanged()
{
    const bool changed = inventoryChanged_;
    inventoryChanged_ = false;
    return changed;
}

void ModernUI::SeedVisualFixture()
{
    sectorsSeen_.fill(false);
    for (int x = 2; x <= 10; ++x) sectorsSeen_[8 * 16 + x] = true;
    for (int y = 4; y <= 8; ++y) sectorsSeen_[y * 16 + 6] = true;
    sectorsSeen_[7 * 16 + 5] = true;
    sectorsSeen_[7 * 16 + 7] = true;
    log_.clear();
    log_.push_back({"ROAD EAST IS CLEAR", false});
    log_.push_back({"KENJI FOUND 42 GP", false});
    log_.push_back({"A HIDDEN DOOR!", true});
    log_.push_back({"PARTY ENTERS KAWAH", false});
    for (int line = 5; line <= 32; ++line)
    {
        char history[19];
        std::snprintf(history, sizeof(history), "HISTORY LINE %02d", line);
        log_.push_back({history, false});
    }
    billboard_[0] = {"\x7eS\x7f SEARCH", true};
    billboard_[1] = {"\x7e" "C\x7f CAST SPELL", false};
    billboard_[2] = {"\x7eI\x7f INVENTORY", false};
    module_ = "OUTDOORS";
    keypress_ = "\x7eSPACE\x7f";
}

void ModernUI::SeedBattleFixture()
{
    using namespace deathlord;
    battleEnemyType_ = 0x23;
    battleActiveActor_ = 1;
    MemGetMainPtr(BattleEnemyCount)[0] = 7;
    MemGetMainPtr(MonsterCurrentHealthMultiplier)[0] = 9;
    constexpr std::array<BYTE, 7> health = {54, 43, 61, 32, 48, 19, 39};
    constexpr std::array<BYTE, 7> disabled = {0, 0, 2, 0, 0, 1, 0};
    std::copy(health.begin(), health.end(), MemGetMainPtr(BattleEnemyHealth));
    std::copy(disabled.begin(), disabled.end(), MemGetMainPtr(BattleEnemyDisabled));
    constexpr const char* name = "ONI WAR BAND";
    BYTE* destination = MemGetMainPtr(MonsterCurrentName);
    for (std::size_t i = 0; i < std::strlen(name); ++i)
        destination[i] = static_cast<BYTE>((name[i] ^ 0x65) & 0x7F);
    destination[std::strlen(name) - 1] |= 0x80;
    billboard_[3] = {std::string(18, static_cast<char>(0x5B)), false};
}

void ModernUI::SeedInventoryFixture()
{
    using namespace deathlord;
    constexpr std::array<BYTE, PartySize> melee = {0x00, 0x0A, 0x26, 0x46, 0x02, 0x32};
    constexpr std::array<BYTE, PartySize> ranged = {0x12, 0xFF, 0x21, 0xFF, 0x12, 0xFF};
    for (int member = 0; member < PartySize; ++member)
    {
        BYTE* inventory = MemGetMainPtr(static_cast<WORD>(PartyInventory + member * 0x20));
        std::fill_n(inventory, 16, static_cast<BYTE>(0xFF));
        inventory[0] = melee[member];
        inventory[1] = ranged[member];
        inventory[8] = 0xFF;
        inventory[9] = ranged[member] == 0xFF ? 0xFF : 12 + member;
        MemGetMainPtr(PartyWeaponReady)[member] = member < 3 ? 0 : 1;
    }
    inventorySlot_ = 0;
}

void ModernUI::HandleEvent(const HookEvent& event)
{
    using namespace deathlord;
    auto scrollLog = [&]()
    {
        if (!log_.empty()
            && std::any_of(log_.front().text.begin(), log_.front().text.end(),
                           [](char value) { return value != ' '; }))
        {
            longLog_.push_back(log_.front().text);
            if (longLog_.size() > 2000) longLog_.pop_front();
        }
        if (log_.size() >= 32) log_.pop_back();
        log_.push_front({std::string(18, ' '), false});
    };
    auto scrollBillboard = [&]()
    {
        for (int index = 7; index > 0; --index) billboard_[index] = billboard_[index - 1];
        billboard_[0] = {std::string(18, ' '), false};
    };

    switch (event.type)
    {
    case HookEventType::BattleStarted:
        battleActiveActor_ = -1;
        break;
    case HookEventType::BattleEnded:
        battleActiveActor_ = -1;
        break;
    case HookEventType::EnemyType:
        battleEnemyType_ = event.value;
        break;
    case HookEventType::ActiveActor:
        battleActiveActor_ = event.actor;
        break;
    case HookEventType::PrintCharacter:
    {
        const int x = MemGetMainPtr(PrintX)[0];
        const int y = MemGetMainPtr(PrintY)[0];
        const int xOrigin = MemGetMainPtr(PrintXOrigin)[0];
        char character = static_cast<char>(event.value & 0x7F);
        if (static_cast<unsigned char>(character) < 32) character = ' ';
        if (y < 12) break;
        if (xOrigin == 1 && x >= 1 && x <= 18)
        {
            if (log_.empty()) scrollLog();
            log_.front().text.resize(18, ' ');
            log_.front().text[x - 1] = character;
            log_.front().inverse = event.auxiliary != 0;
        }
        else if (y == 13 && x >= 22 && x < 32)
        {
            module_.resize(10, ' ');
            module_[x - 22] = character;
        }
        else if (y == 23 && x >= 26 && x < 33)
        {
            keypress_.resize(7, ' ');
            keypress_[x - 26] = character;
        }
        else if (y >= 15 && y <= 22 && x >= 21 && x < 39)
        {
            TextLine& line = billboard_[22 - y];
            line.text.resize(18, ' ');
            line.text[x - 21] = character;
            line.inverse = event.auxiliary != 0;
        }
        break;
    }
    case HookEventType::ScrollText:
        if (MemGetMainPtr(PrintXOrigin)[0] == 1) scrollLog();
        else scrollBillboard();
        break;
    case HookEventType::ClearText:
        if (event.value == 3)
        {
            scrollLog();
            scrollLog();
            scrollLog();
        }
        else
        {
            for (TextLine& line : billboard_) line = {std::string(18, ' '), false};
        }
        break;
    case HookEventType::InverseTextLine:
        if (event.value >= 0 && event.value < static_cast<int>(billboard_.size()))
            billboard_[event.value].inverse = !billboard_[event.value].inverse;
        break;
    case HookEventType::MissingXp:
        if (log_.size() >= 32) log_.pop_back();
        log_.push_front({"MISSING " + std::to_string(std::max(0, event.value)) + " XP", true});
        break;
    default:
        break;
    }
}

void ModernUI::UpdateMapTexture()
{
    const BYTE* map = MemGetMainPtr(GameMap);
    std::uint64_t signature = 1469598103934665603ull;
    for (std::size_t i = 0; i < 64 * 64; ++i)
    {
        signature ^= map[i];
        signature *= 1099511628211ull;
    }
    const std::array<BYTE, 4> state = {
        MemGetMainPtr(MapType)[0], MemGetMainPtr(MapX)[0], MemGetMainPtr(MapY)[0],
        MemGetMainPtr(PartyCurrentClass)[0]
    };
    for (BYTE value : state)
    {
        signature ^= value;
        signature *= 1099511628211ull;
    }
    for (int index = 0; index < 16; ++index)
    {
        signature ^= MemGetMainPtr(MapMonsterSpriteIds)[index];
        signature *= 1099511628211ull;
    }
    if (signature == mapSignature_ && mapTexture_.IsValid()) return;

    constexpr int tileWidth = 28;
    constexpr int tileHeight = 32;
    constexpr int mapWidth = 64 * tileWidth;
    constexpr int mapHeight = 64 * tileHeight;
    mapPixels_.assign(static_cast<std::size_t>(mapWidth) * mapHeight * 4, 0);

    auto blit = [&](const Texture& sheet, int sprite, int columns, int tileX, int tileY)
    {
        const auto& source = sheet.Pixels();
        if (source.empty()) return;
        const int sourceColumn = sprite % columns;
        const int sourceRow = sprite / columns;
        for (int y = 0; y < tileHeight; ++y)
        {
            for (int x = 0; x < tileWidth; ++x)
            {
                const int sourceX = sourceColumn * tileWidth + x;
                const int sourceY = sourceRow * tileHeight + y;
                const std::size_t sourceOffset =
                    (static_cast<std::size_t>(sourceY) * sheet.Width() + sourceX) * 4;
                const std::size_t destinationOffset =
                    (static_cast<std::size_t>(tileY * tileHeight + y) * mapWidth
                     + tileX * tileWidth + x) * 4;
                if (source[sourceOffset + 3] == 0) continue;
                std::copy_n(source.data() + sourceOffset, 4,
                            mapPixels_.data() + destinationOffset);
            }
        }
    };

    const Texture& environment = MemGetMainPtr(MapType)[0] == 1
                               ? tilesOverland_ : tilesDungeon_;
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            const BYTE id = static_cast<BYTE>(map[x + y * 64] % 0x50);
            if (id < 0x40)
            {
                int elementRow = -1;
                if (MemGetMainPtr(MapType)[0] == 1)
                {
                    if (id == 0x2B) elementRow = 2;
                    else if (id == 0x3C) elementRow = 1;
                }
                else
                {
                    if (id == 0x26) elementRow = 2;
                    else if (id == 0x27) elementRow = 4;
                    else if (id == 0x2C) elementRow = 0;
                    else if (id == 0x2D) elementRow = 1;
                    else if (id == 0x38) elementRow = 3;
                }
                if (elementRow >= 0) blit(animatedElements_, elementRow * 7, 7, x, y);
                else blit(environment, id, 16, x, y);
            }
            else
            {
                const int localMonster = id - 0x40;
                const BYTE monster = MemGetMainPtr(MapMonsterSpriteIds)[localMonster];
                blit(monsters_, monster, 16, x, y);
            }
        }
    }

    const int avatarX = MemGetMainPtr(MapX)[0];
    const int avatarY = MemGetMainPtr(MapY)[0];
    if (avatarX < 64 && avatarY < 64)
        blit(monsters_, MemGetMainPtr(PartyCurrentClass)[0] & 0x0F, 16, avatarX, avatarY);

    // Match the DX12 AutoMap: compose native 28x32 sprites first, then let
    // linear filtering scale the complete 1792x2048 map into 896x1024.
    mapTexture_.Upload(mapPixels_.data(), mapWidth, mapHeight, false);
    mapSignature_ = signature;
}

void ModernUI::Render(unsigned int appleFramebufferTexture, bool showAppleVideo,
                      int originalInterfaceOpacity, bool paused,
                      MapViewMode mapViewMode, bool englishNames, bool battle, bool inventory,
                      bool loading, bool loadingReady, bool gameOver)
{
    if (!ready_) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##DLRLModernCanvas", nullptr, flags);

    const ImVec2 available = ImGui::GetContentRegionAvail();
    // DLRL v2 draws its 1904x1041 background at native size and centers it in
    // the client area. Never enlarge it; only scale down on smaller displays.
    const float scale = std::min({1.0f, available.x / CanvasWidth,
                                  available.y / CanvasHeight});
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 origin(start.x + (available.x - CanvasWidth * scale) * 0.5f,
                        start.y + (available.y - CanvasHeight * scale) * 0.5f);
    ImGui::SetCursorScreenPos(origin);
    ImGui::Image(ImTextureRef(static_cast<ImTextureID>(background_.Id())),
                 ImVec2(CanvasWidth * scale, CanvasHeight * scale));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    auto AddText = [&](ImDrawList*, const ImVec2&, float, float x, float y,
                       ImU32 color, const std::string& text,
                       float logicalSize = 16.0f)
    {
        AddDeathlordText(draw, deathlordCharset_, origin, scale,
                         x, y, color, text, logicalSize);
    };

    // The original parchment is the no-map fallback until the portable
    // automap is ready. The live Apple view can still be overlaid explicitly.
    draw->AddImage(static_cast<ImTextureID>(noMap_.Id()),
                   Point(origin, scale, 361, 10), Point(origin, scale, 1257, 1034));
    ImVec2 mapUv0(0, 0);
    ImVec2 mapUv1(1, 1);
    if (mapViewMode != MapViewMode::Full)
    {
        float left = 0;
        float top = 0;
        switch (mapViewMode)
        {
        case MapViewMode::TopRight: left = 0.5f; break;
        case MapViewMode::BottomLeft: top = 0.5f; break;
        case MapViewMode::BottomRight: left = top = 0.5f; break;
        case MapViewMode::FollowPlayer:
            left = std::clamp((MemGetMainPtr(MapX)[0] + 0.5f) / 64.0f - 0.25f,
                              0.0f, 0.5f);
            top = std::clamp((MemGetMainPtr(MapY)[0] + 0.5f) / 64.0f - 0.25f,
                             0.0f, 0.5f);
            break;
        default: break;
        }
        mapUv0 = ImVec2(left, top);
        mapUv1 = ImVec2(left + 0.5f, top + 0.5f);
    }
    draw->AddImage(static_cast<ImTextureID>(mapTexture_.Id()),
                   Point(origin, scale, 361, 10), Point(origin, scale, 1257, 1034),
                   mapUv0, mapUv1);

    const int avatarX = MemGetMainPtr(MapX)[0];
    const int avatarY = MemGetMainPtr(MapY)[0];
    if (avatarX < 64 && avatarY < 64)
    {
        const float uvWidth = mapUv1.x - mapUv0.x;
        const float uvHeight = mapUv1.y - mapUv0.y;
        const float tileWidth = 14.0f / uvWidth;
        const float tileHeight = 16.0f / uvHeight;
        const float avatarScreenX = 361 + ((avatarX / 64.0f - mapUv0.x) / uvWidth) * 896;
        const float avatarScreenY = 10 + ((avatarY / 64.0f - mapUv0.y) / uvHeight) * 1024;
        // v2 used the four-corner cursor at the bottom of SpriteSheet.png,
        // breathing through a ten-step two-second pulse around the avatar.
        constexpr std::array<float, 10> pulse = {
            1.13f, 1.10f, 1.05f, 1.00f, 1.00f,
            1.00f, 1.00f, 1.05f, 1.10f, 1.13f};
        const int pulseIndex = static_cast<int>(ImGui::GetTime() * 5.0)
                             % static_cast<int>(pulse.size());
        const float cursorWidth = tileWidth * (32.0f / 28.0f) * pulse[pulseIndex];
        const float cursorHeight = tileHeight * (36.0f / 32.0f) * pulse[pulseIndex];
        const float avatarCenterX = avatarScreenX + tileWidth * 0.5f;
        const float avatarCenterY = avatarScreenY + tileHeight * 0.5f;
        draw->AddImage(static_cast<ImTextureID>(autoMapSprites_.Id()),
                       Point(origin, scale, avatarCenterX - cursorWidth * 0.5f,
                             avatarCenterY - cursorHeight * 0.5f),
                       Point(origin, scale, avatarCenterX + cursorWidth * 0.5f,
                             avatarCenterY + cursorHeight * 0.5f),
                       ImVec2(0.0f, 288.0f / 338.0f),
                       ImVec2(32.0f / 112.0f, 324.0f / 338.0f));
    }

    // Day/time display: the legacy sprite sheet supplies moon phases and the
    // clock hand, with the original Deathlord bitmap font for the label.
    const int hour = MemGetMainPtr(DayHour)[0] % 24;
    const BYTE minuteBcd = MemGetMainPtr(DayMinute)[0];
    const int minute = std::min(59, ((minuteBcd >> 4) * 10) + (minuteBcd & 0x0F));
    const bool daytime = hour >= 6 && hour < 18;
    const int hour12 = hour % 12 == 0 ? 12 : hour % 12;
    char timeBuffer[32];
    std::snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d %s",
                  hour12, minute, hour < 12 ? "AM" : "PM");
    AddText(draw, origin, scale, 133, 208,
            daytime ? IM_COL32(255, 150, 30, 255) : IM_COL32(70, 150, 255, 255),
            timeBuffer, 18);

    const int moonPhase = std::clamp<int>(MemGetMainPtr(DayOfMonth)[0] / 4, 0, 6);
    const float moonU0 = moonPhase * 28.0f / daytimeSprites_.Width();
    const float moonU1 = (moonPhase + 1) * 28.0f / daytimeSprites_.Width();
    draw->AddImage(static_cast<ImTextureID>(daytimeSprites_.Id()),
                   Point(origin, scale, 162, 260), Point(origin, scale, 190, 292),
                   ImVec2(moonU0, 0),
                   ImVec2(moonU1, 32.0f / daytimeSprites_.Height()));

    const float angle = 6.28318530718f * (hour * 60 + minute) / 1440.0f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    auto handPoint = [&](float x, float y)
    {
        const float rotatedX = x * cosine - y * sine;
        const float rotatedY = x * sine + y * cosine;
        return Point(origin, scale, 177 + rotatedX, 244 + rotatedY);
    };
    draw->AddImageQuad(static_cast<ImTextureID>(daytimeSprites_.Id()),
                       handPoint(-14, -16), handPoint(14, -16),
                       handPoint(14, 59), handPoint(-14, 59),
                       ImVec2(0, 64.0f / daytimeSprites_.Height()),
                       ImVec2(28.0f / daytimeSprites_.Width(), 64.0f / daytimeSprites_.Height()),
                       ImVec2(28.0f / daytimeSprites_.Width(), 139.0f / daytimeSprites_.Height()),
                       ImVec2(0, 139.0f / daytimeSprites_.Height()));

    // World minimap fog and current-sector pin.
    const int sectorX = MemGetMainPtr(MapOverlandX)[0];
    const int sectorY = MemGetMainPtr(MapOverlandY)[0];
    if (sectorX < 16 && sectorY < 16)
    {
        sectorsSeen_[sectorY * 16 + sectorX] = true;
        auto seen = [&](int x, int y)
        {
            return x >= 0 && x < 16 && y >= 0 && y < 16 && sectorsSeen_[y * 16 + x];
        };
        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 16; ++x)
            {
                if (seen(x, y)) continue;
                int neighbors = 0;
                if (x == 0 || seen(x - 1, y)) neighbors |= 0b1000;
                if (x == 15 || seen(x + 1, y)) neighbors |= 0b0100;
                if (y == 0 || seen(x, y - 1)) neighbors |= 0b0010;
                if (y == 15 || seen(x, y + 1)) neighbors |= 0b0001;
                const int mask = neighbors ^ 0b1111;
                const ImVec2 uv0(mask * 18.0f / minimapSprites_.Width(), 0);
                const ImVec2 uv1((mask + 1) * 18.0f / minimapSprites_.Width(),
                                 20.0f / minimapSprites_.Height());
                draw->AddImage(static_cast<ImTextureID>(minimapSprites_.Id()),
                               Point(origin, scale, 1585 + x * 18, 27 + y * 20),
                               Point(origin, scale, 1603 + x * 18, 47 + y * 20),
                               uv0, uv1);
            }
        }
        const ImVec2 pinUv0(0, 32.0f / minimapSprites_.Height());
        const ImVec2 pinUv1(28.0f / minimapSprites_.Width(), 1.0f);
        draw->AddImage(static_cast<ImTextureID>(minimapSprites_.Id()),
                       Point(origin, scale, 1582 + sectorX * 18, 16 + sectorY * 20),
                       Point(origin, scale, 1610 + sectorX * 18, 48 + sectorY * 20),
                       pinUv0, pinUv1);
    }

    auto trimmed = [](std::string value)
    {
        while (!value.empty() && value.back() == ' ') value.pop_back();
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());
        return value;
    };
    auto centeredText = [&](float centerX, float y, const std::string& value,
                            ImU32 color, float logicalSize)
    {
        if (value.empty()) return;
        AddDeathlordText(draw, deathlordCharset_, origin, scale,
                         centerX - DeathlordTextWidth(value, logicalSize) * 0.5f,
                         y, color, value, logicalSize);
    };
    const float panelFontScale = scale >= 0.9f ? 1.0f : scale;
    auto PanelText = [&](float x, float y, ImU32 color,
                         const std::string& value, bool inverse = false)
    {
        if (value.empty()) return;
        const ImVec2 anchor(std::floor(origin.x + x * scale),
                            std::floor(origin.y + y * scale));
        AddDeathlordText(draw, deathlordCharset_, anchor, panelFontScale,
                         0, 0, color, value, 16.0f, inverse);
    };
    auto PanelCenteredText = [&](float centerX, float y, ImU32 color,
                                 const std::string& value, bool inverse = false)
    {
        if (value.empty()) return;
        const float width = DeathlordTextWidth(value) * panelFontScale;
        const ImVec2 anchor(
            std::floor(origin.x + centerX * scale - width * 0.5f),
            std::floor(origin.y + y * scale));
        AddDeathlordText(draw, deathlordCharset_, anchor, panelFontScale,
                         0, 0, color, value, 16.0f, inverse);
    };
    const ImU32 textColor = IM_COL32(225, 228, 215, 235);
    const ImU32 inverseColor = IM_COL32(255, 220, 55, 255);
    const std::string partyName = DeathlordString(PartyPartyName, 16);
    PanelCenteredText(1415, 37, textColor, partyName);
    std::string module = trimmed(module_);
    // Deathlord's terminal character in these location labels takes a
    // different print path from the ordinary character hook. Preserve v2's
    // complete labels instead of displaying the captured prefix.
    if (module == "OUTDOO") module = "OUTDOOR";
    else if (module == "INDOO") module = "INDOOR";
    else if (module == "DUNGEO") module = "DUNGEON";
    PanelCenteredText(1415, 59, textColor, module);
    const std::string keypress = trimmed(keypress_);
    const bool keypressInverse = (static_cast<int>(ImGui::GetTime() * 2.0) & 1) == 0;
    PanelCenteredText(1412, 815, inverseColor, keypress, keypressInverse);

    int lineIndex = 0;
    for (const TextLine& line : log_)
    {
        if (lineIndex >= 32) break;
        const std::string value = trimmed(line.text);
        if (!value.empty())
            PanelText(1298, 764 - lineIndex * 18,
                      line.inverse ? inverseColor : textColor,
                      value, line.inverse);
        ++lineIndex;
    }
    for (int index = 0; index < static_cast<int>(billboard_.size()); ++index)
    {
        const std::string value = trimmed(billboard_[index].text);
        if (!value.empty())
            PanelCenteredText(1412, 994 - index * 18,
                              billboard_[index].inverse ? inverseColor : textColor,
                              value, billboard_[index].inverse);
    }
    const int partyCount = std::clamp<int>(MemGetMainPtr(PartySizeAddress)[0], 0, PartySize);
    const int current = MemGetMainPtr(PartyCurrentCharacter)[0];
    const ImU32 normal = IM_COL32(220, 225, 215, 235);
    const ImU32 dim = IM_COL32(165, 175, 165, 230);
    const ImU32 active = IM_COL32(255, 220, 40, 255);
    const ImU32 warning = IM_COL32(255, 105, 45, 255);
    // Near native canvas size, keep card glyphs at a true 1x pixel grid.
    // Scaling 16 source rows to ~15 output rows was dropping the name's top
    // row or the power line's bottom row depending on its fractional anchor.
    const float partyFontScale = scale >= 0.9f ? 1.0f : scale;
    auto PartyText = [&](float x, float y, ImU32 color,
                         const std::string& value, float logicalSize)
    {
        const ImVec2 anchor(std::floor(origin.x + x * scale),
                            std::floor(origin.y + y * scale));
        AddDeathlordText(draw, deathlordCharset_, anchor, partyFontScale,
                         0, 0, color, value, logicalSize);
    };
    auto PartyCenteredText = [&](float centerX, float y, ImU32 color,
                                 const std::string& value, float logicalSize)
    {
        const float width = DeathlordTextWidth(value, logicalSize) * partyFontScale;
        const ImVec2 anchor(
            std::floor(origin.x + centerX * scale - width * 0.5f),
            std::floor(origin.y + y * scale));
        AddDeathlordText(draw, deathlordCharset_, anchor, partyFontScale,
                         0, 0, color, value, logicalSize);
    };
    for (int member = 0; member < partyCount; ++member)
    {
        const float x = PartyX[member];
        const float y = PartyY[member];
        const Texture& portraits = Party(PartyGender, member) == 0
                                 ? portraitsMale_ : portraitsFemale_;
        const float u0 = Party(PartyRace, member) * 92.0f / portraits.Width();
        const float v0 = Party(PartyClass, member) * 121.0f / portraits.Height();
        const float u1 = u0 + 92.0f / portraits.Width();
        const float v1 = v0 + 121.0f / portraits.Height();
        draw->AddImage(static_cast<ImTextureID>(portraits.Id()),
                       Point(origin, scale, x + 2, y + 2),
                       Point(origin, scale, x + 94, y + 123),
                       ImVec2(u0, v0), ImVec2(u1, v1));

        const BYTE status = Party(PartyStatus, member);
        constexpr std::array<std::pair<BYTE, const char*>, 7> statusLabels = {{
            {0x02, "STV"}, {0x04, "TOX"}, {0x08, "ILL"}, {0x10, "PAR"},
            {0x20, "STN"}, {0x40, "RIP"}, {0x80, "ASHES"}
        }};
        // Portrait status labels remain at the atlas's exact 14x16 native
        // pixel size at every canvas scale. Anchor them from the scaled
        // portrait's lower-right corner, then add the v2-style readable
        // one-pixel shadow requested for arbitrary portrait artwork.
        float statusScreenY = std::floor(
            origin.y + (y + 123.0f) * scale - 4.0f * scale
            - DeathlordGlyphHeight);
        for (const auto& [mask, label] : statusLabels)
        {
            if ((status & mask) == 0) continue;
            const std::string value = label;
            const float statusScreenX = std::floor(
                origin.x + (x + 94.0f) * scale - 4.0f * scale
                - DeathlordTextWidth(value));
            const ImVec2 anchor(statusScreenX, statusScreenY);
            AddDeathlordText(draw, deathlordCharset_,
                             ImVec2(anchor.x + 1.0f, anchor.y + 1.0f), 1.0f,
                             0, 0, IM_COL32(0, 0, 0, 255), value);
            AddDeathlordText(draw, deathlordCharset_, anchor, 1.0f, 0, 0,
                             mask >= 0x10 ? IM_COL32(255, 70, 40, 255)
                                          : IM_COL32(255, 225, 40, 255),
                             value);
            statusScreenY -= 17.0f;
        }

        const ImU32 memberColor = member == current ? active : normal;
        PartyText(x + 100, y + 5, memberColor, CharacterName(member), 16);
        char buffer[128];
        const int levelPlus = Party(PartyLevelPlus, member);
        if (levelPlus)
            std::snprintf(buffer, sizeof(buffer), "%02d+%d", Party(PartyLevel, member),
                          levelPlus);
        else
            std::snprintf(buffer, sizeof(buffer), "  %02d ", Party(PartyLevel, member));
        PartyText(x + 262, y + 5, levelPlus ? warning : normal, buffer, 16);
        std::snprintf(buffer, sizeof(buffer), "H %04d/%04d",
                      PartyWord(PartyHealthLow, PartyHealthHigh, member),
                      PartyWord(PartyHealthMaxLow, PartyHealthMaxHigh, member));
        PartyText(x + 100, y + 27, normal, buffer, 16);
        std::snprintf(buffer, sizeof(buffer), "P %03d/%03d",
                      Party(PartyPower, member), Party(PartyPowerMax, member));
        PartyText(x + 100, y + 49, normal, buffer, 16);
        std::snprintf(buffer, sizeof(buffer), "G %05d",
                      PartyWord(PartyGoldLow, PartyGoldHigh, member));
        PartyText(x + 100, y + 71, normal, buffer, 16);
        std::snprintf(buffer, sizeof(buffer), "AC%+03d",
                      10 - Party(PartyArmorClass, member));
        PartyText(x + 248, y + 71, normal, buffer, 16);
        std::snprintf(buffer, sizeof(buffer), "F %03d", Party(PartyFood, member));
        PartyText(x + 100, y + 93,
                  Party(PartyFood, member) < 20 ? warning : normal, buffer, 16);
        std::snprintf(buffer, sizeof(buffer), "T %02d", Party(PartyTorches, member));
        PartyText(x + 262, y + 93, normal, buffer, 16);
        const int characterClass = Party(PartyClass, member) & 0x0F;
        const int race = Party(PartyRace, member) & 0x07;
        PartyCenteredText(x + 48, y + 129, normal, ClassNames[characterClass], 8);
        PartyCenteredText(x + 48, y + 139, normal, RaceNames[race], 8);
        std::snprintf(buffer, sizeof(buffer), "STR:%02d  INT:%02d",
                      Party(PartyStrength, member), Party(PartyIntelligence, member));
        PartyText(x + 2, y + 151, dim, buffer, 8);
        std::snprintf(buffer, sizeof(buffer), "CON:%02d  DEX:%02d",
                      Party(PartyConstitution, member), Party(PartyDexterity, member));
        PartyText(x + 2, y + 167, dim, buffer, 8);
        std::snprintf(buffer, sizeof(buffer), "SIZ:%02d  CHA:%02d",
                      Party(PartySizeAttribute, member), Party(PartyCharisma, member));
        PartyText(x + 2, y + 183, dim, buffer, 8);

        for (int slot = 0; slot < 8; ++slot)
        {
            const BYTE* inventory = MemGetMainPtr(
                static_cast<WORD>(PartyInventory + member * 0x20));
            const BYTE item = inventory[slot];
            const BYTE charges = inventory[slot + 8];
            std::string count = "  ";
            if (item != 0xFF && charges == 0) count = "**";
            else if (item != 0xFF && charges != 0xFF)
            {
                char countBuffer[8];
                std::snprintf(countBuffer, sizeof(countBuffer), "%02d", charges);
                count = countBuffer;
            }
            std::string itemName = item == 0xFF
                                 ? std::string(13, '.')
                                 : inventoryRules_
                                     ? inventoryRules_->Name(item, englishNames)
                                     : "Unknown";
            if (itemName.size() > 13) itemName.resize(13);
            itemName.resize(13, '.');
            const float itemY = y + 120 + slot * 9;
            PartyText(x + 138, itemY, normal, count + " " + itemName, 8);
            std::string equipment;
            ImU32 equipmentColor = dim;
            if (item == 0xFF)
            {
                equipment.clear();
            }
            else if (slot < 2 && Party(PartyWeaponReady, member) == slot)
            {
                equipment = "IN HANDS";
                equipmentColor = active;
            }
            else if (inventoryRules_
                     && inventoryRules_->CanEquip(item, characterClass, race))
            {
                equipment = slot < 2 ? "SHEATHED" : "EQUIPPED";
                equipmentColor = IM_COL32(90, 220, 100, 255);
            }
            else
            {
                equipment = "UNUSABLE";
            }
            PartyText(x + 258, itemY, equipmentColor, equipment, 8);
        }
    }

    draw->AddImage(static_cast<ImTextureID>(backgroundTop_.Id()), origin,
                   Point(origin, scale, CanvasWidth, CanvasHeight));
    for (int member = 0; member < partyCount; ++member)
    {
        const ImU32 memberColor = member == current ? active : normal;
        PartyText(PartyX[member] + 1, PartyY[member] + 2,
                  memberColor, std::to_string(member + 1), 16);
    }

    if (battle && !loading && !gameOver)
    {
        constexpr float left = 652;
        constexpr float top = 220;
        constexpr float width = 600;
        constexpr float height = 600;
        draw->AddRectFilled(origin, Point(origin, scale, CanvasWidth, CanvasHeight),
                            IM_COL32(0, 0, 0, 80));
        draw->AddRectFilled(Point(origin, scale, left, top),
                            Point(origin, scale, left + width, top + height),
                            IM_COL32(5, 7, 5, 245));
        draw->AddRect(Point(origin, scale, left, top),
                      Point(origin, scale, left + width, top + height),
                      IM_COL32(245, 170, 35, 255), 0, 0, std::max(2.0f, 4.0f * scale));
        draw->AddLine(Point(origin, scale, left + 12, top + 317),
                      Point(origin, scale, left + width - 12, top + 317),
                      IM_COL32(245, 170, 35, 255), std::max(2.0f, 4.0f * scale));

        const std::string monsterName = DeathlordString(MonsterCurrentName, 20);
        AddDeathlordText(draw, deathlordCharset_, origin, scale,
                         CanvasWidth * 0.5f - DeathlordTextWidth(monsterName) * 0.5f,
                         top + 18, IM_COL32(255, 220, 55, 255), monsterName);
        centeredText(CanvasWidth * 0.5f, top + 555, "BATTLE",
                     IM_COL32(225, 228, 215, 230), 14);

        constexpr std::array<int, 13> battleX = {
            286, 240, 330, 286, 190, 385, 286, 246, 330, 190, 247, 320, 385
        };
        constexpr std::array<int, 13> battleY = {
            379, 385, 382, 463, 505, 508, 318, 277, 282, 238, 234, 236, 239
        };
        auto sprite = [&](int actor, int spriteId, int health, int maximum,
                          int power, int powerMaximum, int disabled)
        {
            const float x = left + battleX[actor];
            const float y = top + battleY[actor];
            const float u0 = (spriteId % 16) * 28.0f / monsters_.Width();
            const float v0 = (spriteId / 16) * 32.0f / monsters_.Height();
            const float u1 = u0 + 28.0f / monsters_.Width();
            const float v1 = v0 + 32.0f / monsters_.Height();
            const ImU32 tint = disabled ? IM_COL32(100, 100, 100, 255)
                                        : IM_COL32(255, 255, 255, 255);
            draw->AddImage(static_cast<ImTextureID>(monsters_.Id()),
                           Point(origin, scale, x, y), Point(origin, scale, x + 28, y + 32),
                           ImVec2(u0, v0), ImVec2(u1, v1), tint);
            if (actor == battleActiveActor_)
                draw->AddRect(Point(origin, scale, x - 2, y - 2),
                              Point(origin, scale, x + 30, y + 34),
                              actor < PartySize ? IM_COL32(255, 235, 40, 255)
                                                : IM_COL32(255, 65, 35, 255),
                              0, 0, std::max(1.0f, 2.0f * scale));
            const float healthFraction = maximum > 0
                                       ? std::clamp(static_cast<float>(health) / maximum, 0.0f, 1.0f)
                                       : 0.0f;
            const float redV0 = 0.0f;
            const float redV1 = 5.0f / battleSprites_.Height();
            draw->AddImage(static_cast<ImTextureID>(battleSprites_.Id()),
                           Point(origin, scale, x, y + 34),
                           Point(origin, scale, x + 28 * healthFraction, y + 39),
                           ImVec2(0, redV0), ImVec2(28 * healthFraction / battleSprites_.Width(), redV1));
            if (powerMaximum > 0)
            {
                const float powerFraction = std::clamp(
                    static_cast<float>(power) / powerMaximum, 0.0f, 1.0f);
                draw->AddImage(static_cast<ImTextureID>(battleSprites_.Id()),
                               Point(origin, scale, x, y + 41),
                               Point(origin, scale, x + 28 * powerFraction, y + 46),
                               ImVec2(0, 5.0f / battleSprites_.Height()),
                               ImVec2(28 * powerFraction / battleSprites_.Width(),
                                      10.0f / battleSprites_.Height()));
            }
            if (disabled > 0)
                AddText(draw, origin, scale, x + 9, y + 7,
                        IM_COL32(210, 210, 210, 255), std::to_string(disabled), 12);
        };
        for (int member = 0; member < partyCount; ++member)
            sprite(member, Party(PartyClass, member),
                   PartyWord(PartyHealthLow, PartyHealthHigh, member),
                   PartyWord(PartyHealthMaxLow, PartyHealthMaxHigh, member),
                   Party(PartyPower, member), Party(PartyPowerMax, member), 0);
        const int enemyCount = std::clamp<int>(MemGetMainPtr(BattleEnemyCount)[0], 0, 7);
        const int enemyMaximum = std::max(1, MemGetMainPtr(MonsterCurrentHealthMultiplier)[0] * 7);
        for (int enemy = 0; enemy < enemyCount; ++enemy)
            sprite(enemy + PartySize, battleEnemyType_,
                   MemGetMainPtr(BattleEnemyHealth)[enemy], enemyMaximum, 0, 0,
                   MemGetMainPtr(BattleEnemyDisabled)[enemy]);
    }

    if (inventory && !loading && !gameOver)
    {
        // The inventory is a self-contained 1150x500 v2 overlay. Scaling it
        // with the full 1904-wide world canvas reduced it to 0.945x even on a
        // fullscreen 1800-wide Mac, visibly damaging the 7-pixel font. Keep
        // the overlay pixel-native whenever it fits; only shrink it in a
        // genuinely smaller work area.
        const float scale = std::min({1.0f, available.x / 1150.0f,
                                      available.y / 500.0f});
        const ImVec2 center(start.x + available.x * 0.5f,
                            start.y + available.y * 0.5f);
        constexpr float left = 377;
        constexpr float top = 270;
        constexpr float width = 1150;
        constexpr float height = 500;
        // v2 stores the centered overlay in an integer RECT. Centering through
        // the odd-height 1041 canvas instead left this even-height overlay on
        // a half pixel, turning solid SpriteFont rows into filtered 25/100/25
        // coverage. Anchor the overlay rectangle itself to framebuffer pixels.
        const float overlayLeft = std::floor(center.x - width * scale * 0.5f);
        const float overlayTop = std::floor(center.y - height * scale * 0.5f);
        const ImVec2 origin(overlayLeft - left * scale,
                            overlayTop - top * scale);
        constexpr float borderPadding = 20;
        constexpr float innerLeft = left + borderPadding;
        constexpr float innerTop = top + borderPadding;
        constexpr float innerRight = left + width - borderPadding;
        constexpr float innerBottom = top + height - borderPadding;
        constexpr float tabsY = innerTop + 90;
        constexpr float partyColumnsX = innerLeft + 515;
        constexpr float memberColumnWidth = 84;
        constexpr float stashColumnX = innerRight - memberColumnWidth;
        constexpr float rowStartY = tabsY + 74;
        constexpr float rowAdvance = 30;
        constexpr ImU32 amber = IM_COL32(128, 51, 0, 255);
        constexpr ImU32 amberDark = IM_COL32(64, 26, 0, 255);
        constexpr ImU32 white = IM_COL32(255, 255, 255, 255);
        constexpr std::array<const char*, 8> slotNames = {
            "MELEE", "RANGED", "CHEST", "SHIELD",
            "MISC", "JEWELRY", "TOOL", "SCROLL"
        };

        struct InventoryRow
        {
            BYTE item = 0xFF;
            BYTE charges = 0xFF;
            int owner = -1; // 0-5 party, 6-7 stash positions
        };
        std::vector<InventoryRow> rows;
        auto rebuildRows = [&]()
        {
            rows.clear();
            for (int member = 0; member < partyCount; ++member)
            {
                const BYTE* item = MemGetMainPtr(static_cast<WORD>(
                    PartyInventory + member * 0x20 + inventorySlot_));
                if (item[0] != 0xFF) rows.push_back({item[0], item[8], member});
            }
            for (int stashIndex = 0; stashIndex < 2; ++stashIndex)
            {
                const InventoryState::StoredItem& stored =
                    inventoryState_.Stashed(inventorySlot_, stashIndex);
                if (stored.item != 0xFF)
                    rows.push_back({stored.item, stored.charges, PartySize + stashIndex});
            }
            std::stable_sort(rows.begin(), rows.end(), [](const InventoryRow& a,
                                                          const InventoryRow& b)
            {
                return a.item == b.item ? a.charges < b.charges : a.item < b.item;
            });
        };
        rebuildRows();

        const ImVec2 mouseScreen = ImGui::GetIO().MousePos;
        const ImVec2 mouseLogical((mouseScreen.x - origin.x) / scale,
                                  (mouseScreen.y - origin.y) / scale);
        auto contains = [&](float x, float y, float w, float h)
        {
            return mouseLogical.x >= x && mouseLogical.x < x + w
                && mouseLogical.y >= y && mouseLogical.y < y + h;
        };
        const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        draw->AddRectFilled(origin, Point(origin, scale, CanvasWidth, CanvasHeight),
                            IM_COL32(0, 0, 0, 51));
        draw->AddRectFilled(Point(origin, scale, left, top),
                            Point(origin, scale, left + width, top + height),
                            IM_COL32(0, 0, 0, 255));
        draw->AddRect(Point(origin, scale, left, top),
                      Point(origin, scale, left + width, top + height),
                      amber, 0, 0, std::max(1.0f, 3.0f * scale));

        float tabX = innerLeft;
        for (int slot = 0; slot < static_cast<int>(slotNames.size()); ++slot)
        {
            const float tabWidth = 20.0f + AppleTextWidth(slotNames[slot]);
            const bool hovered = contains(tabX, tabsY - 10, tabWidth + 3, 33);
            if (slot == inventorySlot_ || hovered)
            {
                const ImU32 color = slot == inventorySlot_ ? amber : amberDark;
                draw->AddRectFilled(Point(origin, scale, tabX, tabsY - 10),
                                    Point(origin, scale, tabX + tabWidth + 3, tabsY + 23),
                                    color);
                draw->AddRectFilled(Point(origin, scale, tabX + 3, tabsY - 7),
                                    Point(origin, scale, tabX + tabWidth, tabsY + 20),
                                    IM_COL32(0, 0, 0, 255));
            }
            AddAppleText(draw, appleFont_, origin, scale,
                         tabX + 10, tabsY, white, slotNames[slot]);
            if (hovered && clicked)
            {
                inventorySlot_ = slot;
                rebuildRows();
            }
            tabX += tabWidth;
        }
        draw->AddRectFilled(Point(origin, scale, innerLeft, tabsY + 20),
                            Point(origin, scale, tabX, tabsY + 23), amber);
        draw->AddRectFilled(Point(origin, scale, innerLeft, tabsY + 60),
                            Point(origin, scale, innerRight, tabsY + 63), amber);
        draw->AddRectFilled(Point(origin, scale, stashColumnX, innerTop),
                            Point(origin, scale, stashColumnX + 3, innerBottom), amber);

        const char* headers = inventorySlot_ < 2
            ? "Name                  TH0    Damage    AC   Special"
            : "Name                   TH0   AC   Special";
        AddAppleText(draw, appleFont_, origin, scale,
                     innerLeft, tabsY + 40, white, headers);

        for (int member = 0; member < partyCount; ++member)
        {
            const float x = partyColumnsX + member * memberColumnWidth;
            const int characterClass = Party(PartyClass, member) & 0x0F;
            const float u0 = (characterClass % 8) * 28.0f / inventorySprites_.Width();
            const float v0 = (characterClass / 8) * 32.0f / inventorySprites_.Height();
            draw->AddImage(static_cast<ImTextureID>(inventorySprites_.Id()),
                           Point(origin, scale, x + 28, innerTop),
                           Point(origin, scale, x + 56, innerTop + 32),
                           ImVec2(u0, v0),
                           ImVec2(u0 + 28.0f / inventorySprites_.Width(),
                                  v0 + 32.0f / inventorySprites_.Height()));
            auto centeredApple = [&](float y, const std::string& value)
            {
                // v2's PaddingToCenterString uses integer arithmetic. Odd
                // length labels must truncate the half-pixel remainder or the
                // SpriteFont is linearly filtered (FIGHTER/ORC vs MONK).
                const float centeredX = x + std::floor(
                    (memberColumnWidth - AppleTextWidth(value)) * 0.5f);
                AddAppleText(draw, appleFont_, origin, scale,
                             centeredX, y, white, value);
            };
            centeredApple(innerTop + 37, CharacterName(member));
            centeredApple(innerTop + 55, ClassNames[characterClass]);
            centeredApple(innerTop + 73, RaceNames[Party(PartyRace, member) & 0x07]);
            char armor[16];
            std::snprintf(armor, sizeof(armor), "AC %d",
                          10 - Party(PartyArmorClass, member));
            centeredApple(innerTop + 91, armor);

            draw->AddRectFilled(Point(origin, scale, x + 2, innerTop + 117),
                                Point(origin, scale, x + memberColumnWidth - 2,
                                      innerTop + 137), amber);
            const BYTE ready = Party(PartyWeaponReady, member);
            const char* readyLabel = ready == 0 ? "MELEE" : ready == 1 ? "RANGED" : "FISTS";
            const float readyX = x + std::floor(
                (memberColumnWidth - AppleTextWidth(readyLabel)) * 0.5f);
            AddAppleText(draw, appleFont_, origin, scale,
                         readyX, innerTop + 119, white, readyLabel);
        }

        const int stashCount = inventoryState_.StashCount(inventorySlot_);
        draw->AddImage(static_cast<ImTextureID>(inventorySprites_.Id()),
                       Point(origin, scale, stashColumnX + 14, innerTop),
                       Point(origin, scale, stashColumnX + 70, innerTop + 32),
                       ImVec2(112.0f / inventorySprites_.Width(),
                              64.0f / inventorySprites_.Height()),
                       ImVec2(168.0f / inventorySprites_.Width(),
                              96.0f / inventorySprites_.Height()));
        auto centeredStash = [&](float y, const std::string& value, ImU32 color = white)
        {
            const float centeredX = stashColumnX + std::floor(
                (memberColumnWidth - AppleTextWidth(value)) * 0.5f);
            AddAppleText(draw, appleFont_, origin, scale,
                         centeredX, y, color, value);
        };
        centeredStash(innerTop + 37, "STASH");
        centeredStash(innerTop + 55, std::to_string(stashCount) + " / 2");
        if (stashCount == 2) centeredStash(innerTop + 119, "FULL", amber);

        int hoveredRow = -1;
        int hoveredOwner = -1;
        bool hoveredTrash = false;

        // Resolve interaction first. v2 draws the swap helpers behind the
        // marker sprites, so their center dots remain visible over the lines.
        for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex)
        {
            const InventoryRow& row = rows[rowIndex];
            const float y = rowStartY + rowIndex * rowAdvance;
            for (int member = 0; member < partyCount; ++member)
            {
                const float markerX = partyColumnsX + member * memberColumnWidth + 28;
                if (contains(markerX + 5, y, 18, 18))
                {
                    hoveredRow = rowIndex;
                    hoveredOwner = member;
                    break;
                }
            }
            const float stashMarkerX = partyColumnsX
                                     + PartySize * memberColumnWidth + 36;
            if (row.owner < PartySize && stashCount < 2
                && contains(stashMarkerX + 5, y, 18, 18))
            {
                hoveredRow = rowIndex;
                hoveredOwner = PartySize;
            }
            if (row.owner >= PartySize)
            {
                const float trashX = stashMarkerX + 40;
                if (contains(trashX, y, 16, 22))
                {
                    hoveredRow = rowIndex;
                    hoveredOwner = row.owner;
                    hoveredTrash = true;
                }
            }
        }

        if (hoveredRow >= 0 && !hoveredTrash && rows[hoveredRow].owner != hoveredOwner)
        {
            const InventoryRow& row = rows[hoveredRow];
            auto ownerCenterX = [&](int owner)
            {
                return owner < PartySize
                    ? partyColumnsX + owner * memberColumnWidth + 42.0f
                    : partyColumnsX + PartySize * memberColumnWidth + 50.0f;
            };
            const float sourceX = ownerCenterX(row.owner);
            const float targetX = ownerCenterX(hoveredOwner);
            // The v2 interaction rectangle is { markerX + 5, y, 18, 18 }.
            // Its center—and the visible center dot—is therefore y + 9.
            const float sourceY = rowStartY + hoveredRow * rowAdvance + 9.0f;
            draw->AddLine(Point(origin, scale, sourceX, sourceY),
                          Point(origin, scale, targetX, sourceY), amber,
                          std::max(1.0f, 4.0f * scale));
            const auto other = std::find_if(rows.begin(), rows.end(),
                [&](const InventoryRow& candidate)
                {
                    return candidate.owner == hoveredOwner;
                });
            if (other != rows.end())
            {
                const float otherY = rowStartY
                    + static_cast<float>(std::distance(rows.begin(), other)) * rowAdvance
                    + 9.0f;
                draw->AddLine(Point(origin, scale, sourceX, otherY),
                              Point(origin, scale, targetX, otherY), amber,
                              std::max(1.0f, 4.0f * scale));
            }
        }

        for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex)
        {
            const InventoryRow& row = rows[rowIndex];
            const float y = rowStartY + rowIndex * rowAdvance;
            std::string name = inventoryRules_
                             ? inventoryRules_->Name(row.item, englishNames)
                             : "Unknown item";
            char itemText[256];
            const int thaco = inventoryRules_ ? inventoryRules_->Thaco(row.item) : 0;
            const int ac = inventoryRules_ ? -inventoryRules_->ArmorClass(row.item) : 0;
            const std::string special = inventoryRules_ ? inventoryRules_->Special(row.item) : "";
            if (inventorySlot_ < 2)
            {
                const int attacks = inventoryRules_ ? inventoryRules_->Attacks(row.item) : 0;
                const int damageMin = inventoryRules_ ? inventoryRules_->DamageMinimum(row.item) : 0;
                const int damageMax = inventoryRules_ ? inventoryRules_->DamageMaximum(row.item) : 0;
                if (row.charges != 0xFF)
                    std::snprintf(itemText, sizeof(itemText),
                                  "%-14s (%03d)  %+d    %dx %2d-%-2d   %+d   %s",
                                  name.c_str(), row.charges, thaco, attacks,
                                  damageMin, damageMax, ac, special.c_str());
                else
                    std::snprintf(itemText, sizeof(itemText),
                                  "%-20s  %+d    %dx %2d-%-2d   %+d   %s",
                                  name.c_str(), thaco, attacks, damageMin,
                                  damageMax, ac, special.c_str());
            }
            else if (row.charges != 0xFF)
                std::snprintf(itemText, sizeof(itemText),
                              "%-14s (%03d)   %+d    %+d   %s",
                              name.c_str(), row.charges, thaco, ac, special.c_str());
            else
                std::snprintf(itemText, sizeof(itemText),
                              "%-20s   %+d    %+d   %s",
                              name.c_str(), thaco, ac, special.c_str());
            AddAppleText(draw, appleFont_, origin, scale,
                         innerLeft, y, white, itemText);

            for (int member = 0; member < partyCount; ++member)
            {
                const float markerX = partyColumnsX + member * memberColumnWidth + 28;
                // v2 always draws the empty outline, then overlays the small
                // carried/equippable center dot for the owner or hover target.
                draw->AddImage(static_cast<ImTextureID>(inventorySprites_.Id()),
                               Point(origin, scale, markerX, y),
                               Point(origin, scale, markerX + 28, y + 32),
                               ImVec2(0.0f,
                                      96.0f / inventorySprites_.Height()),
                               ImVec2(28.0f / inventorySprites_.Width(),
                                      128.0f / inventorySprites_.Height()));
                const bool preview = hoveredRow == rowIndex && hoveredOwner == member
                                  && !hoveredTrash;
                if (row.owner == member || preview)
                {
                    const float sourceX = inventoryRules_
                        && inventoryRules_->CanEquip(row.item, Party(PartyClass, member),
                                                     Party(PartyRace, member)) ? 56.0f : 28.0f;
                    draw->AddImage(static_cast<ImTextureID>(inventorySprites_.Id()),
                                   Point(origin, scale, markerX, y),
                                   Point(origin, scale, markerX + 28, y + 32),
                                   ImVec2(sourceX / inventorySprites_.Width(),
                                          96.0f / inventorySprites_.Height()),
                                   ImVec2((sourceX + 28) / inventorySprites_.Width(),
                                          128.0f / inventorySprites_.Height()));
                }
            }

            const float stashMarkerX = partyColumnsX
                                     + PartySize * memberColumnWidth + 36;
            draw->AddImage(static_cast<ImTextureID>(inventorySprites_.Id()),
                           Point(origin, scale, stashMarkerX, y),
                           Point(origin, scale, stashMarkerX + 28, y + 32),
                           ImVec2(0.0f,
                                  96.0f / inventorySprites_.Height()),
                           ImVec2(28.0f / inventorySprites_.Width(),
                                  128.0f / inventorySprites_.Height()));
            const bool stashPreview = hoveredRow == rowIndex && hoveredOwner == PartySize
                                   && !hoveredTrash;
            if (row.owner >= PartySize || stashPreview)
            {
                draw->AddImage(static_cast<ImTextureID>(inventorySprites_.Id()),
                               Point(origin, scale, stashMarkerX, y),
                               Point(origin, scale, stashMarkerX + 28, y + 32),
                               ImVec2(28.0f / inventorySprites_.Width(),
                                      96.0f / inventorySprites_.Height()),
                               ImVec2(56.0f / inventorySprites_.Width(),
                                      128.0f / inventorySprites_.Height()));
            }
            if (row.owner >= PartySize)
            {
                const float trashX = stashMarkerX + 40;
                const bool trashHovered = hoveredRow == rowIndex && hoveredTrash;
                const float trashSourceX = trashHovered ? 28.0f : 0.0f;
                draw->AddImage(static_cast<ImTextureID>(inventorySprites_.Id()),
                               Point(origin, scale, trashX, y - 5),
                               Point(origin, scale, trashX + 28, y + 27),
                               ImVec2(trashSourceX / inventorySprites_.Width(),
                                      64.0f / inventorySprites_.Height()),
                               ImVec2((trashSourceX + 28) / inventorySprites_.Width(),
                                      96.0f / inventorySprites_.Height()));
            }
        }

        if (clicked && hoveredRow >= 0)
        {
            const InventoryRow row = rows[hoveredRow];
            if (hoveredTrash && row.owner >= PartySize)
            {
                inventoryChanged_ = inventoryState_.DeleteStashed(
                    inventorySlot_, row.owner - PartySize);
            }
            else if (hoveredOwner < PartySize && row.owner != hoveredOwner)
            {
                inventoryChanged_ = inventoryState_.MoveToParty(
                    inventorySlot_, row.owner, hoveredOwner);
            }
            else if (hoveredOwner == PartySize && row.owner < PartySize)
            {
                inventoryChanged_ = inventoryState_.MovePartyToStash(
                    inventorySlot_, row.owner);
            }
        }
    }

    // Match v2's F11 presentation: first hide the entire Relorded interface
    // behind its configurable black curtain, then place a 2x borderless
    // Apple //e display and amber frame above every normal gameplay overlay.
    if (showAppleVideo && !loading && !gameOver)
    {
        const int opacity = std::clamp(originalInterfaceOpacity, 0, 100);
        draw->AddRectFilled(origin, Point(origin, scale, CanvasWidth, CanvasHeight),
                            IM_COL32(0, 0, 0, opacity * 255 / 100));
        constexpr float appleWidth = 1120.0f;
        constexpr float appleHeight = 768.0f;
        constexpr float border = 5.0f;
        constexpr float appleX = (CanvasWidth - appleWidth) * 0.5f;
        constexpr float appleY = (CanvasHeight - appleHeight) * 0.5f;
        draw->AddRectFilled(Point(origin, scale, appleX - border, appleY - border),
                            Point(origin, scale, appleX + appleWidth + border,
                                  appleY + appleHeight + border),
                            IM_COL32(128, 51, 0, 255));
        draw->AddRectFilled(Point(origin, scale, appleX, appleY),
                            Point(origin, scale, appleX + appleWidth,
                                  appleY + appleHeight),
                            IM_COL32(0, 0, 0, 255));
        draw->AddImage(static_cast<ImTextureID>(appleFramebufferTexture),
                       Point(origin, scale, appleX, appleY),
                       Point(origin, scale, appleX + appleWidth, appleY + appleHeight),
                       ImVec2(20.0f / 600.0f, 1.0f - 18.0f / 420.0f),
                       ImVec2(1.0f - 20.0f / 600.0f, 18.0f / 420.0f));
    }

    // Transition and death presentation deliberately live in the same canvas
    // as the gameplay UI. Hook-driven runtime states and deterministic visual
    // fixtures therefore exercise exactly the same rendering path.
    if (loading && !gameOver)
    {
        draw->AddRectFilled(origin, Point(origin, scale, CanvasWidth, CanvasHeight),
                            IM_COL32(0, 0, 0, 255));
        draw->AddImage(static_cast<ImTextureID>(loadingScreen_.Id()), origin,
                       Point(origin, scale, CanvasWidth, CanvasHeight));
        if (loadingReady)
        {
            const std::string prompt = "PRESS SPACE";
            const bool inverse = (static_cast<int>(ImGui::GetTime()) & 1) != 0;
            const float promptSize = 22.0f;
            AddDeathlordText(draw, deathlordCharset_, origin, scale,
                             CanvasWidth * 0.5f
                                 - DeathlordTextWidth(prompt, promptSize) * 0.5f,
                             945, IM_COL32(255, 220, 55, 255), prompt,
                             promptSize, inverse);
        }
    }
    else if (gameOver)
    {
        draw->AddRectFilled(origin, Point(origin, scale, CanvasWidth, CanvasHeight),
                            IM_COL32(0, 0, 0, 235));
        draw->AddImage(static_cast<ImTextureID>(gameOver_.Id()), origin,
                       Point(origin, scale, CanvasWidth, CanvasHeight));
        centeredText(CanvasWidth * 0.5f, 765, "Deathlord hates you.",
                     IM_COL32(235, 235, 225, 255), 24);
        centeredText(CanvasWidth * 0.5f, 805, "Press Cmd/Alt-R to Reboot.",
                     IM_COL32(255, 220, 55, 255), 20);
    }

    if (paused && !loading)
    {
        draw->AddRectFilled(origin, Point(origin, scale, CanvasWidth, CanvasHeight),
                            IM_COL32(0, 0, 0, 130));
        centeredText(CanvasWidth * 0.5f, CanvasHeight * 0.5f - 36.0f,
                     "GAME PAUSED", IM_COL32(255, 90, 35, 255), 72);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ModernUI::RenderSpellWindow(bool* open)
{
    if (!open || !*open || !spellList_.IsValid()) return;
    ImGui::SetNextWindowPos(ImVec2(30, 35), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Spells", open))
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float scale = std::min(available.x / spellList_.Width(),
                                     available.y / spellList_.Height());
        ImGui::Image(ImTextureRef(static_cast<ImTextureID>(spellList_.Id())),
                     ImVec2(spellList_.Width() * scale, spellList_.Height() * scale));
    }
    ImGui::End();
}

void ModernUI::RenderLogWindow(bool* open)
{
    if (!open || !*open) return;
    ImGui::SetNextWindowSize(ImVec2(440, 620), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Game Log", open))
    {
        if (ImGui::Button("Clear")) log_.clear();
        ImGui::Separator();
        ImGui::BeginChild("##LogScroll", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar);
        for (const std::string& line : longLog_)
            ImGui::TextUnformatted(line.c_str());
        for (auto line = log_.rbegin(); line != log_.rend(); ++line)
            ImGui::TextUnformatted(line->text.c_str());
        ImGui::EndChild();
    }
    ImGui::End();
}

void ModernUI::RenderHostHint(const std::string& text, float centerX, float y,
                              float maximumWidth) const
{
    if (text.empty() || !deathlordCharset_.IsValid()) return;
    constexpr float logicalSize = 16.0f;
    std::vector<std::string> lines = { text };
    if (DeathlordTextWidth(text, logicalSize) > maximumWidth)
    {
        std::size_t bestSplit = std::string::npos;
        float bestBalance = maximumWidth;
        for (std::size_t split = text.find(' '); split != std::string::npos;
             split = text.find(' ', split + 1))
        {
            const std::string first = text.substr(0, split);
            const std::string second = text.substr(split + 1);
            const float firstWidth = DeathlordTextWidth(first, logicalSize);
            const float secondWidth = DeathlordTextWidth(second, logicalSize);
            if (firstWidth <= maximumWidth && secondWidth <= maximumWidth)
            {
                const float balance = std::abs(firstWidth - secondWidth);
                if (balance < bestBalance)
                {
                    bestBalance = balance;
                    bestSplit = split;
                }
            }
        }
        if (bestSplit != std::string::npos)
            lines = { text.substr(0, bestSplit), text.substr(bestSplit + 1) };
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (std::size_t line = 0; line < lines.size(); ++line)
    {
        const std::string& value = lines[line];
        AddDeathlordText(draw, deathlordCharset_, ImVec2(0, 0), 1.0f,
                         std::floor(centerX - DeathlordTextWidth(value, logicalSize) * 0.5f),
                         std::floor(y + line * 19.0f),
                         IM_COL32(235, 235, 220, 255), value, logicalSize);
    }
}

} // namespace dlrl
