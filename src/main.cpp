#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_main.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

#include "Emulator/StdAfx.h"

#include "AppleStyle.h"
#include "DlrlData.h"
#include "DlrlHooks.h"
#include "Frame.h"
#include "InventoryRules.h"
#include "FileUtil.h"
#include "ModernUI.h"
#include "PartyTransfer.h"
#include "Renderer.h"

#include "pp/postprocessor.h"
#include "pp/shader.h"

#include "Emulator/CardManager.h"
#include "Emulator/Core.h"
#include "Emulator/CPU.h"
#include "Emulator/Harddisk.h"
#include "Emulator/Interface.h"
#include "Emulator/Keyboard.h"
#include "Emulator/Memory.h"
#include "Emulator/MockingboardCardManager.h"
#include "Emulator/NTSC.h"
#include "Emulator/RGBMonitor.h"
#include "Emulator/Speaker.h"
#include "Emulator/Video.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 900;
constexpr float kAppleDisplayWidth = 1120.0f;
constexpr float kAppleDisplayHeight = 768.0f;
constexpr float kAppleHintAreaHeight = 48.0f;

enum class UiFixtureMode
{
    Gameplay,
    Battle,
    Inventory,
    Loading,
    GameOver,
};

struct SpeedPreset { double cyclesPerSecond; };
constexpr SpeedPreset kSpeedPresets[] = {
    {510242.225}, {1020484.45}, {2040968.9}, {4081937.8},
    {6122906.7}, {8163875.6}, {2.0e8},
};

struct VideoPreset { const char* label; VideoType_e type; };
constexpr VideoPreset kVideoPresets[] = {
    { "Idealized", VT_COLOR_IDEALIZED },
    { "Text-Optimized RGB", VT_COLOR_VIDEOCARD_RGB },
    { "Composite Monitor", VT_COLOR_MONITOR_NTSC },
    { "TV Screen", VT_COLOR_TV },
};

constexpr uint32_t kVolumeMax = 99;
constexpr const char* kVolumeLabels[] = { "Off", "Soft", "Medium", "Loud", "Extreme" };
constexpr uint32_t kSpeakerAttenuation[] = {
    kVolumeMax, kVolumeMax * 40 / 100, kVolumeMax * 30 / 100,
    kVolumeMax * 25 / 100, 5
};
constexpr uint32_t kMockingboardAttenuation[] = {
    kVolumeMax, kVolumeMax * 35 / 100, kVolumeMax * 25 / 100,
    kVolumeMax * 15 / 100, 0
};

#if defined(__APPLE__)
constexpr SDL_Keymod kPrimaryModifier = SDL_KMOD_GUI;
constexpr SDL_Keymod kRebootModifier = SDL_KMOD_GUI;
#else
constexpr SDL_Keymod kPrimaryModifier = SDL_KMOD_CTRL;
constexpr SDL_Keymod kRebootModifier = SDL_KMOD_ALT;
#endif

struct AppState
{
    struct ScheduledKey { int frame; BYTE key; };
    dlrl::Renderer renderer;
    dlrl::ModernUI modernUi;
    dlrl::InventoryRules inventoryRules;
    dlrl::DlrlHooks hooks;
    std::filesystem::path hdvPath;
    std::filesystem::path temporaryHdv;
    std::filesystem::path capturePath;
    std::string prefDir;
    std::string imguiIniPath;
    uint64_t lastTicksNs = 0;
    int smokeFrames = 0;
    int presentedFrames = 0;
    int fixtureMouseX = -1;
    int fixtureMouseY = -1;
    bool paused = false;
    bool rebootRequested = false;
    bool postprocessorEnabled = false;
    bool showAppleVideoInGame = false;
    bool showInventory = false;
    bool showSpells = false;
    bool showLog = false;
    bool showAbout = false;
    bool showPartyEditor = false;
    bool newGameRequested = false;
    bool importConfirmationRequested = false;
    bool operationErrorRequested = false;
    bool operationNoticeRequested = false;
    bool fixtureImportConfirmation = false;
    bool testCleanReset = false;
    bool englishNames = false;
    bool autoHideMenuBar = false;
    bool fullscreen = false;
    bool uiFixture = false;
    UiFixtureMode uiFixtureMode = UiFixtureMode::Gameplay;
    int speedIndex = 4; // Internal boot/loading acceleration; never user-selectable.
    dlrl::MapViewMode mapViewMode = dlrl::MapViewMode::FollowPlayer;
    int videoIndex = 0;
    int speakerVolume = 3;
    int mockingboardVolume = 3;
    int interfaceColor = static_cast<int>(dlrl::InterfaceColor_Green);
    int originalInterfaceOpacity = 85;
    int windowWidth = kWindowWidth;
    int windowHeight = kWindowHeight;
    int requestedSpeedIndex = -1;
    bool playLoadingAcceleration = false;
    bool startupSplash = false;
    bool startupSplashReady = false;
    bool startupSplashCompleted = false;
    std::mutex pendingHdvMutex;
    std::filesystem::path pendingHdv;
    std::mutex pendingPartyDialogMutex;
    std::filesystem::path pendingPartyExport;
    std::filesystem::path pendingPartyImport;
    std::optional<dlrl::PartySnapshot> partyToImport;
    std::string operationError;
    std::string operationNotice;
    int partyEditorMember = 0;
    std::vector<ScheduledKey> scheduledKeys;
};

constexpr const char* kSettingsFilename = "dlrl.json";
constexpr const char* kPostprocessorFilename = "pp_state.json";

void LoadHostSettings(AppState& state)
{
    if (state.prefDir.empty()) return;
    const auto path = std::filesystem::path(state.prefDir) / kSettingsFilename;
    if (!std::filesystem::exists(path)) return;
    try
    {
        std::ifstream input(path);
        nlohmann::json json;
        input >> json;
        state.windowWidth = std::max(640, json.value("window_width", state.windowWidth));
        state.windowHeight = std::max(480, json.value("window_height", state.windowHeight));
        state.fullscreen = json.value("fullscreen", state.fullscreen);
        state.videoIndex = std::clamp(json.value("video_index", state.videoIndex), 0, 3);
        state.speakerVolume = std::clamp(json.value("speaker_volume", state.speakerVolume), 0, 4);
        state.mockingboardVolume = std::clamp(
            json.value("mockingboard_volume", state.mockingboardVolume), 0, 4);
        state.interfaceColor = std::clamp(json.value("interface_color", state.interfaceColor), 0, 2);
        state.originalInterfaceOpacity = std::clamp(
            json.value("original_interface_opacity", state.originalInterfaceOpacity), 0, 100);
        state.postprocessorEnabled = json.value(
            "postprocessor_enabled", state.postprocessorEnabled);
        state.showAppleVideoInGame = json.value(
            "show_apple_video_in_game", state.showAppleVideoInGame);
        state.showSpells = json.value("show_spells", state.showSpells);
        state.englishNames = json.value("english_names", state.englishNames);
        state.autoHideMenuBar = json.value("menu_auto_hide", state.autoHideMenuBar);
        const int mapViewMode = json.value(
            "map_view_mode", static_cast<int>(state.mapViewMode));
        if ((mapViewMode >= 0 && mapViewMode <= 4) || mapViewMode == 99)
            state.mapViewMode = static_cast<dlrl::MapViewMode>(mapViewMode);
        if (json.contains("relorded_changes"))
        {
            const auto& saved = json["relorded_changes"];
            auto& changes = state.hooks.Changes();
            changes.xpReallocation = saved.value("xp_reallocation", changes.xpReallocation);
            changes.exitPitByMoving = saved.value("exit_pit_by_moving", changes.exitPitByMoving);
            changes.freezeTimeWhenIdle = saved.value("freeze_time_when_idle", changes.freezeTimeWhenIdle);
            changes.rangedAttackForRearLine = saved.value("ranged_attack_for_rear_line", changes.rangedAttackForRearLine);
            changes.searchAlwaysSucceeds = saved.value("search_always_succeeds", changes.searchAlwaysSucceeds);
            changes.noLevelDrain = saved.value("no_level_drain", changes.noLevelDrain);
            changes.magicWaterIncreasesStats = saved.value("magic_water_increases_stats", changes.magicWaterIncreasesStats);
            changes.noStatsLimit = saved.value("no_stats_limit", changes.noStatsLimit);
            changes.noHpLossFromStarvation = saved.value("no_hp_loss_from_starvation", changes.noHpLossFromStarvation);
            changes.extraRaceAndClassBonuses = saved.value("extra_race_and_class_bonuses", changes.extraRaceAndClassBonuses);
            changes.noAutosaveAfterDeath = saved.value("no_autosave_after_death", changes.noAutosaveAfterDeath);
            changes.expandedWeaponUse = saved.value("expanded_weapon_use", changes.expandedWeaponUse);
            changes.keepExtraXpOnLevel = saved.value("keep_extra_xp_on_level", changes.keepExtraXpOnLevel);
            changes.distributeFood = saved.value("distribute_food", changes.distributeFood);
            changes.fixGoldPooling = saved.value("fix_gold_pooling", changes.fixGoldPooling);
            changes.distributeGold = saved.value("distribute_gold", changes.distributeGold);
        }
        const std::filesystem::path savedHdv = json.value("hdv_path", std::string{});
        if (std::filesystem::exists(savedHdv)) state.hdvPath = savedHdv;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Unable to load settings: %s\n", error.what());
    }
}

