#include "Frame.h"
#include "DlrlData.h"
#include "DlrlHooks.h"
#include "InventoryRules.h"
#include "InventoryState.h"

#include "Emulator/CardManager.h"
#include "Emulator/Core.h"
#include "Emulator/CPU.h"
#include "Emulator/Interface.h"
#include "Emulator/Keyboard.h"
#include "Emulator/Memory.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace
{

int failures = 0;

void Check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

CpuInstructionHookResult BridgeProbe(WORD pc, void* userData)
{
    auto* calls = static_cast<int*>(userData);
    ++*calls;
    if (pc == 0x0200)
    {
        regs.a = 0x42;
        regs.pc = 0x0201;
        return {2, true};
    }
    return {};
}

struct EventProbe
{
    int count = 0;
    dlrl::HookEvent last{};
};

void ReceiveEvent(const dlrl::HookEvent& event, void* userData)
{
    auto& probe = *static_cast<EventProbe*>(userData);
    ++probe.count;
    probe.last = event;
}

int SumWords(std::uint16_t low, std::uint16_t high)
{
    int total = 0;
    for (int i = 0; i < dlrl::deathlord::PartySize; ++i)
        total += MemGetMainPtr(low)[i] | (MemGetMainPtr(high)[i] << 8);
    return total;
}

} // namespace

