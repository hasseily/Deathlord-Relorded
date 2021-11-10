#pragma once

#include "Common.h"


// Memory locations to hijack when processing the game
// to change the behavior on the fly without patching the original code
#define PC_DECREMENT_TIMER		0x621F		// routine to decrement a timer before it makes the player "wait" and pass a turn
#define PC_TRANSIT_OUT_OVERLAND	0xE8F4		// routine that gets the player into a dungeon or town
#define PC_TRANSIT_IN_OVERLAND	0xEF0A		// routine that gets the player back in the overland
#define PC_BEGIN_DRAWING_TILES	0x5931		// Beginning of the tiles drawing routine on the main screen. $300-350 contains the "current" map, $351-3A0 contains the "new" map
#define PC_END_DRAWING_TILES	0x586A		// After all the drawing is done and the LOS is updated. $300-350 contains the map
#define PC_CHECK_REAR_ATTACK	0xA7EF		// Check for the rear rank not allowed to attack
#define MEM_CURRENT_CHAR_POS	0xFC21		// Current active character position (1-6)
#define MEM_CHAR_1_WEAP_READY	0xFDEA		// Character 1 weapon ready status: ff = fists, 00 = melee, 01 = ranged

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