void SaveSettings(const AppState& state)
{
    if (state.prefDir.empty() || state.smokeFrames > 0) return;
    int width = state.windowWidth;
    int height = state.windowHeight;
    if (state.renderer.Window()) SDL_GetWindowSize(state.renderer.Window(), &width, &height);

    const auto& changes = state.hooks.Changes();
    nlohmann::json json = {
        { "window_width", width },
        { "window_height", height },
        { "fullscreen", state.fullscreen },
        { "video_index", state.videoIndex },
        { "speaker_volume", state.speakerVolume },
        { "mockingboard_volume", state.mockingboardVolume },
        { "interface_color", state.interfaceColor },
        { "original_interface_opacity", state.originalInterfaceOpacity },
        { "postprocessor_enabled", state.postprocessorEnabled },
        { "show_apple_video_in_game", state.showAppleVideoInGame },
        { "show_spells", state.showSpells },
        { "english_names", state.englishNames },
        { "menu_auto_hide", state.autoHideMenuBar },
        { "map_view_mode", static_cast<int>(state.mapViewMode) },
        { "hdv_path", state.hdvPath.string() },
        { "relorded_changes", {
            { "xp_reallocation", changes.xpReallocation },
            { "exit_pit_by_moving", changes.exitPitByMoving },
            { "freeze_time_when_idle", changes.freezeTimeWhenIdle },
            { "ranged_attack_for_rear_line", changes.rangedAttackForRearLine },
            { "search_always_succeeds", changes.searchAlwaysSucceeds },
            { "no_level_drain", changes.noLevelDrain },
            { "magic_water_increases_stats", changes.magicWaterIncreasesStats },
            { "no_stats_limit", changes.noStatsLimit },
            { "no_hp_loss_from_starvation", changes.noHpLossFromStarvation },
            { "extra_race_and_class_bonuses", changes.extraRaceAndClassBonuses },
            { "no_autosave_after_death", changes.noAutosaveAfterDeath },
            { "expanded_weapon_use", changes.expandedWeaponUse },
            { "keep_extra_xp_on_level", changes.keepExtraXpOnLevel },
            { "distribute_food", changes.distributeFood },
            { "fix_gold_pooling", changes.fixGoldPooling },
            { "distribute_gold", changes.distributeGold },
        }},
    };
    try
    {
        std::ofstream output(std::filesystem::path(state.prefDir) / kSettingsFilename);
        output << json.dump(2);
        sa2::PostProcessor::GetInstance()->SaveState(
            (std::filesystem::path(state.prefDir) / kPostprocessorFilename).string());
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Unable to save settings: %s\n", error.what());
    }
}

void ApplySpeed(const AppState& state)
{
    g_fCurrentCLK6502 = kSpeedPresets[state.speedIndex].cyclesPerSecond;
    SpkrReinitialize();
}

void BeginPlayLoadingAcceleration(AppState& state)
{
    if (state.playLoadingAcceleration || state.hooks.State().inGameMap) return;

    state.playLoadingAcceleration = true;
    state.requestedSpeedIndex = 4;
    std::puts("DLRL internal speed: Play Game loading at 6x");
    std::fflush(stdout);
}

void ApplyVideo(const AppState& state)
{
    Video& video = GetVideo();
    video.SetVideoType(kVideoPresets[state.videoIndex].type);
    video.SetVideoStyle(VS_COLOR_VERTICAL_BLEND);
    video.VideoReinitialize(false);
    GetFrame().VideoRedrawScreen();
}

void ApplyVolume(const AppState& state)
{
    SpkrSetVolume(kSpeakerAttenuation[state.speakerVolume], kVolumeMax);
    GetCardMgr().GetMockingboardCardMgr().SetVolume(
        kMockingboardAttenuation[state.mockingboardVolume], kVolumeMax);
}

void RecalculateArmorClasses()
{
    // V2 temporarily entered Deathlord's own all-party AC routine after every
    // inventory mutation, then restored the interrupted CPU registers.
    constexpr WORD begin = 0xA93F;
    constexpr WORD end = 0xA9E4;
    const regsrec saved = regs;
    regs.pc = begin;
    int instructions = 0;
    while (regs.pc != end && instructions < 5000)
    {
        CpuExecute(1, false);
        ++instructions;
    }
    regs = saved;
    if (instructions >= 5000)
        std::fprintf(stderr, "Deathlord armor-class recalculation did not terminate\n");
}

void SetPaused(AppState& state, bool paused)
{
    state.paused = paused;
    g_nAppMode = paused ? MODE_PAUSED : MODE_RUNNING;
    if (paused)
    {
        Spkr_Mute();
        GetCardMgr().GetMockingboardCardMgr().MuteControl(true);
    }
    else
    {
        Spkr_Unmute();
        GetCardMgr().GetMockingboardCardMgr().MuteControl(false);
    }
}

void DismissStartupSplash(AppState& state)
{
    if (!state.startupSplash || !state.startupSplashReady) return;
    state.startupSplash = false;
    state.startupSplashReady = false;
    state.startupSplashCompleted = true;
    // The clean HDV waits for the same Space at the underlying Deathlord
    // startup gate. Forward the dismissal so a brand-new game can continue
    // into character creation rather than returning to the interstitial.
    KeybQueueKeypress(' ', ASCII);
    SetPaused(state, false);
    std::puts("DLRL startup splash: dismissed with Space");
    std::fflush(stdout);
}

void SetFullscreen(AppState& state, bool fullscreen)
{
    if (!SDL_SetWindowFullscreen(state.renderer.Window(), fullscreen))
    {
        std::fprintf(stderr, "Unable to change fullscreen state: %s\n", SDL_GetError());
        return;
    }
    // SDL3 window state changes are asynchronous. Rendering or capturing
    // immediately otherwise uses the old drawable size for several frames.
    if (!SDL_SyncWindow(state.renderer.Window()))
        std::fprintf(stderr, "Unable to synchronize fullscreen state: %s\n", SDL_GetError());
    state.fullscreen = fullscreen;
}

std::filesystem::path FindResourcesDir()
{
    const char* base = SDL_GetBasePath();
    std::filesystem::path dir = base ? base : ".";
    for (int i = 0; i < 7; ++i)
    {
        const auto staged = dir / "Resources";
        if (std::filesystem::exists(staged / "Apple2e_Enhanced.rom")) return staged;
        const auto source = dir / "Deathlord-Relorded" / "Emulator" / "Resources";
        if (std::filesystem::exists(source / "Apple2e_Enhanced.rom")) return source;
        dir = dir.parent_path();
        if (dir.empty()) break;
    }
    return "Resources";
}

std::filesystem::path FindDevelopmentHdv()
{
    const char* base = SDL_GetBasePath();
    std::filesystem::path dir = base ? base : ".";
    for (int i = 0; i < 7; ++i)
    {
        const auto staged = dir / "Resources" / "Images" / "Deathlord PRODOS.hdv";
        if (std::filesystem::exists(staged)) return staged;
        const auto packaged = dir / "Images" / "Deathlord PRODOS.hdv";
        if (std::filesystem::exists(packaged)) return packaged;
        const auto local = dir / "assets" / "Images" / "Deathlord PRODOS.hdv";
        if (std::filesystem::exists(local)) return local;
        dir = dir.parent_path();
        if (dir.empty()) break;
    }
    return {};
}

std::filesystem::path FindDlrlAssetsDir()
{
    const char* base = SDL_GetBasePath();
    std::filesystem::path dir = base ? base : ".";
    for (int i = 0; i < 7; ++i)
    {
        const auto portable = dir / "DLRLAssets";
        if (std::filesystem::exists(portable / "InventoryList.csv")) return portable;
        const auto staged = dir / "Resources" / "DLRLAssets";
        if (std::filesystem::exists(staged / "InventoryList.csv")) return staged;
        const auto source = dir / "Deathlord-Relorded" / "Assets";
        if (std::filesystem::exists(source / "InventoryList.csv")) return source;
        dir = dir.parent_path();
        if (dir.empty()) break;
    }
    return "DLRLAssets";
}

std::filesystem::path FindPortableAssetsDir()
{
    const char* base = SDL_GetBasePath();
    std::filesystem::path dir = base ? base : ".";
    for (int i = 0; i < 7; ++i)
    {
        const auto portable = dir / "assets";
        if (std::filesystem::exists(portable / "Apple2eFont14x16")) return portable;
        const auto staged = dir / "Resources" / "assets";
        if (std::filesystem::exists(staged / "Apple2eFont14x16")) return staged;
        const auto source = dir / "assets" / "pp" / "assets";
        if (std::filesystem::exists(source / "Apple2eFont14x16")) return source;
        dir = dir.parent_path();
        if (dir.empty()) break;
    }
    return "assets";
}

std::filesystem::path UniqueTemporaryHdv()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
         / ("dlrl-window-smoke-" + std::to_string(stamp) + ".hdv");
}

std::filesystem::path ResolveHdv(AppState& state, const std::filesystem::path& requested)
{
    const std::filesystem::path source = requested.empty() ? FindDevelopmentHdv() : requested;
    if (source.empty() || !std::filesystem::exists(source)) return {};

    if (state.smokeFrames > 0)
    {
        state.temporaryHdv = UniqueTemporaryHdv();
        std::filesystem::copy_file(source, state.temporaryHdv,
                                   std::filesystem::copy_options::overwrite_existing);
        return state.temporaryHdv;
    }

    if (requested.empty() && !state.prefDir.empty())
    {
        const auto saved = std::filesystem::path(state.prefDir) / "Deathlord PRODOS.hdv";
        if (!std::filesystem::exists(saved))
            std::filesystem::copy_file(source, saved);
        return saved;
    }
    return source;
}

bool InitEmulator(AppState& state, const std::filesystem::path& hdvPath)
{
    g_nAppMode = MODE_RUNNING;
    SpkrInitialize();
    SetApple2Type(A2TYPE_APPLE2EENHANCED);
    RGB_SetVideocard(Video7_SL7, 15, 0);

    Video& video = GetVideo();
    video.SetVideoType(VT_COLOR_IDEALIZED);
    video.SetVideoStyle(VS_COLOR_VERTICAL_BLEND);
    video.SetVideoRefreshRate(VR_60HZ);
    SetCurrentCLK6502();

    GetCardMgr().Insert(SLOT7, CT_GenericHDD, false);
    GetCardMgr().Insert(SLOT4, CT_MockingboardC, false);
    GetFrame().Initialize(true);
    MemInitialize();
    GetCardMgr().Reset(true);
    state.hooks.ResetRuntime();
    state.hooks.Attach();

    auto* hdc = static_cast<HarddiskInterfaceCard*>(GetCardMgr().GetObj(SLOT7));
    if (!hdc)
    {
        std::fprintf(stderr, "SmartPort card did not initialize\n");
        return false;
    }
    if (!hdvPath.empty() && !hdc->Insert(HARDDISK_1, hdvPath.string()))
    {
        std::fprintf(stderr, "Unable to insert DLRL HDV: %s\n", hdvPath.string().c_str());
        return false;
    }

    GetFrame().VideoRedrawScreen();
    // DLRL v2 and the original Apple II flow expect command letters in
    // uppercase. In particular, autoroll recognizes $C1 ('A' with strobe),
    // not lowercase $E1. NAC deliberately disabled Caps Lock for prose input;
    // Deathlord must retain the AppleWin/v2 default instead.
    KeybSetCapsLock(true);
    return true;
}