int main()
{
    using namespace dlrl;
    using namespace dlrl::deathlord;

    Frame::SetResourceDir(std::filesystem::path(DLRL_RESOURCE_DIR));
    g_nAppMode = MODE_RUNNING;
    SetApple2Type(A2TYPE_APPLE2EENHANCED);
    GetFrame().Initialize(true);
    MemInitialize();
    GetCardMgr().Reset(true);
    std::memset(MemGetMainPtr(0), 0, 0x10000);

    int bridgeCalls = 0;
    g_cpuInstructionHook = BridgeProbe;
    g_cpuInstructionHookUserData = &bridgeCalls;
    regs = {};
    regs.pc = 0x0200;
    regs.sp = 0x01FF;
    regs.ps = AF_RESERVED | AF_INTERRUPT;
    MemGetMainPtr(0x0200)[0] = 0xEA;
    const auto bridgeCycles = CpuExecute(1, false);
    Check(bridgeCycles == 2, "CPU accounts for replacement instruction cycles");
    Check(bridgeCalls == 1, "CPU invokes the portable instruction hook");
    Check(regs.a == 0x42 && regs.pc == 0x0201,
          "CPU hook can replace an instruction and mutate registers");
    g_cpuInstructionHook = nullptr;
    g_cpuInstructionHookUserData = nullptr;

    DlrlHooks hooks;
    InventoryRules inventoryRules;
    const auto inventoryCsv = std::filesystem::path(DLRL_RESOURCE_DIR).parent_path()
                            .parent_path() / "Assets" / "InventoryList.csv";
    Check(inventoryRules.Load(inventoryCsv), "portable inventory rules load from CSV");
    Check(inventoryRules.CanEquip(0x12, static_cast<BYTE>(CharacterClass::Peasant),
                                  static_cast<BYTE>(Race::Human)),
          "Relorded Crossbow rule permits a Peasant");
    Check(!inventoryRules.CanEquip(0x12, static_cast<BYTE>(CharacterClass::Priest),
                                   static_cast<BYTE>(Race::Human)),
          "Relorded Crossbow rule still rejects a Priest");
    Check(inventoryRules.Name(0x00) == "Dagger" && inventoryRules.Slot(0x00) == 0,
          "inventory catalog exposes the portable display name and slot");

    InventoryState inventoryState;
    for (int member = 0; member < PartySize; ++member)
    {
        BYTE* inventory = MemGetMainPtr(
            static_cast<WORD>(PartyInventory + member * 0x20));
        inventory[0] = inventory[8] = 0xFF;
    }
    BYTE* firstInventory = MemGetMainPtr(PartyInventory);
    BYTE* secondInventory = MemGetMainPtr(PartyInventory + 0x20);
    BYTE* thirdInventory = MemGetMainPtr(PartyInventory + 0x40);
    BYTE* fourthInventory = MemGetMainPtr(PartyInventory + 0x60);
    firstInventory[0] = 0x10;
    firstInventory[8] = 3;
    secondInventory[0] = 0x11;
    secondInventory[8] = 4;
    thirdInventory[0] = 0x12;
    thirdInventory[8] = 5;
    fourthInventory[0] = 0x13;
    fourthInventory[8] = 6;
    Check(inventoryState.MovePartyToStash(0, 0)
              && firstInventory[0] == 0xFF && firstInventory[8] == 0xFF,
          "inventory click moves an item and its charges into the memory-only stash");
    Check(inventoryState.MovePartyToStash(0, 1)
              && inventoryState.StashCount(0) == 2,
          "inventory stash holds exactly two items per category");
    Check(!inventoryState.MovePartyToStash(0, 2)
              && thirdInventory[0] == 0x12 && thirdInventory[8] == 5,
          "inventory rejects a third stashed item without losing it");
    Check(inventoryState.MoveToParty(0, PartySize, 2)
              && thirdInventory[0] == 0x10 && thirdInventory[8] == 3
              && inventoryState.Stashed(0, 0).item == 0x12
              && inventoryState.Stashed(0, 0).charges == 5,
          "stash-to-party click swaps both the item and its charges");
    Check(inventoryState.MoveToParty(0, 2, 3)
              && thirdInventory[0] == 0x13 && thirdInventory[8] == 6
              && fourthInventory[0] == 0x10 && fourthInventory[8] == 3,
          "party-to-party click preserves the v2 swap behavior");
    Check(inventoryState.DeleteStashed(0, 1)
              && inventoryState.StashCount(0) == 1,
          "trash click deletes only the selected stashed item");
    hooks.SetCanEquipCallback(InventoryRules::HookCanEquip, &inventoryRules);
    EventProbe events;
    hooks.SetEventCallback(ReceiveEvent, &events);
    hooks.SetRandomSeed(0x12345678);
    hooks.Attach();
    regs.a = 0;
    hooks.HandleInstruction(PcTitleKey);
    Check(events.last.type == HookEventType::RequestSpeed && events.last.value == 1,
          "title wait restores the hidden emulator speed to 1x");
    regs.a = 'P' | 0x80;
    hooks.HandleInstruction(PcMenuKey);
    Check(events.last.type == HookEventType::RequestSpeed && events.last.value == 6,
          "Play Game starts hidden loading acceleration");
    hooks.HandleInstruction(PcStartupSplash);
    Check(events.last.type == HookEventType::StartupSplash,
          "validated HDV pre-game flow requests the v2 Relorded splash");

    // Preserve v2's state machine literally: the first matching management
    // pass only proposes autoroll; a later A starts it, each failed roll
    // injects N and returns through the management routine, and the wait loop
    // reaches the hook again with A idle rather than the synthetic key.
    MemGetMainPtr(MapIsInGame)[0] = 0;
    regs.sp = 0x0100;
    MemGetMainPtr(0x0101)[0] = 0xA9;
    MemGetMainPtr(0x0102)[0] = 0x6E;
    MemGetMainPtr(0x00EC)[0] = 0;
    std::fill_n(MemGetMainPtr(RaceAttributeMaximums + 1), 7, 18);
    std::fill_n(MemGetMainPtr(CharacterCreateAttributes), 7, 8);
    regs.a = 0;
    hooks.HandleInstruction(PcCharacterManagementKey);
    Check(hooks.State().startMenu == StartMenuState::AttributesRerollPropose,
          "first attribute-screen pass proposes autoroll without consuming its key");
    // Reproduce the real SDL/Apple //e latch sequence. Once the game clears
    // the A strobe, v2 expects the latch to retain plain uppercase A; a
    // lowercase residue is interpreted by the wait hook as a cancellation.
    KeybReset();
    KeybSetCapsLock(true);
    KeybQueueKeypress('a', ASCII);
    regs.a = KeybReadData();
    KeybClearStrobe();
    hooks.HandleInstruction(PcCharacterManagementKey);
    Check(hooks.State().startMenu == StartMenuState::AttributesRerolling
              && events.last.type == HookEventType::RequestSpeed
              && events.last.value == 6,
          "A starts v2 full-speed autoroll");
    regs.a = KeybReadData();
    hooks.HandleInstruction(PcCharacterWaitKey);
    Check(hooks.State().startMenu == StartMenuState::AttributesRerolling
              && hooks.State().rerollCount == 1 && regs.a == ('N' | 0x80),
          "autoroll injects its first N and remains active");
    hooks.HandleInstruction(PcCharacterManagementKey);
    Check(regs.pc == 0x7C16,
          "rolling state bypasses character-management input exactly like v2");
    regs.a = 0;
    hooks.HandleInstruction(PcCharacterWaitKey);
    Check(hooks.State().startMenu == StartMenuState::AttributesRerolling
              && hooks.State().rerollCount == 2 && regs.a == ('N' | 0x80),
          "idle wait loop continues autoroll");
    std::fill_n(MemGetMainPtr(CharacterCreateAttributes), 7, 17);
    regs.a = 0;
    hooks.HandleInstruction(PcCharacterWaitKey);
    Check(hooks.State().startMenu == StartMenuState::AttributesRerollDone,
          "autoroll stops only after all four requested attributes qualify");
    MemGetMainPtr(MapIsInGame)[0] = 0xE5;

    regs.pc = PcDecrementTimer;
    auto result = hooks.HandleInstruction(PcDecrementTimer);
    Check(result.skipOpcode && result.cycles == 6 && regs.pc == PcDecrementTimer + 3,
          "idle timer DEC is replaced with a six-cycle no-op");
    Check(hooks.State().hasBeenIdle, "idle hook records a settled game state");

    regs.sp = 0x0100;
    MemGetMainPtr(0x0101)[0] = 0;
    MemGetMainPtr(0x0102)[0] = 0;
    MemGetMainPtr(PartyIconType)[0] = 0;
    regs.a = 0x8B;
    hooks.HandleInstruction(PcMapKey);
    Check(regs.a == ('I' | 0x80), "up arrow maps to Deathlord movement key");
    MemGetMainPtr(PartyIconType)[0] = 2;
    hooks.HandleInstruction(PcMapKey);
    Check(regs.a == ('^' | 0x80), "movement exits a pit when the fix is enabled");

    MemGetMainPtr(PartyCurrentCharacter)[0] = 4;
    MemGetMainPtr(PartyWeaponReady)[4] = 1;
    regs.pc = PcRearAttackCheck;
    result = hooks.HandleInstruction(PcRearAttackCheck);
    Check(result.cycles == 2 && regs.pc == PcRearAttackCheck + 2,
          "rear-rank ranged attack bypasses the branch");

    regs.pc = PcNinjaMonkAcMask;
    result = hooks.HandleInstruction(PcNinjaMonkAcMask);
    Check(result.cycles == 2 && regs.pc == PcNinjaMonkAcMask + 2,
          "Ninja/Monk armor-class wrap bug remains fixed");

    MemGetMainPtr(PartyClass)[1] = static_cast<BYTE>(CharacterClass::Peasant);
    MemGetMainPtr(CurrentPlayerTile)[0] = 0x3C;
    MemGetMainPtr(MapIsOverland)[0] = 0x80;
    regs.x = 1;
    regs.pc = PcCharacterTileDamage;
    result = hooks.HandleInstruction(PcCharacterTileDamage);
    Check(result.cycles == 12 && regs.pc == PcCharacterTileDamage + 6,
          "Peasant resilience skips damaging-tile calls, including fire");

    std::fill_n(MemGetMainPtr(PartyXpLow), PartySize, 0);
    std::fill_n(MemGetMainPtr(PartyXpHigh), PartySize, 0);
    MemGetMainPtr(BattleXpLow)[0] = 5;
    MemGetMainPtr(BattleXpHigh)[0] = 0;
    regs.pc = PcBattleBeginXp;
    result = hooks.HandleInstruction(PcBattleBeginXp);
    Check(result.cycles == 2 && regs.pc == PcBattleBeginXp + 2,
          "small combat XP pool starts at a selected character");
    Check(SumWords(PartyXpLow, PartyXpHigh) == 4,
          "combat XP overflow gives four points directly and leaves one to the game");
    Check(std::all_of(MemGetMainPtr(BattleGetXp), MemGetMainPtr(BattleGetXp) + PartySize,
                      [](BYTE value) { return value == 1; }),
          "every party member is eligible for combat XP");

    std::fill_n(MemGetMainPtr(PartyFood), PartySize, 10);
    regs.a = 40;
    regs.y = 2;
    regs.pc = PcIncrementFood;
    result = hooks.HandleInstruction(PcIncrementFood);
    Check(result.cycles == 5 && regs.pc == PcIncrementFood + 3,
          "food purchase store is replaced");
    Check(std::all_of(MemGetMainPtr(PartyFood), MemGetMainPtr(PartyFood) + PartySize,
                      [](BYTE value) { return value == 15; }),
          "purchased food is evenly distributed");

    for (int i = 0; i < PartySize; ++i)
    {
        MemGetMainPtr(PartyGoldLow)[i] = static_cast<BYTE>(6000);
        MemGetMainPtr(PartyGoldHigh)[i] = static_cast<BYTE>(6000 >> 8);
    }
    regs.a = 2;
    regs.pc = PcPoolGold;
    hooks.HandleInstruction(PcPoolGold);
    const int selectedGold = MemGetMainPtr(PartyGoldLow)[2]
                           | (MemGetMainPtr(PartyGoldHigh)[2] << 8);
    Check(regs.pc == PcPoolGoldEnd && selectedGold == 10000,
          "gold pooling caps the selected character and jumps to RTS");
    Check(SumWords(PartyGoldLow, PartyGoldHigh) == 36000,
          "gold pooling preserves every gold piece");

    hooks.HandleInstruction(PcClearMap);
    Check(events.count > 0 && events.last.type == HookEventType::MapTransition,
          "presentation consumers receive typed map-transition events");

    hooks.Detach();
    Check(g_cpuInstructionHook == nullptr && g_cpuInstructionHookUserData == nullptr,
          "hook detaches cleanly from the emulator");

    MemDestroy();
    GetFrame().Destroy();

    if (failures == 0)
        std::printf("Relorded hook tests passed: CPU bridge, gameplay fixes, distributions, events\n");
    return failures == 0 ? 0 : 1;
}
