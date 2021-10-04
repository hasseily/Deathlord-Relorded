#pragma once

#include "Card.h"
#include <Emulator/SaveState_Structs_common.h>

struct SY6522
{
	BYTE ORB;				// $00 - Port B
	BYTE ORA;				// $01 - Port A (with handshaking)
	BYTE DDRB;				// $02 - Data Direction Register B
	BYTE DDRA;				// $03 - Data Direction Register A
	//
	// $04 - Read counter (L) / Write latch (L)
	// $05 - Read / Write & initiate count (H)
	// $06 - Read / Write & latch (L)
	// $07 - Read / Write & latch (H)
	// $08 - Read counter (L) / Write latch (L)
	// $09 - Read counter (H) / Write latch (H)
	IWORD TIMER1_COUNTER;
	IWORD TIMER1_LATCH;
	IWORD TIMER2_COUNTER;
	IWORD TIMER2_LATCH;
	int timer1IrqDelay;
	int timer2IrqDelay;
	//
	BYTE SERIAL_SHIFT;		// $0A
	BYTE ACR;				// $0B - Auxiliary Control Register
	BYTE PCR;				// $0C - Peripheral Control Register
	BYTE IFR;				// $0D - Interrupt Flag Register
	BYTE IER;				// $0E - Interrupt Enable Register
	BYTE ORA_NO_HS;			// $0F - Port A (without handshaking)
};

struct SSI263A
{
	BYTE DurationPhoneme;
	BYTE Inflection;		// I10..I3
	BYTE RateInflection;
	BYTE CtrlArtAmp;
	BYTE FilterFreq;
	//
	BYTE CurrentMode;		// b7:6=Mode; b0=D7 pin (for IRQ)
};

void	MB_Initialize();
void	MB_Reinitialize();
void	MB_Destroy();
void    MB_Reset();
void	MB_InitializeForLoadingSnapshot(void);
void    MB_InitializeIO(LPBYTE pCxRomPeripheral, UINT uSlot4, UINT uSlot5);
void    MB_Mute();
void    MB_Demute();
#ifdef _DEBUG
void    MB_CheckCumulativeCycles();	// DEBUG
#endif
void    MB_SetCumulativeCycles();
void    MB_PeriodicUpdate(UINT executedCycles);
void    MB_UpdateCycles(ULONG uExecutedCycles);
SS_CARDTYPE MB_GetSoundcardType();
bool    MB_IsActive();
DWORD   MB_GetVolume();
void    MB_SetVolume(DWORD dwVolume, DWORD dwVolumeMax);