void ShutdownEmulator(AppState& state)
{
    state.hooks.Detach();
    if (auto* hdc = static_cast<HarddiskInterfaceCard*>(GetCardMgr().GetObj(SLOT7)))
        hdc->Unplug(HARDDISK_1);
    GetCardMgr().Destroy();
    SpkrDestroy();
    MemDestroy();
    GetFrame().Destroy();
}

void RebootEmulator(AppState& state)
{
    g_nAppMode = MODE_RUNNING;
    g_bFullSpeed = false;
    MemReset();
    GetVideo().VideoResetState();
    KeybReset();
    GetCardMgr().Reset(true);
    state.hooks.ResetRuntime();
    state.modernUi.ResetText();
    SpkrReset();
    SetActiveCpu(GetMainCpu());
    GetFrame().VideoRedrawScreen();
    state.speedIndex = 4;
    state.requestedSpeedIndex = -1;
    state.playLoadingAcceleration = false;
    state.startupSplash = false;
    state.startupSplashReady = false;
    state.startupSplashCompleted = false;
    ApplySpeed(state);
    SetPaused(state, false);
}

void SeedModernUiFixture(AppState& state)
{
    using namespace dlrl::deathlord;
    auto setName = [](int member, const char* name)
    {
        BYTE* destination = MemGetMainPtr(static_cast<WORD>(PartyName + member * 9));
        std::fill_n(destination, 9, static_cast<BYTE>(' ' ^ 0x65));
        const std::size_t length = std::min<std::size_t>(std::strlen(name), 9);
        for (std::size_t i = 0; i < length; ++i)
            destination[i] = static_cast<BYTE>((name[i] ^ 0x65) & 0x7F);
        if (length > 0) destination[length - 1] |= 0x80;
    };

    MemGetMainPtr(MapIsInGame)[0] = 0xE5;
    MemGetMainPtr(MapIsOverland)[0] = 0x80;
    MemGetMainPtr(MapType)[0] = 1;
    MemGetMainPtr(MapX)[0] = 31;
    MemGetMainPtr(MapY)[0] = 30;
    MemGetMainPtr(MapOverlandX)[0] = 6;
    MemGetMainPtr(MapOverlandY)[0] = 8;
    MemGetMainPtr(DayHour)[0] = 17;
    MemGetMainPtr(DayMinute)[0] = 0x30;
    MemGetMainPtr(DayOfMonth)[0] = 12;
    MemGetMainPtr(PartySizeAddress)[0] = PartySize;
    MemGetMainPtr(PartyCurrentCharacter)[0] = 0;
    constexpr const char* names[] = {"AKIRA", "MARIKO", "TARO", "HANA", "KENJI", "YUKI"};
    constexpr BYTE races[] = {0, 0, 1, 0, 1, 0};
    constexpr BYTE classes[] = {0, 1, 2, 5, 12, 15};
    {
        constexpr const char* partyName = "LOST SECTORS";
        BYTE* destination = MemGetMainPtr(PartyPartyName);
        std::fill_n(destination, 16, static_cast<BYTE>(' ' ^ 0x65));
        const std::size_t length = std::strlen(partyName);
        for (std::size_t i = 0; i < length; ++i)
            destination[i] = static_cast<BYTE>((partyName[i] ^ 0x65) & 0x7F);
        destination[length - 1] |= 0x80;
    }
    BYTE* map = MemGetMainPtr(GameMap);
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            // A dense, asymmetric overland fixture. Tile $00 is real terrain,
            // not transparency; regions exercise static, animated, and
            // monster look-up paths without the old artificial cross.
            BYTE tile = 0x01;
            const int lakeX = x - 14;
            const int lakeY = y - 17;
            if (lakeX * lakeX * 3 + lakeY * lakeY * 7 < 260) tile = 0x2B;
            else if (x > 47 && y < 19) tile = static_cast<BYTE>(0x08 + (x + y) % 8);
            else if (y > 48 && x > 35) tile = static_cast<BYTE>(0x30 + (x * 3 + y) % 8);
            else if ((x * 11 + y * 17) % 43 == 0) tile = 0x02;
            else if ((x * 19 + y * 7) % 67 == 0) tile = 0x03;
            else if ((x * 11 + y * 17) % 97 == 0) tile = 0x04;
            else if ((x - 36) * (x - 36) + (y - 34) * (y - 34) < 42)
                tile = static_cast<BYTE>(0x10 + (x + y) % 5);
            map[x + y * 64] = tile;
        }
    }
    for (int index = 0; index < 16; ++index)
        MemGetMainPtr(MapMonsterSpriteIds)[index] = static_cast<BYTE>(0x20 + index);
    constexpr std::array<std::pair<int, int>, 6> monsters = {{
        {14, 24}, {23, 16}, {38, 22}, {47, 39}, {27, 48}, {52, 12}
    }};
    for (std::size_t i = 0; i < monsters.size(); ++i)
        map[monsters[i].first + monsters[i].second * 64] = static_cast<BYTE>(0x40 + i);
    for (int member = 0; member < PartySize; ++member)
    {
        setName(member, names[member]);
        MemGetMainPtr(PartyRace)[member] = races[member];
        MemGetMainPtr(PartyClass)[member] = classes[member];
        MemGetMainPtr(PartyGender)[member] = static_cast<BYTE>(member & 1);
        MemGetMainPtr(PartyLevel)[member] = static_cast<BYTE>(8 + member * 3);
        MemGetMainPtr(PartyLevelPlus)[member] = member == 2 ? 1 : 0;
        const int health = 80 + member * 37;
        const int maximum = health + 25;
        MemGetMainPtr(PartyHealthLow)[member] = static_cast<BYTE>(health);
        MemGetMainPtr(PartyHealthHigh)[member] = static_cast<BYTE>(health >> 8);
        MemGetMainPtr(PartyHealthMaxLow)[member] = static_cast<BYTE>(maximum);
        MemGetMainPtr(PartyHealthMaxHigh)[member] = static_cast<BYTE>(maximum >> 8);
        MemGetMainPtr(PartyPower)[member] = static_cast<BYTE>(30 + member * 7);
        MemGetMainPtr(PartyPowerMax)[member] = static_cast<BYTE>(55 + member * 7);
        const int gold = 1200 + member * 733;
        MemGetMainPtr(PartyGoldLow)[member] = static_cast<BYTE>(gold);
        MemGetMainPtr(PartyGoldHigh)[member] = static_cast<BYTE>(gold >> 8);
        MemGetMainPtr(PartyArmorClass)[member] = static_cast<BYTE>(12 + member);
        MemGetMainPtr(PartyFood)[member] = static_cast<BYTE>(member == 4 ? 9 : 35 + member * 4);
        MemGetMainPtr(PartyTorches)[member] = static_cast<BYTE>(8 - member);
        MemGetMainPtr(PartyStrength)[member] = static_cast<BYTE>(12 + member);
        MemGetMainPtr(PartyConstitution)[member] = static_cast<BYTE>(11 + member);
        MemGetMainPtr(PartySizeAttribute)[member] = static_cast<BYTE>(13 + member);
        MemGetMainPtr(PartyIntelligence)[member] = static_cast<BYTE>(14 + member);
        MemGetMainPtr(PartyDexterity)[member] = static_cast<BYTE>(15 + member);
        MemGetMainPtr(PartyCharisma)[member] = static_cast<BYTE>(10 + member);
        MemGetMainPtr(PartyMagicUserType)[member] = member < 3 ? 0xFF : member - 3;
        MemGetMainPtr(PartyStatus)[member] = member == 1 ? 0x04
                                            : member == 4 ? 0x20 : 0;
        BYTE* inventory = MemGetMainPtr(static_cast<WORD>(PartyInventory + member * 0x20));
        std::fill_n(inventory, 16, static_cast<BYTE>(0xFF));
    }
    constexpr std::array<BYTE, PartySize> fixtureMelee = {0x00, 0x0A, 0x26, 0x46, 0x02, 0x32};
    constexpr std::array<BYTE, PartySize> fixtureRanged = {0x12, 0xFF, 0x21, 0xFF, 0x12, 0xFF};
    for (int member = 0; member < PartySize; ++member)
    {
        BYTE* inventory = MemGetMainPtr(static_cast<WORD>(PartyInventory + member * 0x20));
        inventory[0] = fixtureMelee[member];
        inventory[1] = fixtureRanged[member];
        inventory[8] = 0;
        inventory[9] = fixtureRanged[member] == 0xFF ? 0xFF : 12 + member;
        MemGetMainPtr(PartyWeaponReady)[member] = member < 3 ? 0 : 1;
    }
    MemGetMainPtr(PartyCurrentClass)[0] = classes[0];
    state.modernUi.SeedVisualFixture();
    if (state.uiFixtureMode == UiFixtureMode::Battle)
        state.modernUi.SeedBattleFixture();
    if (state.uiFixtureMode == UiFixtureMode::Inventory)
        state.modernUi.SeedInventoryFixture();
    state.hooks.HandleInstruction(0);
}

void LoadHdv(AppState& state, const std::filesystem::path& path)
{
    if (path.empty() || !std::filesystem::exists(path)) return;
    auto* hdc = static_cast<HarddiskInterfaceCard*>(GetCardMgr().GetObj(SLOT7));
    if (!hdc) return;

    const auto previous = state.hdvPath;
    hdc->Unplug(HARDDISK_1);
    if (!hdc->Insert(HARDDISK_1, path.string()))
    {
        std::fprintf(stderr, "Unable to load HDV: %s\n", path.string().c_str());
        if (!previous.empty()) hdc->Insert(HARDDISK_1, previous.string());
        return;
    }
    state.hdvPath = path;
    RebootEmulator(state);
}

bool StartCleanNewGame(AppState& state, std::string& error)
{
    const std::filesystem::path cleanHdv = FindDevelopmentHdv();
    if (cleanHdv.empty() || state.hdvPath.empty())
    {
        error = "The clean or active hard disk image could not be found.";
        return false;
    }

    auto* hdc = static_cast<HarddiskInterfaceCard*>(GetCardMgr().GetObj(SLOT7));
    if (!hdc)
    {
        error = "The SmartPort hard disk controller is unavailable.";
        return false;
    }

    hdc->Unplug(HARDDISK_1);
    if (!dlrl::ReplaceFileFromTemplate(cleanHdv, state.hdvPath, error))
    {
        hdc->Insert(HARDDISK_1, state.hdvPath.string());
        return false;
    }
    if (!hdc->Insert(HARDDISK_1, state.hdvPath.string()))
    {
        error = "The clean image was installed but could not be reopened.";
        return false;
    }
    // A clean game starts unexplored: drop every saved fog-of-war marker.
    state.modernUi.ResetFogOfWar();
    RebootEmulator(state);
    return true;
}

