#include "HackingChecks.h"

#include "DlrlHooks.h"
#include "Emulator/Memory.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace dlrl
{
namespace
{
using namespace deathlord;

BYTE& Ram(WORD address) { return MemGetMainPtr(address)[0]; }
unsigned Health(int member = 0)
{
    return Ram(PartyHealthLow + member) | (Ram(PartyHealthHigh + member) << 8);
}
void SetWord(WORD low, WORD high, unsigned value, int member = 0)
{
    Ram(low + member) = static_cast<BYTE>(value);
    Ram(high + member) = static_cast<BYTE>(value >> 8);
}
unsigned TotalXp()
{
    unsigned total = 0;
    for (int member = 0; member < PartySize; ++member)
        total += Ram(PartyXpLow + member) | (Ram(PartyXpHigh + member) << 8);
    return total;
}

struct BattleProbe
{
    int ended = 0;
    int attacks = 0;
};
void OnBattleEvent(const HookEvent& event, void* context)
{
    auto& probe = *static_cast<BattleProbe*>(context);
    if (event.type == HookEventType::BattleEnded) ++probe.ended;
    if (event.type == HookEventType::ActorAttacks) ++probe.attacks;
}
}

bool RunHackingChecks(DlrlHooks& hooks)
{
    // Called only after booting a disposable HDV to its movement prompt.
    // Execute the real 2.0.1 routines, not synthetic copies of their opcodes.
    std::array<BYTE, 65536> baseline{};
    std::copy_n(MemGetMainPtr(0), baseline.size(), baseline.begin());
    const auto originalRegisters = regs;
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool okay, const char* description) {
        ++checks;
        if (!okay) { ++failures; std::fprintf(stderr,"Hacking FAIL: %s\n",description); }
    };
    const auto reset = [&] {
        std::copy(baseline.begin(), baseline.end(), MemGetMainPtr(0));
        hooks.ResetRuntime();
        hooks.Hacking() = {};
        hooks.SetEventCallback(nullptr);
        Ram(PartySizeAddress) = PartySize;
        Ram(PartyCurrentCharacter) = Ram(PartyLeader) = 0;
        Ram(0xFC64) = 0; // Disable the game's cosmetic battle delay for tests.
        Ram(0xFC61) = 0xFF; // Party initiative, including the mid-battle-toggle case.
        for (int member = 0; member < PartySize; ++member)
        {
            Ram(PartyStatus + member) = 0;
            Ram(PartyPower + member) = 77;
            Ram(PartyLevelPlus + member) = 0;
            SetWord(PartyHealthLow, PartyHealthHigh, 200, member);
            SetWord(PartyHealthMaxLow, PartyHealthMaxHigh, 200, member);
            SetWord(PartyXpLow, PartyXpHigh, 0, member);
        }
    };
    const auto call = [&](WORD pc, BYTE x = 0, BYTE a = 0, BYTE y = 0,
                          bool enableAtPrompt = false) {
        constexpr WORD sentinel = 0x0300;
        regs = originalRegisters;
        regs.pc = pc;
        regs.sp = 0x01ED;
        regs.ps = AF_RESERVED | AF_INTERRUPT;
        regs.x = x;
        regs.a = a;
        regs.y = y;
        Ram(0x01EF) = (sentinel - 1) >> 8;
        Ram(0x01EE) = static_cast<BYTE>(sentinel - 1);
        for (int instruction = 0; instruction < 2000000; ++instruction)
        {
            if (regs.pc == sentinel) return regs.sp == 0x01EF;
            if (enableAtPrompt && regs.pc == PcBattleCommandPrompt)
                hooks.Hacking().automaticBattleSuccess = true;
            CpuExecute(1, false);
        }
        std::fprintf(stderr,"Hacking routine $%04X timed out at $%04X, SP $%04X\n",pc,regs.pc,regs.sp);
        return false;
    };

    for (bool invincible : {false, true})
    {
        for (unsigned health : {1u, 100u, 256u, 65535u})
            for (unsigned damage : {1u, 257u, 65535u})
            {
                reset();
                hooks.Hacking().invincible = invincible;
                SetWord(PartyHealthLow, PartyHealthHigh, health);
                Ram(0x6068) = static_cast<BYTE>(damage);
                Ram(0x606E) = static_cast<BYTE>(damage >> 8);
                check(call(PcApplyHpDamage), "shared HP subtractor returns with a balanced stack");
                const unsigned expected = invincible ? health : health > damage ? health - damage : 0;
                check(Health() == expected, "16-bit HP damage and invincibility, including borrow and lethal hits");
                check(((Ram(PartyStatus) & 0x40) != 0) == (expected == 0), "lethal damage/death flags agree with protected HP");
                check(!invincible || Ram(PartyPower) == 77, "prevented death does not erase power");
            }

        reset();
        hooks.Hacking().invincible = invincible;
        check(call(0x6008), "whole-party HP-halving routine returns");
        for (int member = 0; member < PartySize; ++member)
            check(Health(member) == (invincible ? 200u : 100u), "HP-halving protection covers every party member");

        reset();
        hooks.Hacking().invincible = invincible;
        Ram(PartyStatus) = 0x40;
        check(call(0xABFD), "instant-death HP routine returns");
        check(Health() == (invincible ? 200u : 0u), "instant-death HP clearing respects invincibility");
        check(!invincible || (Ram(PartyStatus) & 0xC0) == 0, "protected living character is not left dead with HP");

        reset();
        hooks.Hacking().invincible = invincible;
        Ram(BattleDamage) = 10;
        check(call(PcDrainMaximumHp), "maximum-HP drain routine returns");
        check(Health() == (invincible ? 200u : 190u), "maximum-HP drain cannot reduce protected HP");
        check(Ram(PartyHealthMaxLow) == (invincible ? 200 : 190), "maximum HP is protected too");

        reset();
        hooks.Hacking().invincible = invincible;
        check(call(PcDestroyMaximumHp), "maximum-HP destruction routine returns");
        check(Health() == (invincible ? 200u : 0u), "fatal maximum-HP destruction respects invincibility");
    }

    reset();
    hooks.Hacking().invincible = true;
    SetWord(PartyHealthLow, PartyHealthHigh, 100);
    check(call(PcBattleCharacterHealed,0,0,150), "native healing still returns while invincible");
    check(Health() == 150, "invincibility allows healing");
    hooks.Hacking().invincible = false;
    Ram(0x6068) = 1;
    Ram(0x606E) = 0;
    check(call(PcApplyHpDamage) && Health() == 149, "disabling invincibility immediately restores damage");

    reset();
    hooks.Hacking().invincible = true;
    Ram(PartyStatus) = 0x0E; // Poison/illness/starvation remain statuses, not HP damage.
    Ram(0x6068) = 255;
    Ram(0x606E) = 255;
    check(call(PcApplyHpDamage) && Health() == 200 && Ram(PartyStatus) == 0x0E,
          "damage is blocked without clearing nonfatal status effects");

    for (bool midBattle : {false, true})
        for (BYTE monster : {BYTE(1), BYTE(17), BYTE(64), BYTE(126), BYTE(127), BYTE(128)})
        {
            reset();
            BattleProbe probe;
            hooks.SetEventCallback(OnBattleEvent, &probe);
            hooks.Hacking().automaticBattleSuccess = !midBattle;
            // Exercise native XP distribution with and without Relorded's
            // optional fair-XP hook; no stale participation masks may hang it.
            hooks.Changes().xpReallocation = !midBattle;
            Ram(MapMonsterSpriteIds) = monster;
            const bool returned = call(0xA200,0,monster == 128 ? 0x50 : 0x40,0,midBattle);
            check(returned, "automatic battle victory completes the native battle and restores the stack");
            check(probe.ended == 1 && !hooks.State().inBattle, "battle-ended event and state are emitted once");
            check(probe.attacks == 0, "automatic victory requires no party or enemy attacks");
            check(Ram(BattleEnemyCount) == 0 && Ram(BattleEscaped) == 0, "automatic battle is a defeat, not an escape");
            check(Health() == 200, "automatic victory does not cost party HP");
            check(TotalXp() > 0 || Ram(BattleEnemyXp) == 0, "normal native XP rewards are retained");
            std::printf("Automatic battle: monster %u, mid-battle=%d, XP=%u, PC=$%04X\n",
                        monster,midBattle,TotalXp(),regs.pc);
        }

    hooks.SetEventCallback(nullptr);
    std::copy(baseline.begin(), baseline.end(), MemGetMainPtr(0));
    regs = originalRegisters;
    hooks.ResetRuntime();
    hooks.Hacking() = {};
    std::printf("Hacking checks: %d passed, %d failed\n",checks-failures,failures);
    return failures == 0;
}
}
