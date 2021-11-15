#pragma once

#include "Common.h"
#include "DeathlordHacks.h"

// Program Counter locations to hijack when processing the game
// to change the behavior on the fly without patching the original code
#define PC_DECREMENT_TIMER			0x621F		// routine to decrement a timer before it makes the player "wait" and pass a turn
#define PC_TRANSIT_OUT_OVERLAND		0xE8F4		// routine that gets the player into a dungeon or town
#define PC_TRANSIT_IN_OVERLAND		0xEF0A		// routine that gets the player back in the overland
#define PC_BEGIN_DRAWING_TILES		0x5931		// Beginning of the tiles drawing routine on the main screen. $300-350 contains the "current" map, $351-3A0 contains the "new" map
#define PC_END_DRAWING_TILES		0x586A		// After all the drawing is done and the LOS is updated. $300-350 contains the map
#define PC_CHECK_REAR_ATTACK		0xA7EF		// BCS after CMP that checks for the rear rank not allowed to attack
#define PC_CHECK_SEARCH_SUCCESS		0x9428		// BCS after CMP that checks success of search
#define PC_ENEMY_ACTION_DRAIN		0xAB19		// BCS: Always branch to avoid level drain. This option makes the enemy skip the turn without feedback
#define PC_ENEMY_ACTION_DRAIN2		0xAADF		// BCS: Always branch to avoid level drain. This option makes the enemy always Miss the level drain
#define PC_MAGIC_WATER_EFFECT		0xB707		// Set X register to 0 to always branch to stat +1
#define PC_STAT_INCREASE_CEILING	0xB7A3		// BCS: Don't branch to remove the max 18 of the stat increase from magic water
#define PC_CHAR_HP_LOSS				0x54B8		// It sets A to 01 (lo byte) and Y to 00 (hi byte), which is the amount we'll drop HP by because of starvation, toxicity, etc... when calling subroutine 0x605D. X has the char position.
#define PC_CHAR_TILE_DAMAGE			0x6038		// JSR to the tile damage subroutine at 0x6063. Bypass it to avoid tile damage to char at index in X.
#define PC_SAVE_AFTER_DEATH			0x5BC5		// BNE should not branch in order to stop saving after a char dies in combat
#define PC_NINJA_MONK_AC_RESET		0xA952		// AND with 0F that resets the Ninja and Monk A/C to 0 every 32 levels. Bypass this bug.

struct regsrec
{
  BYTE a;   // accumulator
  BYTE x;   // index X
  BYTE y;   // index Y
  BYTE ps;  // processor status
  WORD pc;  // program counter
  WORD sp;  // stack pointer
  BYTE bJammed; // CPU has crashed (NMOS 6502 only)
};
extern regsrec    regs;

extern unsigned __int64 g_nCumulativeCycles;

void    CpuDestroy ();
void    CpuCalcCycles(ULONG nExecutedCycles);
DWORD   CpuExecute(const DWORD uCycles, const bool bVideoUpdate);
ULONG   CpuGetCyclesThisVideoFrame(ULONG nExecutedCycles);
void    CpuInitialize ();
void    CpuSetupBenchmark ();
void	CpuIrqReset();
void	CpuIrqAssert(eIRQSRC Device);
void	CpuIrqDeassert(eIRQSRC Device);
void	CpuNmiReset();
void	CpuNmiAssert(eIRQSRC Device);
void	CpuNmiDeassert(eIRQSRC Device);
void    CpuReset ();

BYTE	CpuRead(USHORT addr, ULONG uExecutedCycles);
void	CpuWrite(USHORT addr, BYTE value, ULONG uExecutedCycles);

enum eCpuType {CPU_UNKNOWN=0, CPU_6502=1, CPU_65C02, CPU_Z80};	// Don't change! Persisted to Registry

eCpuType GetMainCpu(void);
void     SetMainCpu(eCpuType cpu);
eCpuType ProbeMainCpuDefault(eApple2Type apple2Type);
void     SetMainCpuDefault(eApple2Type apple2Type);
eCpuType GetActiveCpu(void);
void     SetActiveCpu(eCpuType cpu);

bool Is6502InterruptEnabled(void);
void ResetCyclesExecutedForDebugger(void);