void SDLCALL HdvDialogCallback(void* userdata, const char* const* files, int)
{
    if (!files || !files[0]) return;
    auto& state = *static_cast<AppState*>(userdata);
    std::lock_guard<std::mutex> lock(state.pendingHdvMutex);
    state.pendingHdv = files[0];
}

void OpenHdvDialog(AppState& state)
{
    static const SDL_DialogFileFilter filters[] = {
        { "Hard disk image (*.hdv)", "hdv" },
        { "All files", "*" },
    };
    std::string initialDirectory;
    if (!state.hdvPath.empty())
    {
        initialDirectory = std::filesystem::absolute(state.hdvPath.parent_path()).string();
        initialDirectory.push_back(static_cast<char>(std::filesystem::path::preferred_separator));
    }
    SDL_ShowOpenFileDialog(&HdvDialogCallback, &state, state.renderer.Window(),
                           filters, static_cast<int>(std::size(filters)),
                           initialDirectory.empty() ? nullptr : initialDirectory.c_str(), false);
}

void SDLCALL PartyExportDialogCallback(void* userdata, const char* const* files, int)
{
    if (!files || !files[0]) return;
    auto& state = *static_cast<AppState*>(userdata);
    std::lock_guard<std::mutex> lock(state.pendingPartyDialogMutex);
    state.pendingPartyExport = files[0];
}

void SDLCALL PartyImportDialogCallback(void* userdata, const char* const* files, int)
{
    if (!files || !files[0]) return;
    auto& state = *static_cast<AppState*>(userdata);
    std::lock_guard<std::mutex> lock(state.pendingPartyDialogMutex);
    state.pendingPartyImport = files[0];
}

std::string SafePartyFilename(const std::string& partyName)
{
    std::string filename;
    for (char character : partyName)
    {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte)) filename.push_back(character);
        else if ((character == ' ' || character == '-' || character == '_')
                 && !filename.empty() && filename.back() != '-')
            filename.push_back('-');
    }
    while (!filename.empty() && filename.back() == '-') filename.pop_back();
    return filename.empty() ? "deathlord-party" : filename;
}

void OpenPartyExportDialog(AppState& state)
{
    static const SDL_DialogFileFilter filters[] = {
        { "Deathlord Relorded party (*.json)", "json" },
    };
    const std::string partyName = dlrl::PartyTransfer::DecodeString(
        dlrl::deathlord::PartyPartyName, dlrl::PartySnapshot::PartyNameLength);
    std::filesystem::path suggested = state.prefDir.empty()
        ? std::filesystem::current_path() : std::filesystem::path(state.prefDir);
    suggested /= SafePartyFilename(partyName) + ".dlrl-party.json";
    const std::string location = suggested.string();
    SDL_ShowSaveFileDialog(&PartyExportDialogCallback, &state, state.renderer.Window(),
                           filters, static_cast<int>(std::size(filters)), location.c_str());
}

void OpenPartyImportDialog(AppState& state)
{
    static const SDL_DialogFileFilter filters[] = {
        { "Deathlord Relorded party (*.json)", "json" },
        { "All files", "*" },
    };
    const char* initial = state.prefDir.empty() ? nullptr : state.prefDir.c_str();
    SDL_ShowOpenFileDialog(&PartyImportDialogCallback, &state, state.renderer.Window(),
                           filters, static_cast<int>(std::size(filters)), initial, false);
}

