#include "DlrlHooks.h"

#include "Emulator/Memory.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace dlrl
{
namespace
{

using namespace deathlord;

BYTE& Ram(std::uint16_t address)
{
    return MemGetMainPtr(address)[0];
}

BYTE& PartyByte(std::uint16_t address, std::size_t character)
{
    return MemGetMainPtr(address)[character];
}

std::uint16_t PartyWord(std::uint16_t lowAddress, std::uint16_t highAddress,
                        std::size_t character)
{
    return static_cast<std::uint16_t>(PartyByte(lowAddress, character))
         | (static_cast<std::uint16_t>(PartyByte(highAddress, character)) << 8);
}

void SetPartyWord(std::uint16_t lowAddress, std::uint16_t highAddress,
                  std::size_t character, int value)
{
    PartyByte(lowAddress, character) = static_cast<BYTE>(value);
    PartyByte(highAddress, character) = static_cast<BYTE>(value >> 8);
}

void SkipTo(CpuInstructionHookResult& result, std::uint16_t pc,
            std::uint32_t cycles, bool consumeIteration = false)
{
    regs.pc = pc;
    result.cycles += cycles;
    result.skipOpcode = consumeIteration;
}

bool IsPoisonResistantClass(BYTE value)
{
    const auto characterClass = static_cast<CharacterClass>(value);
    return characterClass == CharacterClass::Barbarian
        || characterClass == CharacterClass::Druid
        || characterClass == CharacterClass::Peasant
        || characterClass == CharacterClass::Ranger;
}

bool IsPoisonResistantRace(BYTE value)
{
    const auto race = static_cast<Race>(value);
    return race == Race::Elf || race == Race::HalfElf
        || race == Race::Orc || race == Race::HalfOrc;
}

} // namespace

DlrlHooks::~DlrlHooks()
{
    Detach();
}

void DlrlHooks::Attach()
{
    g_cpuInstructionHook = &DlrlHooks::InstructionThunk;
    g_cpuInstructionHookUserData = this;
    attached_ = true;
}

void DlrlHooks::Detach()
{
    if (attached_ && g_cpuInstructionHookUserData == this)
    {
        g_cpuInstructionHook = nullptr;
        g_cpuInstructionHookUserData = nullptr;
    }
    attached_ = false;
}

void DlrlHooks::ResetRuntime()
{
    state_ = {};
}

void DlrlHooks::SetEventCallback(HookEventCallback callback, void* userData)
{
    eventCallback_ = callback;
    eventUserData_ = userData;
}

void DlrlHooks::SetCanEquipCallback(CanEquipCallback callback, void* userData)
{
    canEquipCallback_ = callback;
    canEquipUserData_ = userData;
}

void DlrlHooks::SetRandomSeed(std::uint32_t seed)
{
    randomState_ = seed ? seed : 1u;
}

CpuInstructionHookResult DlrlHooks::InstructionThunk(WORD pc, void* userData)
{
    return static_cast<DlrlHooks*>(userData)->HandleInstruction(pc);
}

void DlrlHooks::Emit(HookEventType type, std::uint16_t pc, int actor,
                     int value, int auxiliary) const
{
    if (eventCallback_)
        eventCallback_({type, pc, actor, value, auxiliary}, eventUserData_);
}

std::uint32_t DlrlHooks::NextRandom()
{
    // Small deterministic generator: hooks never perturb libc's global rand().
    std::uint32_t value = randomState_;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    randomState_ = value ? value : 1u;
    return randomState_;
}

CpuInstructionHookResult DlrlHooks::HandleInstruction(std::uint16_t pc)
{
    CpuInstructionHookResult result{};
    ++state_.hookCalls;
    state_.inGameMap = Ram(MapIsInGame) == 0xE5;

    // Presentation events are intentionally data-only. ImGui, the automap,
    // battle sprites and the log consume them outside the emulator core.
    switch (pc)
    {
    case PcBattleAmbush:
    case PcBattleEnter:
        state_.inBattle = true;
        Emit(HookEventType::BattleStarted, pc);
        break;
    case PcBattleExit:
        state_.inBattle = false;
        Emit(HookEventType::BattleEnded, pc);
        break;
    case PcBattleEnemyId:
        state_.inBattle = true;
        Emit(HookEventType::EnemyType, pc, -1, regs.a);
        break;
    case PcBattleEnemyHpSet:
        Emit(HookEventType::EnemyHpReady, pc);
        break;
    case PcBattleCharacterTurn:
        Emit(HookEventType::ActiveActor, pc, regs.x);
        break;
    case PcBattleEnemyTurn:
        Emit(HookEventType::ActiveActor, pc, regs.x + PartySize);
        break;
    case PcBattleCharacterAttack:
        Emit(HookEventType::ActorAttacks, pc, Ram(PartyCurrentCharacter));
        break;
    case PcBattleEnemyAttack:
        Emit(HookEventType::ActorAttacks, pc, Ram(BattleEnemyIndex) + PartySize);
        break;
    case PcBattleEnemyMissed:
        Emit(HookEventType::ActorDodges, pc, Ram(PartyCurrentCharacter));
        break;
    case PcBattleEnemyHit:
        Emit(HookEventType::ActorHit, pc, Ram(PartyCurrentCharacter), Ram(BattleDamage));
        break;
    case PcBattleCharacterHit:
        Emit(HookEventType::ActorHit, pc, regs.x + PartySize, regs.a);
        break;
    case PcBattleCharacterKilled:
        Emit(HookEventType::ActorKilled, pc, regs.x + PartySize);
        break;
    case PcBattleCharacterBanished:
        Emit(HookEventType::ActorKilled, pc, regs.x + PartySize - 1);
        break;
    case PcBattleCharacterHealed:
        Emit(HookEventType::ActorHealed, pc, regs.x,
             Ram(BattleCharacterHealLow) | (Ram(BattleCharacterHealHigh) << 8));
        break;
    case PcCharacterLevelUp:
        Emit(HookEventType::LevelUpAvailable, pc, regs.x);
        break;
    case PcScrollWindow:
        Emit(HookEventType::ScrollText, pc, -1, regs.a);
        break;
    case PcPrintCharacter:
        Emit(HookEventType::PrintCharacter, pc, -1, regs.a,
             Ram(PrintInverse) != 0);
        break;
    case PcInverseLine:
        Emit(HookEventType::InverseTextLine, pc, -1, 7 - regs.a);
        break;
    case PcClearPrintArea:
        Emit(HookEventType::ClearText, pc, -1, regs.a);
        break;
    case PcDead:
        state_.dead = true;
        Emit(HookEventType::AllCharactersDead, pc);
        break;
    case PcGiveOrb:
        state_.endCredits = true;
        Emit(HookEventType::EndCredits, pc);
        break;
    default:
        break;
    }

    if (!state_.inGameMap)
    {
        switch (pc)
        {
        case PcTitleKey:
            state_.startMenu = StartMenuState::Title;
            Emit(HookEventType::RequestSpeed, pc, -1, 1);
            break;
        case PcMenuKey:
            state_.startMenu = regs.a > 0x7F ? StartMenuState::Other
                                             : StartMenuState::Menu;
            if (regs.a == ('P' | 0x80))
                Emit(HookEventType::RequestSpeed, pc, -1, 6);
            break;
        case PcStartupSplash:
            Emit(HookEventType::StartupSplash, pc);
            break;
        case PcCharacterEscape:
            state_.startMenu = StartMenuState::Other;
            break;
        case PcCharacterManagementKey:
        {
            const BYTE stackLow = Ram(static_cast<std::uint16_t>(regs.sp + 1));
            const BYTE stackHigh = Ram(static_cast<std::uint16_t>(regs.sp + 2));
            if (stackLow != 0xA9 || stackHigh != 0x6E)
            {
                state_.startMenu = StartMenuState::Other;
                break;
            }
            switch (state_.startMenu)
            {
            case StartMenuState::AttributesRerolling:
                regs.pc = 0x7C16;
                break;
            case StartMenuState::AttributesRerollCancelled:
            case StartMenuState::AttributesRerollDone:
            case StartMenuState::AttributesRerollPropose:
                if (regs.a == ('A' | 0x80))
                {
                    state_.rerollCount = 0;
                    state_.startMenu = StartMenuState::AttributesRerolling;
                    Ram(0xC000) = 0;
                    regs.pc = 0x7C16;
                    Emit(HookEventType::RequestSpeed, pc, -1, 6);
                }
                else if (regs.a == ('N' | 0x80))
                {
                    state_.startMenu = StartMenuState::AttributesRerollPropose;
                    Ram(0xC000) = 0;
                    regs.pc = 0x7C16;
                }
                else if (regs.a > 0x80)
                {
                    state_.startMenu = StartMenuState::Other;
                    Ram(0xC000) = 0;
                    regs.pc = 0x7C17;
                }
                break;
            default:
                // v2 deliberately ignores the current key the first time it
                // recognizes the attribute-roll screen. A subsequent pass
                // handles A/N through the explicit states above.
                state_.startMenu = StartMenuState::AttributesRerollPropose;
                break;
            }
            break;
        }
        case PcCharacterWaitKey:
            if (state_.startMenu == StartMenuState::AttributesRerolling)
            {
                if (regs.a != 'A' && regs.a != 0)
                {
                    state_.startMenu = StartMenuState::AttributesRerollCancelled;
                    Emit(HookEventType::RequestSpeed, pc, -1, 1);
                    break;
                }

                const BYTE race = Ram(0x00EC);
                const BYTE* maximums = MemGetMainPtr(
                    static_cast<WORD>(RaceAttributeMaximums + 8 * race + 1));
                const BYTE* attributes = MemGetMainPtr(CharacterCreateAttributes);
                const std::array<int, 4> checkedAttributes = {
                    static_cast<int>(Attribute::Strength),
                    static_cast<int>(Attribute::Constitution),
                    static_cast<int>(Attribute::Intelligence),
                    static_cast<int>(Attribute::Dexterity),
                };
                bool reroll = false;
                for (int attribute : checkedAttributes)
                    reroll = reroll || attributes[attribute] < maximums[attribute] - 1;

                if (reroll)
                {
                    ++state_.rerollCount;
                    regs.a = 'N' | 0x80;
                    Ram(0xC000) = 0;
                    regs.pc = 0x7A6B;
                    if (state_.rerollCount > 255)
                    {
                        for (std::size_t i = 0; i < 255; ++i)
                            MemGetMainPtr(CharacterCreateRng)[i] =
                                static_cast<BYTE>(NextRandom() % 255);
                    }
                }
                else
                {
                    state_.startMenu = StartMenuState::AttributesRerollDone;
                    Emit(HookEventType::RequestSpeed, pc, -1, 1);
                }
            }
            break;
        default:
            break;
        }
        return result;
    }

    switch (pc)
    {
    case PcTransitOutDungeon:
    case PcTransitOutTown:
    case PcTransitOutOverland:
    case PcChangeFloor:
    case PcChangeMapType:
    case PcClearMap:
    case PcChangeOverlandMap:
        state_.inTransition = true;
        Emit(HookEventType::RequestSpeed, pc, -1, 6);
        Emit(HookEventType::MapTransition, pc);
        break;
    case PcMoveDungeon:
    case PcMoveOverland:
        if (regs.a > 0x4F) Emit(HookEventType::MapMayNeedSave, pc);
        break;
    case PcMapKey:
    {
        const BYTE stackLow = Ram(static_cast<std::uint16_t>(regs.sp + 1));
        const BYTE stackHigh = Ram(static_cast<std::uint16_t>(regs.sp + 2));
        if (stackLow != StackMapKeyLow || stackHigh != StackMapKeyHigh)
        {
            if (stackLow != StackSpellLow || stackHigh != StackSpellHigh)
            {
                switch (regs.a)
                {
                case 0x8B: regs.a = 'I' | 0x80; break;
                case 0x88: regs.a = 'J' | 0x80; break;
                case 0x95: regs.a = 'K' | 0x80; break;
                case 0x8A: regs.a = 'M' | 0x80; break;
                default: break;
                }
            }
        }
        if (changes_.exitPitByMoving && Ram(PartyIconType) == 2)
        {
            const char key = static_cast<char>(regs.a & 0x7F);
            if (key == 'I' || key == 'J' || key == 'K' || key == 'M')
                regs.a = '^' | 0x80;
        }
        state_.shouldRecalculateTiles = true;
        break;
    }
    case PcDecrementTimer:
        if (changes_.freezeTimeWhenIdle)
        {
            if (state_.inTransition)
                Emit(HookEventType::RequestSpeed, pc, -1, 1);
            state_.inBattle = false;
            state_.inTransition = false;
            state_.hasBeenIdle = true;
            SkipTo(result, static_cast<std::uint16_t>(pc + 3), 6, true);
        }
        break;
    case PcAllCharactersDead:
        state_.inTransition = false;
        state_.inBattle = false;
        state_.hasBeenIdle = true;
        Emit(HookEventType::AllCharactersDead, pc);
        break;
    case PcEndDrawingTiles:
        if (state_.shouldRecalculateTiles)
        {
            Emit(HookEventType::RecalculateTileVisibility, pc);
            state_.shouldRecalculateTiles = false;
        }
        break;
    case PcRearAttackCheck:
    {
        const BYTE character = Ram(PartyCurrentCharacter);
        if (changes_.rangedAttackForRearLine && character > 2
            && character < PartySize && PartyByte(PartyWeaponReady, character) == 1)
            SkipTo(result, static_cast<std::uint16_t>(pc + 2), 2);
        break;
    }
    case PcSearchCheck:
        if (changes_.searchAlwaysSucceeds)
            SkipTo(result, static_cast<std::uint16_t>(pc + 2), 2);
        break;
    case PcEnemyDrain:
        if (changes_.noLevelDrain)
            SkipTo(result, static_cast<std::uint16_t>(pc + 2), 2);
        break;
    case PcMagicWater:
        if (changes_.magicWaterIncreasesStats) regs.x = 0;
        break;
    case PcStatCeiling:
    {
        if (changes_.noStatsLimit)
            SkipTo(result, static_cast<std::uint16_t>(pc + 2), 2);
        int attribute = -1;
        switch (Ram(0x00A0))
        {
        case 0x72: attribute = static_cast<int>(Attribute::Strength); break;
        case 0x78: attribute = static_cast<int>(Attribute::Constitution); break;
        case 0x7E: attribute = static_cast<int>(Attribute::Size); break;
        case 0x84: attribute = static_cast<int>(Attribute::Intelligence); break;
        case 0x8A: attribute = static_cast<int>(Attribute::Dexterity); break;
        case 0x90: attribute = static_cast<int>(Attribute::Charisma); break;
        case 0x9C: attribute = static_cast<int>(Attribute::Power); break;
        default: break;
        }
        if (attribute >= 0) Emit(HookEventType::AttributeIncreased, pc, regs.y, attribute);
        break;
    }
    case PcCharacterHpLoss:
        if (changes_.noHpLossFromStarvation && regs.x < PartySize
            && PartyByte(PartyStatus, regs.x) == 0x02)
        {
            regs.a = 0;
            regs.y = 0;
        }
        break;
    case PcCharacterTileDamage:
        if (changes_.extraRaceAndClassBonuses && regs.x < PartySize)
        {
            const BYTE characterClass = PartyByte(PartyClass, regs.x);
            const BYTE tile = Ram(CurrentPlayerTile);
            bool avoid = static_cast<CharacterClass>(characterClass) == CharacterClass::Peasant;
            const bool overland = Ram(MapIsOverland) == 0x80;
            const bool severeTile = overland ? tile == 0x3C
                : (tile == 0x2D || tile == 0x2C || tile == 0x38);
            if (!severeTile)
                avoid = avoid || IsPoisonResistantClass(characterClass)
                              || IsPoisonResistantRace(PartyByte(PartyRace, regs.x));
            if (avoid) SkipTo(result, static_cast<std::uint16_t>(pc + 6), 12);
        }
        break;
    case PcSaveAfterOneDead:
        if (changes_.noAutosaveAfterDeath)
            SkipTo(result, static_cast<std::uint16_t>(pc + 2), 2);
        break;
    case PcSaveAfterAllDead:
        if (changes_.noAutosaveAfterDeath) Ram(0x8710) = 0;
        break;
    case PcNinjaMonkAcMask:
        // Original Deathlord bug: AND #$0F wraps Ninja/Monk AC at level 32.
        SkipTo(result, static_cast<std::uint16_t>(pc + 2), 2);
        break;
    case PcReadyWeaponCheck:
        if (changes_.expandedWeaponUse && regs.x < PartySize && canEquipCallback_
            && canEquipCallback_(regs.a, PartyByte(PartyClass, regs.x),
                                 PartyByte(PartyRace, regs.x), canEquipUserData_))
            SkipTo(result, static_cast<std::uint16_t>(pc + 3), 6);
        break;
    case PcBattleBeginXp:
        if (changes_.xpReallocation)
        {
            for (std::size_t i = 0; i < PartySize; ++i) PartyByte(BattleGetXp, i) = 1;
            std::uint16_t battleXp = Ram(BattleXpLow) | (Ram(BattleXpHigh) << 8);
            if (battleXp < PartySize)
            {
                BYTE character = static_cast<BYTE>(NextRandom() % PartySize);
                while (battleXp > 1)
                {
                    SetPartyWord(PartyXpLow, PartyXpHigh, character,
                                 PartyWord(PartyXpLow, PartyXpHigh, character) + 1);
                    character = static_cast<BYTE>((character + 1) % PartySize);
                    --battleXp;
                }
                regs.x = character;
                SkipTo(result, static_cast<std::uint16_t>(pc + 2), 2);
            }
        }
        break;
    case PcIncrementLevel:
        if (changes_.extraRaceAndClassBonuses && regs.x < PartySize
            && static_cast<CharacterClass>(PartyByte(PartyClass, regs.x))
                == CharacterClass::Peasant
            && PartyByte(PartyLevel, regs.x) % 5 == 4)
        {
            constexpr std::array<std::uint16_t, 3> attributes = {
                PartyStrength, PartyDexterity, PartyConstitution
            };
            ++PartyByte(attributes[NextRandom() % attributes.size()], regs.x);
        }
        break;
    case PcResetXpOnLevel:
        if (changes_.keepExtraXpOnLevel)
            SkipTo(result, static_cast<std::uint16_t>(pc + 6), 10);
        break;
    case PcLevelCheck:
        if (regs.a == 0 && regs.x < PartySize)
        {
            constexpr std::array<int, 22> requirements = {
                0, 200, 200, 200, 200, 400, 400, 400, 400, 600, 600,
                600, 600, 800, 800, 800, 800, 1000, 1000, 1000, 1000, 1200
            };
            const std::size_t level = std::min<std::size_t>(PartyByte(PartyLevel, regs.x), 21);
            const int missing = requirements[level]
                              - PartyWord(PartyXpLow, PartyXpHigh, regs.x);
            Emit(HookEventType::MissingXp, pc, regs.x, missing);
        }
        break;
    case PcBuyFood:
        if (changes_.distributeFood)
        {
            const BYTE current = Ram(PartyCurrentCharacter);
            if (current >= PartySize) break;
            int total = 0;
            for (std::size_t i = 0; i < PartySize; ++i)
            {
                total += PartyByte(PartyFood, i);
                PartyByte(PartyFood, i) = 0;
            }
            if (total > 100 * (PartySize - 1))
            {
                PartyByte(PartyFood, current) = static_cast<BYTE>(total - 100 * (PartySize - 1));
                total -= PartyByte(PartyFood, current);
            }
            const int perMember = total / (PartySize - 1);
            for (std::size_t i = 0; i < PartySize; ++i)
                if (i != current) PartyByte(PartyFood, i) = static_cast<BYTE>(perMember);
            total -= perMember * (PartySize - 1);
            BYTE member = 0;
            while (total-- > 0)
            {
                do { member = static_cast<BYTE>((member + 1) % PartySize); }
                while (member == current);
                ++PartyByte(PartyFood, member);
            }
        }
        break;
    case PcIncrementFood:
        if (changes_.distributeFood && regs.y < PartySize)
        {
            PartyByte(PartyFood, regs.y) = regs.a;
            int total = 0;
            for (std::size_t i = 0; i < PartySize; ++i) total += PartyByte(PartyFood, i);
            for (std::size_t i = 0; i < PartySize; ++i)
                PartyByte(PartyFood, i) = static_cast<BYTE>(total / PartySize);
            for (int i = 0; i < total % PartySize; ++i) ++PartyByte(PartyFood, i);
            SkipTo(result, static_cast<std::uint16_t>(pc + 3), 5);
        }
        break;
    case PcPoolGold:
        if (changes_.fixGoldPooling && regs.a < PartySize)
        {
            int total = 0;
            for (std::size_t i = 0; i < PartySize; ++i)
            {
                total += PartyWord(PartyGoldLow, PartyGoldHigh, i);
                SetPartyWord(PartyGoldLow, PartyGoldHigh, i, 0);
            }
            const BYTE selected = regs.a;
            const int selectedGold = std::min(10000, total);
            SetPartyWord(PartyGoldLow, PartyGoldHigh, selected, selectedGold);
            total -= selectedGold;
            const int perMember = total / (PartySize - 1);
            for (std::size_t i = 0; i < PartySize; ++i)
                if (i != selected) SetPartyWord(PartyGoldLow, PartyGoldHigh, i, perMember);
            total -= perMember * (PartySize - 1);
            BYTE member = 0;
            while (total-- > 0)
            {
                do { member = static_cast<BYTE>((member + 1) % PartySize); }
                while (member == selected);
                SetPartyWord(PartyGoldLow, PartyGoldHigh, member,
                             PartyWord(PartyGoldLow, PartyGoldHigh, member) + 1);
            }
            regs.pc = PcPoolGoldEnd;
        }
        break;
    case PcGiveBattleGold:
        if (changes_.distributeGold && regs.x < PartySize)
        {
            const int battleGold = Ram(BattleGoldLow) | (Ram(BattleGoldHigh) << 8);
            int currentGold = PartyWord(PartyGoldLow, PartyGoldHigh, regs.x);
            if (currentGold + battleGold > 10000)
            {
                int otherGold = 0;
                for (std::size_t i = 0; i < PartySize; ++i)
                    if (i != regs.x) otherGold += PartyWord(PartyGoldLow, PartyGoldHigh, i);
                int overage = std::max(0, currentGold + battleGold - 10000);
                overage = std::min(overage, (PartySize - 1) * 10000 - otherGold);
                currentGold -= overage;
                SetPartyWord(PartyGoldLow, PartyGoldHigh, regs.x, currentGold);
                BYTE member = 0;
                while (overage > 0)
                {
                    member = static_cast<BYTE>((member + 1) % PartySize);
                    if (member == regs.x) continue;
                    const int gold = PartyWord(PartyGoldLow, PartyGoldHigh, member);
                    if (gold >= 10000) continue;
                    SetPartyWord(PartyGoldLow, PartyGoldHigh, member, gold + 1);
                    --overage;
                }
            }
        }
        break;
    default:
        break;
    }

    return result;
}

} // namespace dlrl
