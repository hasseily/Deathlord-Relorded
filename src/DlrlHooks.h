#pragma once

#include "Emulator/StdAfx.h"

#include "DlrlData.h"
#include "Emulator/CPU.h"

#include <cstdint>

namespace dlrl
{

struct RelordedChanges
{
    bool xpReallocation = true;
    bool exitPitByMoving = true;
    bool freezeTimeWhenIdle = true;
    bool rangedAttackForRearLine = true;
    bool searchAlwaysSucceeds = true;
    bool noLevelDrain = true;
    bool magicWaterIncreasesStats = true;
    bool noStatsLimit = true;
    bool noHpLossFromStarvation = true;
    bool extraRaceAndClassBonuses = true;
    bool noAutosaveAfterDeath = true;
    bool expandedWeaponUse = true;
    bool keepExtraXpOnLevel = true;
    bool distributeFood = true;
    bool fixGoldPooling = true;
    bool distributeGold = true;
};

enum class StartMenuState
{
    Other,
    Title,
    Menu,
    AttributesRerollPropose,
    AttributesRerolling,
    AttributesRerollCancelled,
    AttributesRerollDone,
};

enum class HookEventType
{
    MapTransition,
    MapMayNeedSave,
    RecalculateTileVisibility,
    BattleStarted,
    BattleEnded,
    ActiveActor,
    ActorAttacks,
    ActorDodges,
    ActorHit,
    ActorKilled,
    ActorHealed,
    EnemyType,
    EnemyHpReady,
    AttributeIncreased,
    LevelUpAvailable,
    MissingXp,
    ScrollText,
    PrintCharacter,
    InverseTextLine,
    ClearText,
    AllCharactersDead,
    EndCredits,
    StartupSplash,
    RequestSpeed,
};

struct HookEvent
{
    HookEventType type{};
    std::uint16_t pc = 0;
    int actor = -1;
    int value = 0;
    int auxiliary = 0;
};

using HookEventCallback = void (*)(const HookEvent&, void* userData);
using CanEquipCallback = bool (*)(std::uint8_t item, std::uint8_t characterClass,
                                  std::uint8_t race, void* userData);

struct DlrlRuntimeState
{
    bool inGameMap = false;
    bool inBattle = false;
    bool inTransition = false;
    bool dead = false;
    bool endCredits = false;
    bool hasBeenIdle = false;
    bool shouldRecalculateTiles = false;
    StartMenuState startMenu = StartMenuState::Other;
    std::uint32_t rerollCount = 0;
    std::uint64_t hookCalls = 0;
};

class DlrlHooks
{
public:
    DlrlHooks() = default;
    ~DlrlHooks();

    DlrlHooks(const DlrlHooks&) = delete;
    DlrlHooks& operator=(const DlrlHooks&) = delete;

    void Attach();
    void Detach();
    void ResetRuntime();

    RelordedChanges& Changes() { return changes_; }
    const RelordedChanges& Changes() const { return changes_; }
    const DlrlRuntimeState& State() const { return state_; }

    void SetEventCallback(HookEventCallback callback, void* userData = nullptr);
    void SetCanEquipCallback(CanEquipCallback callback, void* userData = nullptr);
    void SetRandomSeed(std::uint32_t seed);

    CpuInstructionHookResult HandleInstruction(std::uint16_t pc);

private:
    static CpuInstructionHookResult InstructionThunk(WORD pc, void* userData);
    void Emit(HookEventType type, std::uint16_t pc, int actor = -1,
              int value = 0, int auxiliary = 0) const;
    std::uint32_t NextRandom();

    RelordedChanges changes_{};
    DlrlRuntimeState state_{};
    HookEventCallback eventCallback_ = nullptr;
    void* eventUserData_ = nullptr;
    CanEquipCallback canEquipCallback_ = nullptr;
    void* canEquipUserData_ = nullptr;
    std::uint32_t randomState_ = 0xD34D10ADu;
    bool attached_ = false;
};

} // namespace dlrl