BYTE SdlKeyToAscii(SDL_Keycode key, SDL_Keymod mod)
{
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

WPARAM SdlKeyToVirtualKey(SDL_Keycode key)
{
    switch (key)
    {
    case SDLK_LEFT: return VK_LEFT;
    case SDLK_RIGHT: return VK_RIGHT;
    case SDLK_UP: return VK_UP;
    case SDLK_DOWN: return VK_DOWN;
    case SDLK_DELETE: return VK_DELETE;
    case SDLK_INSERT: return VK_INSERT;
    case SDLK_HOME: return VK_HOME;
    case SDLK_END: return VK_END;
    case SDLK_PAGEUP: return VK_PRIOR;
    case SDLK_PAGEDOWN: return VK_NEXT;
    case SDLK_F1: return VK_F1; case SDLK_F2: return VK_F2;
    case SDLK_F3: return VK_F3; case SDLK_F4: return VK_F4;
    case SDLK_F5: return VK_F5; case SDLK_F6: return VK_F6;
    case SDLK_F7: return VK_F7; case SDLK_F8: return VK_F8;
    case SDLK_F9: return VK_F9; case SDLK_F10: return VK_F10;
    case SDLK_F11: return VK_F11; case SDLK_F12: return VK_F12;
    default: return 0;
    }
}

void RunEmulator(AppState& state)
{
    if (state.paused || state.uiFixture) return;

    const uint64_t now = SDL_GetTicksNS();
    uint32_t cycles = 0;
    if (state.smokeFrames > 0)
    {
        cycles = NTSC_GetCyclesPerFrame();
    }
    else
    {
        const uint64_t delta = now - state.lastTicksNs;
        cycles = static_cast<uint32_t>(
            delta * kSpeedPresets[state.speedIndex].cyclesPerSecond * 1e-9);
        // Autoroll's v2 speed 6 is AppleWin's unrestricted/full-speed mode.
        // Keep the ordinary catch-up guard, but let this hidden mode consume
        // its full 200 MHz budget while it searches for qualifying stats.
        const uint32_t maximumFrames = state.speedIndex == 6 ? 256u : 16u;
        cycles = std::clamp(cycles, 1u, NTSC_GetCyclesPerFrame() * maximumFrames);
    }
    state.lastTicksNs = now;

    const uint32_t batchSize = static_cast<uint32_t>(g_fCurrentCLK6502 * 1e-3);
    uint32_t total = 0;
    while (total < cycles)
    {
        const uint32_t requested = std::min(batchSize, cycles - total);
        const uint32_t executed = CpuExecute(requested, true);
        if (executed == 0) break;
        total += executed;
        GetCardMgr().Update(executed);
        SpkrUpdate(executed);
    }
    g_dwCyclesThisFrame = (g_dwCyclesThisFrame + total) % NTSC_GetCyclesPerFrame();
    GetFrame().VideoRedrawScreen();
}

void ReceiveDlrlEvent(const dlrl::HookEvent& event, void* userData)
{
    auto& state = *static_cast<AppState*>(userData);
    state.modernUi.HandleEvent(event);
    if (event.type == dlrl::HookEventType::RequestSpeed)
    {
        if (event.pc == dlrl::deathlord::PcMenuKey && event.value >= 6)
            BeginPlayLoadingAcceleration(state);
        else if (event.pc == dlrl::deathlord::PcCharacterManagementKey
                 && event.value >= 6)
            state.requestedSpeedIndex = 6; // v2 EmulatorSetSpeed(6): full speed.
        else if (!(state.playLoadingAcceleration && event.value < 6))
            state.requestedSpeedIndex = event.value >= 6 ? 4 : 1;
    }
    else if (event.type == dlrl::HookEventType::StartupSplash)
    {
        if (!state.startupSplash && !state.startupSplashCompleted)
        {
            state.startupSplash = true;
            state.startupSplashReady = true;
            state.playLoadingAcceleration = false;
            state.requestedSpeedIndex = 1;
            std::puts("DLRL startup splash: pre-game validation complete");
            std::puts("DLRL internal speed: main game ready at 1x");
            std::puts("DLRL startup splash: waiting for Space");
            std::fflush(stdout);
        }
    }
}

std::string StartMenuHint(const AppState& state)
{
    switch (state.hooks.State().startMenu)
    {
    case dlrl::StartMenuState::Title:
        return "Press any key";
    case dlrl::StartMenuState::Menu:
        return "Choose 'U', 'C', or 'P' to continue";
    case dlrl::StartMenuState::AttributesRerollPropose:
        return "Press 'A' to autoroll until STR/CON/INT/DEX are all at least 1 below the maximum for the chosen race";
    case dlrl::StartMenuState::AttributesRerolling:
        return "Rolling attributes for STR/CON/INT/DEX >= (max-1), press almost any key to cancel";
    case dlrl::StartMenuState::AttributesRerollDone:
        return "Saved you rolling " + std::to_string(state.hooks.State().rerollCount)
             + " times to get this set of attributes";
    default:
        return {};
    }
}

int PartyWord(std::uint16_t lowAddress, std::uint16_t highAddress, int member)
{
    return MemGetMainPtr(lowAddress)[member]
         | (MemGetMainPtr(highAddress)[member] << 8);
}

void SetPartyWord(std::uint16_t lowAddress, std::uint16_t highAddress,
                  int member, int value)
{
    value = std::clamp(value, 0, 65535);
    MemGetMainPtr(lowAddress)[member] = static_cast<BYTE>(value);
    MemGetMainPtr(highAddress)[member] = static_cast<BYTE>(value >> 8);
}

void RenderPartyEditor(AppState& state, bool activeParty)
{
    using namespace dlrl::deathlord;
    if (!state.showPartyEditor) return;
    if (!activeParty)
    {
        state.showPartyEditor = false;
        return;
    }

    constexpr std::array<const char*, 16> classNames = {
        "Fighter", "Paladin", "Ranger", "Barbarian", "Berzerker", "Samurai",
        "Dark Knight", "Thief", "Assassin", "Ninja", "Monk", "Priest", "Druid",
        "Magician", "Illusionist", "Peasant"
    };
    constexpr std::array<const char*, 8> raceNames = {
        "Human", "Elf", "Half-Elf", "Dwarf", "Gnome", "Dark Elf", "Orc", "Half-Orc"
    };
    constexpr std::array<const char*, 3> alignmentNames = {"Good", "Neutral", "Evil"};
    constexpr std::array<const char*, 8> inventoryNames = {
        "Melee", "Ranged", "Chest", "Shield", "Misc.", "Jewelry", "Tool", "Scroll"
    };

    ImGui::SetNextWindowSize(ImVec2(900, 760), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Party Editor", &state.showPartyEditor))
    {
        ImGui::End();
        return;
    }

    std::array<char, 17> partyName{};
    const std::string currentPartyName = dlrl::PartyTransfer::DecodeString(
        PartyPartyName, dlrl::PartySnapshot::PartyNameLength);
    std::copy_n(currentPartyName.c_str(),
                std::min(currentPartyName.size(), partyName.size() - 1), partyName.data());
    if (ImGui::InputText("Party name", partyName.data(), partyName.size()))
        dlrl::PartyTransfer::EncodeString(PartyPartyName, partyName.size() - 1,
                                          partyName.data(), true);

    state.partyEditorMember = std::clamp(state.partyEditorMember, 0, PartySize - 1);
    std::array<std::string, PartySize> memberLabels;
    for (int member = 0; member < PartySize; ++member)
    {
        memberLabels[member] = std::to_string(member + 1) + ": "
            + dlrl::PartyTransfer::DecodeString(
                static_cast<std::uint16_t>(PartyName + member * 9), 9);
    }
    if (ImGui::BeginCombo("Character", memberLabels[state.partyEditorMember].c_str()))
    {
        for (int member = 0; member < PartySize; ++member)
            if (ImGui::Selectable(memberLabels[member].c_str(),
                                  state.partyEditorMember == member))
                state.partyEditorMember = member;
        ImGui::EndCombo();
    }

    const int member = state.partyEditorMember;
    ImGui::PushID(member);
    bool recalculateArmor = false;

    auto editByte = [&](const char* label, std::uint16_t address,
                        int minimum, int maximum)
    {
        int value = MemGetMainPtr(address)[member];
        ImGui::SetNextItemWidth(105.0f);
        if (!ImGui::InputInt(label, &value)) return false;
        MemGetMainPtr(address)[member] = static_cast<BYTE>(
            std::clamp(value, minimum, maximum));
        return true;
    };
    auto editWord = [&](const char* label, std::uint16_t low, std::uint16_t high,
                        int maximum)
    {
        int value = PartyWord(low, high, member);
        ImGui::SetNextItemWidth(105.0f);
        if (!ImGui::InputInt(label, &value)) return false;
        SetPartyWord(low, high, member, std::clamp(value, 0, maximum));
        return true;
    };
    auto editCombo = [&](const char* label, std::uint16_t address,
                         const auto& labels)
    {
        int value = std::min<int>(MemGetMainPtr(address)[member], labels.size() - 1);
        bool changed = false;
        if (ImGui::BeginCombo(label, labels[value]))
        {
            for (int index = 0; index < static_cast<int>(labels.size()); ++index)
            {
                if (ImGui::Selectable(labels[index], value == index))
                {
                    value = index;
                    MemGetMainPtr(address)[member] = static_cast<BYTE>(index);
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    };

    if (ImGui::CollapsingHeader("Identity", ImGuiTreeNodeFlags_DefaultOpen))
    {
        std::array<char, 10> name{};
        const std::string currentName = dlrl::PartyTransfer::DecodeString(
            static_cast<std::uint16_t>(PartyName + member * 9), 9);
        std::copy_n(currentName.c_str(), std::min(currentName.size(), name.size() - 1),
                    name.data());
        if (ImGui::InputText("Name", name.data(), name.size()))
            dlrl::PartyTransfer::EncodeString(
                static_cast<std::uint16_t>(PartyName + member * 9), 9, name.data(), false);
        const bool classChanged = editCombo("Class", PartyClass, classNames);
        recalculateArmor |= classChanged;
        if (classChanged && MemGetMainPtr(PartyCurrentCharacter)[0] == member)
            MemGetMainPtr(PartyCurrentClass)[0] = MemGetMainPtr(PartyClass)[member];
        recalculateArmor |= editCombo("Race", PartyRace, raceNames);
        constexpr std::array<const char*, 2> genderNames = {"Male", "Female"};
        editCombo("Gender", PartyGender, genderNames);
        editCombo("Alignment", PartyAlignment, alignmentNames);

        constexpr std::array<const char*, 5> magicNames = {
            "None", "Priest", "Druid", "Magician", "Illusionist"
        };
        const BYTE storedMagic = MemGetMainPtr(PartyMagicUserType)[member];
        int magic = storedMagic == 0xFF ? 0 : std::min<int>(storedMagic + 1, 4);
        if (ImGui::BeginCombo("Magic discipline", magicNames[magic]))
        {
            for (int index = 0; index < static_cast<int>(magicNames.size()); ++index)
            {
                if (ImGui::Selectable(magicNames[index], magic == index))
                {
                    magic = index;
                    MemGetMainPtr(PartyMagicUserType)[member] = index == 0
                        ? 0xFF : static_cast<BYTE>(index - 1);
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Make party leader"))
        {
            MemGetMainPtr(PartyLeader)[0] = static_cast<BYTE>(member);
            MemGetMainPtr(PartyCurrentCharacter)[0] = static_cast<BYTE>(member);
            MemGetMainPtr(PartyCurrentClass)[0] = MemGetMainPtr(PartyClass)[member];
        }
        ImGui::SameLine();
        if (MemGetMainPtr(PartyLeader)[0] == member) ImGui::TextDisabled("Current leader");
    }

    if (ImGui::CollapsingHeader("Progress and resources", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("Progress", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn(); editByte("Level", PartyLevel, 1, 255);
            ImGui::TableNextColumn(); editByte("Levels waiting", PartyLevelPlus, 0, 2);
            ImGui::TableNextColumn(); editWord("Experience", PartyXpLow, PartyXpHigh, 65535);
            ImGui::TableNextColumn(); editWord("Health", PartyHealthLow, PartyHealthHigh, 65535);
            ImGui::TableNextColumn(); editWord("Maximum health", PartyHealthMaxLow,
                                                PartyHealthMaxHigh, 65535);
            ImGui::TableNextColumn(); editByte("Power", PartyPower, 0, 255);
            ImGui::TableNextColumn(); editByte("Maximum power", PartyPowerMax, 0, 255);
            ImGui::TableNextColumn(); editWord("Gold", PartyGoldLow, PartyGoldHigh, 10000);
            ImGui::TableNextColumn(); editByte("Food", PartyFood, 0, 100);
            ImGui::TableNextColumn(); editByte("Torches", PartyTorches, 0, 10);
            int armorClass = 10 - MemGetMainPtr(PartyArmorClass)[member];
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(105.0f);
            if (ImGui::InputInt("Armor class", &armorClass))
                MemGetMainPtr(PartyArmorClass)[member] = static_cast<BYTE>(
                    std::clamp(10 - armorClass, 0, 255));
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Attributes", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("Attributes", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn(); editByte("Strength", PartyStrength, 0, 255);
            ImGui::TableNextColumn(); editByte("Constitution", PartyConstitution, 0, 255);
            ImGui::TableNextColumn(); editByte("Size", PartySizeAttribute, 0, 255);
            ImGui::TableNextColumn(); editByte("Intelligence", PartyIntelligence, 0, 255);
            ImGui::TableNextColumn(); editByte("Dexterity", PartyDexterity, 0, 255);
            ImGui::TableNextColumn(); editByte("Charisma", PartyCharisma, 0, 255);
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen))
    {
        constexpr std::array<std::pair<BYTE, const char*>, 7> statuses = {{
            {0x02, "Starving"}, {0x04, "Toxified"}, {0x08, "Ill"},
            {0x10, "Paralyzed"}, {0x20, "Stone"}, {0x40, "Dead"}, {0x80, "Ashes"}
        }};
        BYTE status = MemGetMainPtr(PartyStatus)[member];
        for (const auto& [mask, label] : statuses)
        {
            bool enabled = (status & mask) != 0;
            if (ImGui::Checkbox(label, &enabled))
            {
                if (enabled) status |= mask;
                else status &= static_cast<BYTE>(~mask);
                MemGetMainPtr(PartyStatus)[member] = status;
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }

    if (ImGui::CollapsingHeader("Inventory", ImGuiTreeNodeFlags_DefaultOpen))
    {
        BYTE* inventory = MemGetMainPtr(
            static_cast<std::uint16_t>(PartyInventory + member * 0x20));
        if (ImGui::BeginTable("InventoryTable", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                              | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Slot");
            ImGui::TableSetupColumn("Item ID");
            ImGui::TableSetupColumn("Item");
            ImGui::TableSetupColumn("Charges");
            ImGui::TableHeadersRow();
            for (int slot = 0; slot < 8; ++slot)
            {
                ImGui::PushID(slot);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(inventoryNames[slot]);
                int item = inventory[slot];
                ImGui::TableNextColumn();
                if (ImGui::InputInt("##Item", &item))
                {
                    inventory[slot] = static_cast<BYTE>(std::clamp(item, 0, 255));
                    recalculateArmor = true;
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(inventory[slot] == 0xFF ? "Empty"
                    : state.inventoryRules.Name(inventory[slot], true).c_str());
                int charges = inventory[slot + 8];
                ImGui::TableNextColumn();
                if (ImGui::InputInt("##Charges", &charges))
                    inventory[slot + 8] = static_cast<BYTE>(std::clamp(charges, 0, 255));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        constexpr std::array<const char*, 3> readyNames = {"Melee", "Ranged", "Fists"};
        BYTE readyStored = MemGetMainPtr(PartyWeaponReady)[member];
        int ready = readyStored < 2 ? readyStored : 2;
        if (ImGui::BeginCombo("Weapon in hands", readyNames[ready]))
        {
            for (int index = 0; index < 3; ++index)
            {
                if (ImGui::Selectable(readyNames[index], ready == index))
                {
                    ready = index;
                    MemGetMainPtr(PartyWeaponReady)[member] = index < 2
                        ? static_cast<BYTE>(index) : 0xFF;
                    recalculateArmor = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    if (recalculateArmor) RecalculateArmorClasses();
    ImGui::PopID();
    ImGui::End();
}

void RenderAppleWindow(AppState& state)
{
    Video& video = GetVideo();
    state.renderer.UploadFramebuffer(video.GetFrameBuffer(),
                                     static_cast<int>(video.GetFrameBufferWidth()),
                                     static_cast<int>(video.GetFrameBufferHeight()));

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    const bool activeParty = dlrl::PartyTransfer::HasActiveParty(
        state.hooks.State().inGameMap || state.hooks.State().inBattle || state.uiFixture);

    // With auto-hide on, the bar only appears while the pointer sits along the
    // top edge or a menu popup is open, so an open menu never vanishes while
    // the mouse travels down into it.
    bool showMenuBar = true;
    if (state.autoHideMenuBar)
    {
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        const ImVec2 mouse = ImGui::GetMousePos();
        const float activation = std::max(ImGui::GetFrameHeight(), 24.0f);
        const bool mouseAtTop = mouse.x >= mainViewport->Pos.x
            && mouse.x <= mainViewport->Pos.x + mainViewport->Size.x
            && mouse.y >= mainViewport->Pos.y
            && mouse.y <= mainViewport->Pos.y + activation;
        showMenuBar = mouseAtTop
            || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId
                                          | ImGuiPopupFlags_AnyPopupLevel);
    }
    if (showMenuBar && ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open DLRL HDV...")) OpenHdvDialog(state);
            ImGui::Separator();
            if (ImGui::MenuItem("Start a clean new game")) state.newGameRequested = true;
            if (ImGui::MenuItem("Export active party...", nullptr, false, activeParty))
                OpenPartyExportDialog(state);
            if (ImGui::MenuItem("Import party...", nullptr, false, activeParty))
                OpenPartyImportDialog(state);
            if (ImGui::MenuItem("Party editor", nullptr,
                                state.showPartyEditor, activeParty))
                state.showPartyEditor = !state.showPartyEditor;
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Cmd/Ctrl-Q"))
            {
                SDL_Event quit{};
                quit.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&quit);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulator"))
        {
            if (ImGui::BeginMenu("Video"))
            {
                for (int i = 0; i < static_cast<int>(std::size(kVideoPresets)); ++i)
                {
                    if (ImGui::MenuItem(kVideoPresets[i].label, nullptr,
                                        state.videoIndex == i))
                    {
                        state.videoIndex = i;
                        ApplyVideo(state);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("CRT post-processing", nullptr,
                                    state.postprocessorEnabled))
                {
                    state.postprocessorEnabled = !state.postprocessorEnabled;
                    sa2::PostProcessor::GetInstance()->SetActive(state.postprocessorEnabled);
                }
                if (ImGui::MenuItem("Post-processor settings"))
                    sa2::PostProcessor::GetInstance()->bImguiWindowIsOpen = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Volume"))
            {
                if (ImGui::BeginMenu("Speaker"))
                {
                    for (int i = 0; i < static_cast<int>(std::size(kVolumeLabels)); ++i)
                    {
                        if (ImGui::MenuItem(kVolumeLabels[i], nullptr,
                                            state.speakerVolume == i))
                        {
                            state.speakerVolume = i;
                            ApplyVolume(state);
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Mockingboard"))
                {
                    for (int i = 0; i < static_cast<int>(std::size(kVolumeLabels)); ++i)
                    {
                        if (ImGui::MenuItem(kVolumeLabels[i], nullptr,
                                            state.mockingboardVolume == i))
                        {
                            state.mockingboardVolume = i;
                            ApplyVolume(state);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Interface color"))
            {
                constexpr const char* labels[] = { "White", "Green", "Amber" };
                for (int i = 0; i < 3; ++i)
                {
                    if (ImGui::MenuItem(labels[i], nullptr, state.interfaceColor == i))
                    {
                        state.interfaceColor = i;
                        dlrl::ApplyAppleStyle(static_cast<dlrl::InterfaceColor>(i));
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(state.paused ? "Resume" : "Pause", "Cmd/Ctrl+P"))
                SetPaused(state, !state.paused);
            if (ImGui::MenuItem("Reboot", "Cmd/Alt+R")) state.rebootRequested = true;
            if (ImGui::MenuItem("Fullscreen", "Alt+Enter", state.fullscreen))
                SetFullscreen(state, !state.fullscreen);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Relorded"))
        {
            ImGui::MenuItem("Inventory", "Insert", &state.showInventory,
                            state.hooks.State().inGameMap || state.uiFixture);
            ImGui::MenuItem("Original Interface", "F11", &state.showAppleVideoInGame);
            if (ImGui::BeginMenu("Original Interface Darkness"))
            {
                ImGui::SetNextItemWidth(180.0f);
                ImGui::SliderInt("##OriginalInterfaceDarkness",
                                 &state.originalInterfaceOpacity, 0, 100, "%d%%");
                ImGui::EndMenu();
            }
            ImGui::MenuItem("English Translation", "F10", &state.englishNames);
            ImGui::MenuItem("Auto-Hide Menu Bar", nullptr, &state.autoHideMenuBar);
            ImGui::Separator();
            if (ImGui::BeginMenu("Map"))
            {
                if (ImGui::MenuItem("Display Centered / Full", "F1",
                                    state.mapViewMode == dlrl::MapViewMode::FollowPlayer
                                    || state.mapViewMode == dlrl::MapViewMode::Full))
                {
                    state.mapViewMode = state.mapViewMode == dlrl::MapViewMode::FollowPlayer
                                      ? dlrl::MapViewMode::Full
                                      : dlrl::MapViewMode::FollowPlayer;
                }
                if (ImGui::MenuItem("Display Top Left Quadrant", "F2",
                                    state.mapViewMode == dlrl::MapViewMode::TopLeft))
                    state.mapViewMode = dlrl::MapViewMode::TopLeft;
                if (ImGui::MenuItem("Display Top Right Quadrant", "F3",
                                    state.mapViewMode == dlrl::MapViewMode::TopRight))
                    state.mapViewMode = dlrl::MapViewMode::TopRight;
                if (ImGui::MenuItem("Display Bottom Left Quadrant", "F4",
                                    state.mapViewMode == dlrl::MapViewMode::BottomLeft))
                    state.mapViewMode = dlrl::MapViewMode::BottomLeft;
                if (ImGui::MenuItem("Display Bottom Right Quadrant", "F5",
                                    state.mapViewMode == dlrl::MapViewMode::BottomRight))
                    state.mapViewMode = dlrl::MapViewMode::BottomRight;
                ImGui::EndMenu();
            }
            ImGui::MenuItem("Spell Window", "Alt-S", &state.showSpells);
            ImGui::MenuItem("Log Window", "Alt-L", &state.showLog);
            ImGui::Separator();
            if (ImGui::BeginMenu("Hacks / Relorded Changes"))
            {
                auto& changes = state.hooks.Changes();
                ImGui::MenuItem("Fair combat XP allocation", nullptr, &changes.xpReallocation);
                ImGui::MenuItem("Exit pits with movement keys", nullptr, &changes.exitPitByMoving);
                ImGui::MenuItem("Freeze time while idle", nullptr, &changes.freezeTimeWhenIdle);
                ImGui::MenuItem("Rear rank ranged attacks", nullptr, &changes.rangedAttackForRearLine);
                ImGui::MenuItem("Search always succeeds", nullptr, &changes.searchAlwaysSucceeds);
                ImGui::MenuItem("Prevent level drain", nullptr, &changes.noLevelDrain);
                ImGui::MenuItem("Magic water raises stats", nullptr, &changes.magicWaterIncreasesStats);
                ImGui::MenuItem("Remove stat ceiling", nullptr, &changes.noStatsLimit);
                ImGui::MenuItem("No starvation HP loss", nullptr, &changes.noHpLossFromStarvation);
                ImGui::MenuItem("Extra race/class bonuses", nullptr, &changes.extraRaceAndClassBonuses);
                ImGui::MenuItem("No autosave after death", nullptr, &changes.noAutosaveAfterDeath);
                ImGui::MenuItem("Expanded weapon use", nullptr, &changes.expandedWeaponUse);
                ImGui::MenuItem("Keep surplus XP on level-up", nullptr, &changes.keepExtraXpOnLevel);
                ImGui::MenuItem("Distribute purchased food", nullptr, &changes.distributeFood);
                ImGui::MenuItem("Fix gold pooling", nullptr, &changes.fixGoldPooling);
                ImGui::MenuItem("Distribute battle gold", nullptr, &changes.distributeGold);
                ImGui::Separator();
                if (ImGui::MenuItem("Enable all fixes")) changes = {};
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About Deathlord Relorded 3.0", "Alt-?"))
                state.showAbout = true;
            ImGui::EndMenu();
        }
        const std::string hdvLabel = state.temporaryHdv.empty()
            ? state.hdvPath.filename().string()
            : std::string("Deathlord PRODOS.hdv (test copy)");
        ImGui::TextDisabled("HDV: %s", hdvLabel.c_str());
        ImGui::EndMainMenuBar();
    }

    const auto& hookState = state.hooks.State();
    const bool fixtureLoading = state.uiFixture
                             && state.uiFixtureMode == UiFixtureMode::Loading;
    const bool fixtureGameOver = state.uiFixture
                              && state.uiFixtureMode == UiFixtureMode::GameOver;
    const bool fixtureBattle = state.uiFixture
                            && state.uiFixtureMode == UiFixtureMode::Battle;
    const bool fixtureInventory = state.uiFixture
                               && state.uiFixtureMode == UiFixtureMode::Inventory;
    const bool modernPresentation = hookState.inGameMap || hookState.inBattle
                                 || hookState.inTransition || hookState.dead
                                 || state.showInventory || state.startupSplash
                                 || state.uiFixture;
    if (modernPresentation)
    {
        unsigned int appleVideoTexture = state.renderer.FramebufferTexId();
        auto* pp = sa2::PostProcessor::GetInstance();
        if (state.showAppleVideoInGame && pp->IsActive())
        {
            // Process the 600:420 Apple framebuffer before ImGui embeds it in
            // the Relorded canvas, matching the standalone Apple //e window.
            constexpr int originalInterfaceWidth = 896;
            constexpr int originalInterfaceHeight = 627;
            pp->Render(originalInterfaceWidth, originalInterfaceHeight,
                       state.renderer.FramebufferTexId(),
                       static_cast<uint32_t>(video.GetFrameBufferWidth()),
                       static_cast<uint32_t>(video.GetFrameBufferHeight()));
            appleVideoTexture = pp->GetTextureId();
            if (state.smokeFrames > 0 && state.presentedFrames == 0)
                std::puts("DLRL postprocessor: modern original interface texture active");
        }
        state.modernUi.Render(appleVideoTexture,
                              state.showAppleVideoInGame,
                              state.originalInterfaceOpacity, state.paused,
                              state.mapViewMode, state.englishNames,
                              hookState.inBattle || fixtureBattle,
                              state.showInventory || fixtureInventory,
                              state.startupSplash || fixtureLoading,
                              state.startupSplashReady || fixtureLoading,
                              hookState.dead || fixtureGameOver);
        if (state.modernUi.ConsumeInventoryChanged()) RecalculateArmorClasses();
    }
    else
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 appleWindowSize(kAppleDisplayWidth + 18.0f,
                                     kAppleDisplayHeight + kAppleHintAreaHeight + 34.0f);
        ImGui::SetNextWindowSize(appleWindowSize, ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Apple //e", nullptr, ImGuiWindowFlags_NoDocking))
        {
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const std::string hint = StartMenuHint(state);
            const ImVec2 cursor = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(
                cursor.x + (available.x - kAppleDisplayWidth) * 0.5f, cursor.y));
            const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
            auto* pp = sa2::PostProcessor::GetInstance();
            ImTextureID texture = static_cast<ImTextureID>(state.renderer.FramebufferTexId());
            if (pp->IsActive())
            {
                pp->Render(static_cast<int>(kAppleDisplayWidth),
                           static_cast<int>(kAppleDisplayHeight),
                           state.renderer.FramebufferTexId(),
                           static_cast<uint32_t>(video.GetFrameBufferWidth()),
                           static_cast<uint32_t>(video.GetFrameBufferHeight()));
                texture = static_cast<ImTextureID>(pp->GetTextureId());
            }
            const float sourceWidth = static_cast<float>(video.GetFrameBufferWidth());
            const float sourceHeight = static_cast<float>(video.GetFrameBufferHeight());
            const float borderX = video.GetFrameBufferBorderWidth() / sourceWidth;
            const float borderY = video.GetFrameBufferBorderHeight() / sourceHeight;
            ImGui::Image(texture, ImVec2(kAppleDisplayWidth, kAppleDisplayHeight),
                         ImVec2(borderX, 1.0f - borderY),
                         ImVec2(1.0f - borderX, borderY));
            if (!hint.empty())
                state.modernUi.RenderHostHint(
                    hint, imagePosition.x + kAppleDisplayWidth * 0.5f,
                    imagePosition.y + kAppleDisplayHeight + 6.0f,
                    kAppleDisplayWidth);
        }
        ImGui::End();
    }

    state.modernUi.RenderSpellWindow(&state.showSpells);
    state.modernUi.RenderLogWindow(&state.showLog);
    RenderPartyEditor(state, activeParty);
    sa2::PostProcessor::GetInstance()->RenderImGuiWindow();

    if (state.showAbout)
    {
        ImGui::OpenPopup("About Deathlord Relorded 3.0");
        state.showAbout = false;
    }
    if (ImGui::BeginPopupModal("About Deathlord Relorded 3.0", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Deathlord Relorded 3.0");
        ImGui::TextUnformatted("Portable AppleWin core, SDL3, OpenGL, and Dear ImGui.");
        ImGui::Separator();
        ImGui::TextUnformatted("Relorded by Rikkles. Deathlord (c) 1987 Al Escudero and David Wong.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (state.rebootRequested)
    {
        ImGui::OpenPopup("Reboot Deathlord?");
        state.rebootRequested = false;
    }
    if (ImGui::BeginPopupModal("Reboot Deathlord?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Deathlord hates you. Power-cycle the Apple //e?");
        if (ImGui::Button("Reboot"))
        {
            RebootEmulator(state);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (state.newGameRequested)
    {
        ImGui::OpenPopup("Start a clean new game?");
        state.newGameRequested = false;
    }
    if (ImGui::BeginPopupModal("Start a clean new game?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 620.0f);
        ImGui::TextUnformatted(
            "Starting a new game overwrites completely the existing hard disk image. "
            "You will lose all characters, active or inactive, and the game will reset "
            "from a brand new state. Are you sure that's what you want?");
        ImGui::PopTextWrapPos();
        ImGui::Separator();
        if (ImGui::Button("Yes"))
        {
            std::string error;
            if (!StartCleanNewGame(state, error))
            {
                state.operationError = error;
                state.operationErrorRequested = true;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (state.importConfirmationRequested)
    {
        ImGui::OpenPopup("Import party?");
        state.importConfirmationRequested = false;
    }
    if (ImGui::BeginPopupModal("Import party?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (state.partyToImport)
        {
            const std::string prompt = "Are you sure you want to overwrite the current party "
                "with the party called " + state.partyToImport->partyName + ", exported on "
                + state.partyToImport->exportedAt + "? This cannot be undone!";
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 620.0f);
            ImGui::TextUnformatted(prompt.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Separator();
            if (ImGui::Button("Yes"))
            {
                if (activeParty)
                {
                    dlrl::PartyTransfer::Apply(*state.partyToImport);
                    state.operationNotice = "The party was imported successfully.";
                    state.operationNoticeRequested = true;
                }
                else
                {
                    state.operationError = "There is no active party to overwrite.";
                    state.operationErrorRequested = true;
                }
                state.partyToImport.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No"))
            {
                state.partyToImport.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            ImGui::TextUnformatted("No party is waiting to be imported.");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (state.operationErrorRequested)
    {
        ImGui::OpenPopup("Operation failed");
        state.operationErrorRequested = false;
    }
    if (ImGui::BeginPopupModal("Operation failed", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("%s", state.operationError.c_str());
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (state.operationNoticeRequested)
    {
        ImGui::OpenPopup("Party operation complete");
        state.operationNoticeRequested = false;
    }
    if (ImGui::BeginPopupModal("Party operation complete", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(state.operationNotice.c_str());
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
    auto state = std::make_unique<AppState>();
    std::filesystem::path requestedHdv;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--hdv" && i + 1 < argc) requestedHdv = argv[++i];
        else if (arg == "--smoke-frames" && i + 1 < argc) state->smokeFrames = std::stoi(argv[++i]);
        else if (arg == "--capture" && i + 1 < argc) state->capturePath = argv[++i];
        else if (arg == "--postprocess") state->postprocessorEnabled = true;
        else if (arg == "--show-original-interface") state->showAppleVideoInGame = true;
        else if (arg == "--fullscreen") state->fullscreen = true;
        else if (arg == "--auto-hide-menu") state->autoHideMenuBar = true;
        else if (arg == "--fixture-mouse" && i + 1 < argc)
        {
            const std::string position = argv[++i];
            const auto separator = position.find('x');
            if (separator != std::string::npos)
            {
                state->fixtureMouseX = std::stoi(position.substr(0, separator));
                state->fixtureMouseY = std::stoi(position.substr(separator + 1));
            }
        }
        else if (arg == "--show-spells") state->showSpells = true;
        else if (arg == "--show-log") state->showLog = true;
        else if (arg == "--show-party-editor") state->showPartyEditor = true;
        else if (arg == "--show-new-game-confirm") state->newGameRequested = true;
        else if (arg == "--show-import-confirmation")
            state->fixtureImportConfirmation = true;
        else if (arg == "--test-clean-reset") state->testCleanReset = true;
        else if (arg == "--ui-fixture") state->uiFixture = true;
        else if (arg == "--key-event" && i + 1 < argc)
        {
            const std::string event = argv[++i];
            const auto separator = event.find(':');
            if (separator == std::string::npos || separator + 1 >= event.size())
            {
                std::fprintf(stderr, "Invalid key event: %s\n", event.c_str());
                return SDL_APP_FAILURE;
            }
            const std::string key = event.substr(separator + 1);
            const BYTE value = key == "SPACE" ? static_cast<BYTE>(' ')
                                                : static_cast<BYTE>(key.front());
            state->scheduledKeys.push_back({std::stoi(event.substr(0, separator)), value});
        }
        else if (arg == "--ui-fixture-mode" && i + 1 < argc)
        {
            state->uiFixture = true;
            const std::string mode = argv[++i];
            if (mode == "battle") state->uiFixtureMode = UiFixtureMode::Battle;
            else if (mode == "inventory") state->uiFixtureMode = UiFixtureMode::Inventory;
            else if (mode == "loading") state->uiFixtureMode = UiFixtureMode::Loading;
            else if (mode == "gameover") state->uiFixtureMode = UiFixtureMode::GameOver;
            else if (mode != "gameplay")
            {
                std::fprintf(stderr, "Unknown UI fixture mode: %s\n", mode.c_str());
                return SDL_APP_FAILURE;
            }
        }
        else if (arg == "--window-size" && i + 1 < argc)
        {
            const std::string size = argv[++i];
            const auto separator = size.find('x');
            if (separator != std::string::npos)
            {
                state->windowWidth = std::max(640, std::stoi(size.substr(0, separator)));
                state->windowHeight = std::max(480, std::stoi(size.substr(separator + 1)));
            }
        }
    }

    if (state->smokeFrames > 0) SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetAppMetadata("Deathlord Relorded", "3.0.0", "com.rikkles.deathlordrelorded");

    if (char* pref = SDL_GetPrefPath("Rikkles", "DeathlordRelorded"))
    {
        state->prefDir = pref;
        state->imguiIniPath = state->prefDir + "imgui.ini";
        SDL_free(pref);
    }
    if (state->smokeFrames == 0) LoadHostSettings(*state);
    if (requestedHdv.empty() && !state->hdvPath.empty()) requestedHdv = state->hdvPath;
    state->hdvPath = ResolveHdv(*state, requestedHdv);
    dlrl::Frame::SetResourceDir(FindResourcesDir());
    if (!state->inventoryRules.Load(FindDlrlAssetsDir() / "InventoryList.csv"))
        std::fprintf(stderr, "Unable to load Relorded inventory rules\n");
    state->hooks.SetCanEquipCallback(dlrl::InventoryRules::HookCanEquip,
                                    &state->inventoryRules);
    state->hooks.SetEventCallback(ReceiveDlrlEvent, state.get());

    if (!state->renderer.Init("Deathlord Relorded 3.0",
                              state->windowWidth, state->windowHeight))
        return SDL_APP_FAILURE;
    if (state->smokeFrames == 0 && !state->imguiIniPath.empty())
        ImGui::GetIO().IniFilename = state->imguiIniPath.c_str();
    else
        ImGui::GetIO().IniFilename = nullptr;
    if (!PP_InitGL(reinterpret_cast<PP_GL_GetProcAddr>(SDL_GL_GetProcAddress)))
    {
        std::fprintf(stderr, "Unable to initialize post-processor OpenGL functions\n");
        return SDL_APP_FAILURE;
    }
    if (!state->modernUi.Initialize(FindDlrlAssetsDir(), FindPortableAssetsDir(),
                                    &state->inventoryRules))
        std::fprintf(stderr, "Unable to initialize the modern DLRL renderer\n");
    if (state->smokeFrames == 0 && !state->prefDir.empty())
        state->modernUi.SetFogOfWarPath(
            std::filesystem::path(state->prefDir) / "fogofwar.json");
    if (state->smokeFrames == 0 && !state->prefDir.empty())
    {
        const auto ppState = std::filesystem::path(state->prefDir) / kPostprocessorFilename;
        if (std::filesystem::exists(ppState))
        {
            try { sa2::PostProcessor::GetInstance()->LoadState(ppState.string()); }
            catch (const std::exception& error)
            {
                std::fprintf(stderr, "Unable to load post-processor state: %s\n", error.what());
            }
        }
    }
    sa2::PostProcessor::GetInstance()->SetActive(state->postprocessorEnabled);
    dlrl::ApplyAppleStyle(static_cast<dlrl::InterfaceColor>(state->interfaceColor));
    if (state->smokeFrames > 0) SDL_GL_SetSwapInterval(0);
    if (!InitEmulator(*state, state->hdvPath)) return SDL_APP_FAILURE;
    if (state->testCleanReset)
    {
        std::string error;
        if (!StartCleanNewGame(*state, error))
        {
            std::fprintf(stderr, "Clean-new-game integration failed: %s\n", error.c_str());
            return SDL_APP_FAILURE;
        }
        std::puts("DLRL clean new game: active HDV replaced and rebooted");
    }
    if (state->uiFixture) SeedModernUiFixture(*state);
    if (state->uiFixture && state->fixtureImportConfirmation)
    {
        state->partyToImport = dlrl::PartyTransfer::Capture("2026-07-12T12:34:56Z");
        state->importConfirmationRequested = true;
    }
    ApplySpeed(*state);
    ApplyVideo(*state);
    ApplyVolume(*state);
    if (state->fullscreen) SetFullscreen(*state, true);
    if (state->fixtureMouseX >= 0 && state->fixtureMouseY >= 0)
        SDL_WarpMouseInWindow(state->renderer.Window(),
                              static_cast<float>(state->fixtureMouseX),
                              static_cast<float>(state->fixtureMouseY));
    if (state->hdvPath.empty() && !state->uiFixture) OpenHdvDialog(*state);

    state->lastTicksNs = SDL_GetTicksNS();
    *appstate = state.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    auto& state = *static_cast<AppState*>(appstate);
    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    if (event->type != SDL_EVENT_KEY_DOWN) return SDL_APP_CONTINUE;
    if (ImGui::GetIO().WantTextInput) return SDL_APP_CONTINUE;

    if ((event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)
        && (event->key.mod & SDL_KMOD_ALT))
    {
        SetFullscreen(state, !state.fullscreen);
        return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_P && (event->key.mod & kPrimaryModifier))
    {
        SetPaused(state, !state.paused);
        return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_R && (event->key.mod & kRebootModifier))
    {
        state.rebootRequested = true;
        return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_Q && (event->key.mod & kPrimaryModifier))
        return SDL_APP_SUCCESS;
    if (state.startupSplash)
    {
        if (event->key.key == SDLK_SPACE) DismissStartupSplash(state);
        return SDL_APP_CONTINUE;
    }
    if ((event->key.key == SDLK_INSERT
         || (event->key.key == SDLK_I && (event->key.mod & kPrimaryModifier)))
        && (state.hooks.State().inGameMap || state.uiFixture))
    {
        state.showInventory = !state.showInventory;
        return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_F1)
    {
        state.mapViewMode = state.mapViewMode == dlrl::MapViewMode::FollowPlayer
                          ? dlrl::MapViewMode::Full
                          : dlrl::MapViewMode::FollowPlayer;
        return SDL_APP_CONTINUE;
    }
    if (event->key.key >= SDLK_F2 && event->key.key <= SDLK_F5)
    {
        constexpr std::array<dlrl::MapViewMode, 4> modes = {
            dlrl::MapViewMode::TopLeft, dlrl::MapViewMode::TopRight,
            dlrl::MapViewMode::BottomLeft, dlrl::MapViewMode::BottomRight
        };
        state.mapViewMode = modes[event->key.key - SDLK_F2];
        return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_F10)
    {
        state.englishNames = !state.englishNames;
        return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_F11)
    {
        state.showAppleVideoInGame = !state.showAppleVideoInGame;
        return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_S && (event->key.mod & SDL_KMOD_ALT))
    {
        state.showSpells = !state.showSpells;
        return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_L && (event->key.mod & SDL_KMOD_ALT))
    {
        state.showLog = !state.showLog;
        return SDL_APP_CONTINUE;
    }
    if (state.showInventory || (state.uiFixture
        && state.uiFixtureMode == UiFixtureMode::Inventory))
    {
        if (event->key.key == SDLK_ESCAPE) state.showInventory = false;
        return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_CAPSLOCK)
    {
        KeybToggleCapsLock();
        return SDL_APP_CONTINUE;
    }

    KeybUpdateCtrlShiftStatus();
    if (const BYTE ascii = SdlKeyToAscii(event->key.key, event->key.mod))
    {
        const BYTE plain = ascii & 0x7F;
        if ((plain == 'P' || plain == 'p')
            && state.hooks.State().startMenu == dlrl::StartMenuState::Menu)
            BeginPlayLoadingAcceleration(state);
        KeybQueueKeypress(ascii, ASCII);
    }
    else if (const WPARAM key = SdlKeyToVirtualKey(event->key.key))
        KeybQueueKeypress(key, NOT_ASCII);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    auto& state = *static_cast<AppState*>(appstate);
    {
        std::filesystem::path pending;
        {
            std::lock_guard<std::mutex> lock(state.pendingHdvMutex);
            std::swap(pending, state.pendingHdv);
        }
        if (!pending.empty()) LoadHdv(state, pending);
    }
    {
        std::filesystem::path exportPath;
        std::filesystem::path importPath;
        {
            std::lock_guard<std::mutex> lock(state.pendingPartyDialogMutex);
            std::swap(exportPath, state.pendingPartyExport);
            std::swap(importPath, state.pendingPartyImport);
        }
        const bool activeParty = dlrl::PartyTransfer::HasActiveParty(
            state.hooks.State().inGameMap || state.hooks.State().inBattle || state.uiFixture);
        if (!exportPath.empty())
        {
            if (exportPath.extension() != ".json") exportPath += ".dlrl-party.json";
            if (!activeParty)
            {
                state.operationError = "There is no active party to export.";
                state.operationErrorRequested = true;
            }
            else
            {
                std::string error;
                const dlrl::PartySnapshot snapshot = dlrl::PartyTransfer::Capture();
                if (dlrl::PartyTransfer::Save(snapshot, exportPath, error))
                {
                    state.operationNotice = "The party was exported to " + exportPath.string();
                    state.operationNoticeRequested = true;
                }
                else
                {
                    state.operationError = error;
                    state.operationErrorRequested = true;
                }
            }
        }
        if (!importPath.empty())
        {
            dlrl::PartySnapshot snapshot;
            std::string error;
            if (dlrl::PartyTransfer::Load(importPath, snapshot, error))
            {
                state.partyToImport = std::move(snapshot);
                state.importConfirmationRequested = true;
            }
            else
            {
                state.operationError = error;
                state.operationErrorRequested = true;
            }
        }
    }
    for (const AppState::ScheduledKey& event : state.scheduledKeys)
        if (event.frame == state.presentedFrames)
        {
            const BYTE plain = event.key & 0x7F;
            if (state.startupSplash)
            {
                if (plain == ' ') DismissStartupSplash(state);
                continue;
            }
            if ((plain == 'P' || plain == 'p')
                && state.hooks.State().startMenu == dlrl::StartMenuState::Menu)
                BeginPlayLoadingAcceleration(state);
            KeybQueueKeypress(event.key, ASCII);
        }
    RunEmulator(state);
    if (state.playLoadingAcceleration && state.hooks.State().inGameMap)
    {
        state.playLoadingAcceleration = false;
        state.requestedSpeedIndex = 1;
        std::puts("DLRL internal speed: main game ready at 1x");
        std::fflush(stdout);
    }
    if (state.requestedSpeedIndex >= 0)
    {
        state.speedIndex = state.requestedSpeedIndex;
        state.requestedSpeedIndex = -1;
        ApplySpeed(state);
    }
    if (state.startupSplash && state.startupSplashReady && !state.paused)
        SetPaused(state, true);

    state.renderer.BeginFrame();
    if (state.hooks.State().inGameMap)
        state.modernUi.UpdateMapTexture(state.hooks.State().inBattle,
                                        state.hooks.State().inTransition,
                                        state.hooks.Changes().extraRaceAndClassBonuses);
    state.renderer.BeginImGui();
    RenderAppleWindow(state);
    state.renderer.EndImGui();

    ++state.presentedFrames;
    if (state.smokeFrames > 0 && state.presentedFrames >= state.smokeFrames)
    {
        if (!state.capturePath.empty())
        {
            std::filesystem::create_directories(state.capturePath.parent_path().empty()
                                                    ? std::filesystem::path(".")
                                                    : state.capturePath.parent_path());
            if (!state.renderer.CaptureBackbufferBmp(state.capturePath.string().c_str()))
                std::fprintf(stderr, "Unable to capture host window\n");
            else
                std::printf("SDL host capture: frames=%d pc=$%04X output=%s\n",
                            state.presentedFrames, regs.pc, state.capturePath.string().c_str());
        }
        state.renderer.EndFrame();
        return SDL_APP_SUCCESS;
    }

    state.renderer.EndFrame();
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult)
{
    auto* state = static_cast<AppState*>(appstate);
    if (state)
    {
        SaveSettings(*state);
        state->modernUi.SaveFogOfWar();
        ShutdownEmulator(*state);
        state->modernUi.Shutdown();
        state->renderer.Shutdown();
        if (!state->temporaryHdv.empty())
        {
            std::error_code ignored;
            std::filesystem::remove(state->temporaryHdv, ignored);
        }
        delete state;
    }
    SDL_Quit();
}
