#pragma once

#include "Common.h"


struct noxcpuconstants
{
	USHORT PC_PRINTSTR;			// program counter of PRINT.STR routine (can be overriden before screen output, especially in combat for variables)
	USHORT PC_CARRIAGE_RETURN1;	// program counter of a CARRIAGE.RETURN that breaks the lines down in specific lengths (16 chars max). Only use it in battle!
	USHORT PC_CARRIAGE_RETURN2;	// program counter of a CARRIAGE.RETURN that finishes a line
	USHORT PC_COUT;				// program counter of COUT routine which is the lowest level and prints a single char at A
	USHORT A_PRINT_RIGHT;		// A register's value for printing to right scroll area (where the conversations are)
	USHORT PC_INITIATE_COMBAT;	// when combat routine starts
	USHORT PC_END_COMBAT;		// when combat routine ends (don't log during combat)
};
extern noxcpuconstants cpuconstants;

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
