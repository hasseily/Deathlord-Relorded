/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2011, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "LogWindow.h"
#include "MemoryTriggers.h"	// trigger the memory polling upon passing time because we know the state is safe
#include "DeathlordHacks.h"
//===========================================================================

#ifndef posxy
static int __posX = 0xff;
static int __posY = 0xff;
#define posxy
#endif

static DWORD Cpu65C02(DWORD uTotalCycles, const bool bVideoUpdate)
{
	WORD addr;
	BOOL flagc; // must always be 0 or 1, no other values allowed
	BOOL flagn; // must always be 0 or 0x80.
	BOOL flagv; // any value allowed
	BOOL flagz; // any value allowed
	WORD temp;
	WORD temp2;
	WORD val;
	AF_TO_EF
		ULONG uExecutedCycles = 0;
	WORD base;

	do
	{
		UINT uExtraCycles = 0;
		BYTE iOpcode;

		// NTSC_BEGIN
		ULONG uPreviousCycles = uExecutedCycles;
		// NTSC_END


		///  START DEATHLORD HOOKS

		WORD _origPC = regs.pc;
		if (g_isInGameMap)
		{
			switch (_origPC)
			{
			case PC_TRANSIT_IN_OVERLAND:
				// Starting transit between maps, either from the overland or into the overland
				[[fallthrough]];
			case PC_TRANSIT_OUT_OVERLAND:
			{
				// OutputDebugStringA("STARTED TRANSITION\n");
				auto aM = AutoMap::GetInstance();
				aM->SetShowTransition(true);
				// The PARSE_TILES trigger will only run after the game is back in the player wait loop
				// It will itself insert a trigger to finish the transition, which will be run later
				auto memT = MemoryTriggers::GetInstance();
				memT->DelayedTriggerInsert(DelayedTriggersFunction::PARSE_TILES, 0);
				break;
			}
			case PC_DECREMENT_TIMER:
			{
				// Process memory triggers every time the game processes the turn pass decrement timer
				// We know here that we're in a safe place to parse the video screen, etc...
				// And furthermore we disable that timer because who in his right mind wants turns to pass when not doing anything?
				auto memT = MemoryTriggers::GetInstance();
				if (memT != NULL)
					memT->Process();
				CYC(6);		// NOP 6 cycles. The DEC instruction takes 6 cycles
				regs.pc = _origPC + 3;
				goto AFTERFETCH;
				break;
			}
			case PC_END_DRAWING_TILES:
			{
				auto aM = AutoMap::GetInstance();
				aM->AnalyzeVisibleTiles();
				break;
			}
			case PC_CHECK_REAR_ATTACK:
			{
				// Let rear characters use their ranged weapons
				UINT8 currCharPos = MemGetMainPtr(PARTY_CURRENT_CHAR_POS)[0];
				if (currCharPos > 2)	// rear line of characters (rear 3 chars)
				{
					if (MemGetMainPtr(PARTY_WEAP_READY_START + currCharPos)[0] == 0x01)	// current character has ranged weapon readied
					{
						CYC(2); // BCS uses 2 cycles;
						regs.pc = _origPC + 2;	// Jump to the next instruction, disregard the branch
					}
				}
				break;
			}
			case PC_CHECK_SEARCH_SUCCESS:
			{
				// Always succeed when explicitly "s"earching for a hidden door or pit
				CYC(2); // BCS uses 2 cycles;
				regs.pc = _origPC + 2;	// Jump to the next instruction, disregard the branch
				break;
			}
			case PC_ENEMY_ACTION_DRAIN:
			{
				CYC(2); // BCS uses 2 cycles;
				regs.pc = regs.pc + 2 + MemGetMainPtr(regs.pc+1)[0];	// Always branch to the new instruction
				break;
			}
			/*	Another option for removing level drain. This one makes the level drain always miss, and is earlier in the process
			case PC_ENEMY_ACTION_DRAIN2:
			{
				CYC(2); // BCS uses 2 cycles;
				regs.pc = regs.pc + 2 + MemGetMainPtr(regs.pc + 1)[0];	// Always branch to the new instruction
				break;
			}
			*/
			case PC_MAGIC_WATER_EFFECT:
			{
				regs.x = 0;		// always choose stat increase
				break;
			}
			case PC_STAT_INCREASE_CEILING:
			{
				CYC(2); // BCS uses 2 cycles;
				regs.pc = _origPC + 2;	// Jump to the next instruction, disregard the branch
				break;
			}
			
			case PC_CHAR_HP_LOSS:
			{
				// In the previous instructions it sets A to 1 (lo byte) and Y to 0 (hi byte), which is the amount to reduce HP by.
				// X is the player position.
				// The reduction is done in the subroutine at 0x605D. Here we reset A and Y to 0 so that no HP is reduced when STV
				if (MemGetMainPtr(PARTY_STATUS_START + regs.x)[0] == 0x02)	// active char is STV
				{
					regs.a = 0;
					regs.y = 0;
				}
				break;
			}
			case PC_CHAR_SWAMP_DAMAGE:
			{
				// this is a JSR to a swamp damage routine that triggers for each char (regs.x has the char position)
				bool avoidsSwampDamage = false;
				// the following types never get damaged by swamp
				switch ((DeathlordClasses)(MemGetMainPtr(PARTY_CLASS_START + regs.x)[0]))
				{
				case DeathlordClasses::Barbarian:
					[[fallthrough]];
				case DeathlordClasses::Druid:
					[[fallthrough]];
				case DeathlordClasses::Peasant:
					[[fallthrough]];
				case DeathlordClasses::Ranger:
					avoidsSwampDamage = true;
					break;
				default:
					avoidsSwampDamage = false;
					break;
				}
				switch ((DeathlordRaces)(MemGetMainPtr(PARTY_RACE_START + regs.x)[0]))
				{
				case DeathlordRaces::Elf:
					[[fallthrough]];
				case DeathlordRaces::HalfElf:
					[[fallthrough]];
				case DeathlordRaces::Orc:
					[[fallthrough]];
				case DeathlordRaces::HalfOrc:
					avoidsSwampDamage = true;
					break;
				default:
					avoidsSwampDamage = false;
					break;
				}
				if (avoidsSwampDamage)
				{
					CYC(6); // JSR (0x20) uses 6 cycles;
					regs.pc = _origPC + 3;	// Skip to the next instruction
				}
				break;
			}
			case PC_SAVE_AFTER_DEATH:
			{
				CYC(2); // BNE uses 2 cycles;
				regs.pc = _origPC + 2;	// Jump to the next instruction
				break;
			}
			case PC_NINJA_MONK_AC_RESET:
			{
				// There's a bug that ANDs the ninja/monk AC with 0F, so it can never get above 15.
				// The AC is 1/2 of the level, so at level 32 it resets to 0.
				// (when displayed, the AC is substracted from 10 to display for example "Armor: -5")
				CYC(2); // AND uses 2 cycles;
				regs.pc = _origPC + 2;	// Jump to the next instruction
				break;
			}
			default:
				break;
			}	// switch
		}

		/// END DEATHLORD HOOKS
		

		HEATMAP_X(regs.pc);
		Fetch(iOpcode, uExecutedCycles);

#ifdef _DEBUG_INSTRUCTIONS

		if ((_origPC == 0xA9A4))
			numInstructions = 1000;

		if (numInstructions > 0)
		{
			--numInstructions;
			WORD addr = *(LPWORD)(mem + _origPC);
			std::string _opStr;
			switch (iOpcode)
			{
			case 0x00: _opStr = "BRK"; break;
			case 0x01: _opStr = "ORA"; break;
			case 0x02: _opStr = "NOP"; break;
			case 0x03: _opStr = "NOP"; break;
			case 0x04: _opStr = "TSB"; break;
			case 0x05: _opStr = "ORA"; break;
			case 0x06: _opStr = "ASL"; break;
			case 0x07: _opStr = "NOP"; break;
			case 0x08: _opStr = "PHP"; break;
			case 0x09: _opStr = "ORA"; break;
			case 0x0A: _opStr = "asl"; break;
			case 0x0B: _opStr = "NOP"; break;
			case 0x0C: _opStr = "TSB"; break;
			case 0x0D: _opStr = "ORA"; break;
			case 0x0E: _opStr = "ASL"; break;
			case 0x0F: _opStr = "NOP"; break;
			case 0x10: _opStr = "BPL"; break;
			case 0x11: _opStr = "ORA"; break;
			case 0x12: _opStr = "ORA"; break;
			case 0x13: _opStr = "NOP"; break;
			case 0x14: _opStr = "TRB"; break;
			case 0x15: _opStr = "ORA"; break;
			case 0x16: _opStr = "ASL"; break;
			case 0x17: _opStr = "NOP"; break;
			case 0x18: _opStr = "CLC"; break;
			case 0x19: _opStr = "ORA"; break;
			case 0x1A: _opStr = "INA"; break;
			case 0x1B: _opStr = "NOP"; break;
			case 0x1C: _opStr = "TRB"; break;
			case 0x1D: _opStr = "ORA"; break;
			case 0x1E: _opStr = "ASL"; break;
			case 0x1F: _opStr = "NOP"; break;
			case 0x20: _opStr = "JSR"; break;
			case 0x21: _opStr = "AND"; break;
			case 0x22: _opStr = "NOP"; break;
			case 0x23: _opStr = "NOP"; break;
			case 0x24: _opStr = "BIT"; break;
			case 0x25: _opStr = "AND"; break;
			case 0x26: _opStr = "ROL"; break;
			case 0x27: _opStr = "NOP"; break;
			case 0x28: _opStr = "PLP"; break;
			case 0x29: _opStr = "AND"; break;
			case 0x2A: _opStr = "rol"; break;
			case 0x2B: _opStr = "NOP"; break;
			case 0x2C: _opStr = "BIT"; break;
			case 0x2D: _opStr = "AND"; break;
			case 0x2E: _opStr = "ROL"; break;
			case 0x2F: _opStr = "NOP"; break;
			case 0x30: _opStr = "BMI"; break;
			case 0x31: _opStr = "AND"; break;
			case 0x32: _opStr = "AND"; break;
			case 0x33: _opStr = "NOP"; break;
			case 0x34: _opStr = "BIT"; break;
			case 0x35: _opStr = "AND"; break;
			case 0x36: _opStr = "ROL"; break;
			case 0x37: _opStr = "NOP"; break;
			case 0x38: _opStr = "SEC"; break;
			case 0x39: _opStr = "AND"; break;
			case 0x3A: _opStr = "DEA"; break;
			case 0x3B: _opStr = "NOP"; break;
			case 0x3C: _opStr = "BIT"; break;
			case 0x3D: _opStr = "AND"; break;
			case 0x3E: _opStr = "ROL"; break;
			case 0x3F: _opStr = "NOP"; break;
			case 0x40: _opStr = "RTI"; break;
			case 0x41: _opStr = "EOR"; break;
			case 0x42: _opStr = "NOP"; break;
			case 0x43: _opStr = "NOP"; break;
			case 0x44: _opStr = "NOP"; break;
			case 0x45: _opStr = "EOR"; break;
			case 0x46: _opStr = "LSR"; break;
			case 0x47: _opStr = "NOP"; break;
			case 0x48: _opStr = "PHA"; break;
			case 0x49: _opStr = "EOR"; break;
			case 0x4A: _opStr = "lsr"; break;
			case 0x4B: _opStr = "NOP"; break;
			case 0x4C: _opStr = "JMP"; break;
			case 0x4D: _opStr = "EOR"; break;
			case 0x4E: _opStr = "LSR"; break;
			case 0x4F: _opStr = "NOP"; break;
			case 0x50: _opStr = "BVC"; break;
			case 0x51: _opStr = "EOR"; break;
			case 0x52: _opStr = "EOR"; break;
			case 0x53: _opStr = "NOP"; break;
			case 0x54: _opStr = "NOP"; break;
			case 0x55: _opStr = "EOR"; break;
			case 0x56: _opStr = "LSR"; break;
			case 0x57: _opStr = "NOP"; break;
			case 0x58: _opStr = "CLI"; break;
			case 0x59: _opStr = "EOR"; break;
			case 0x5A: _opStr = "PHY"; break;
			case 0x5B: _opStr = "NOP"; break;
			case 0x5C: _opStr = "NOP"; break;
			case 0x5D: _opStr = "EOR"; break;
			case 0x5E: _opStr = "LSR"; break;
			case 0x5F: _opStr = "NOP"; break;
			case 0x60: _opStr = "RTS"; break;
			case 0x61: _opStr = "ADC"; break;
			case 0x62: _opStr = "NOP"; break;
			case 0x63: _opStr = "NOP"; break;
			case 0x64: _opStr = "STZ"; break;
			case 0x65: _opStr = "ADC"; break;
			case 0x66: _opStr = "ROR"; break;
			case 0x67: _opStr = "NOP"; break;
			case 0x68: _opStr = "PLA"; break;
			case 0x69: _opStr = "ADC"; break;
			case 0x6A: _opStr = "ror"; break;
			case 0x6B: _opStr = "NOP"; break;
			case 0x6C: _opStr = "JMP"; break;
			case 0x6D: _opStr = "ADC"; break;
			case 0x6E: _opStr = "ROR"; break;
			case 0x6F: _opStr = "NOP"; break;
			case 0x70: _opStr = "BVS"; break;
			case 0x71: _opStr = "ADC"; break;
			case 0x72: _opStr = "ADC"; break;
			case 0x73: _opStr = "NOP"; break;
			case 0x74: _opStr = "STZ"; break;
			case 0x75: _opStr = "ADC"; break;
			case 0x76: _opStr = "ROR"; break;
			case 0x77: _opStr = "NOP"; break;
			case 0x78: _opStr = "SEI"; break;
			case 0x79: _opStr = "ADC"; break;
			case 0x7A: _opStr = "PLY"; break;
			case 0x7B: _opStr = "NOP"; break;
			case 0x7C: _opStr = "JMP"; break;
			case 0x7D: _opStr = "ADC"; break;
			case 0x7E: _opStr = "ROR"; break;
			case 0x7F: _opStr = "NOP"; break;
			case 0x80: _opStr = "BRA"; break;
			case 0x81: _opStr = "STA"; break;
			case 0x82: _opStr = "NOP"; break;
			case 0x83: _opStr = "NOP"; break;
			case 0x84: _opStr = "STY"; break;
			case 0x85: _opStr = "STA"; break;
			case 0x86: _opStr = "STX"; break;
			case 0x87: _opStr = "NOP"; break;
			case 0x88: _opStr = "DEY"; break;
			case 0x89: _opStr = "BIT"; break;
			case 0x8A: _opStr = "TXA"; break;
			case 0x8B: _opStr = "NOP"; break;
			case 0x8C: _opStr = "STY"; break;
			case 0x8D: _opStr = "STA"; break;
			case 0x8E: _opStr = "STX"; break;
			case 0x8F: _opStr = "NOP"; break;
			case 0x90: _opStr = "BCC"; break;
			case 0x91: _opStr = "STA"; break;
			case 0x92: _opStr = "STA"; break;
			case 0x93: _opStr = "NOP"; break;
			case 0x94: _opStr = "STY"; break;
			case 0x95: _opStr = "STA"; break;
			case 0x96: _opStr = "STX"; break;
			case 0x97: _opStr = "NOP"; break;
			case 0x98: _opStr = "TYA"; break;
			case 0x99: _opStr = "STA"; break;
			case 0x9A: _opStr = "TXS"; break;
			case 0x9B: _opStr = "NOP"; break;
			case 0x9C: _opStr = "STZ"; break;
			case 0x9D: _opStr = "STA"; break;
			case 0x9E: _opStr = "STZ"; break;
			case 0x9F: _opStr = "NOP"; break;
			case 0xA0: _opStr = "LDY"; break;
			case 0xA1: _opStr = "LDA"; break;
			case 0xA2: _opStr = "LDX"; break;
			case 0xA3: _opStr = "NOP"; break;
			case 0xA4: _opStr = "LDY"; break;
			case 0xA5: _opStr = "LDA"; break;
			case 0xA6: _opStr = "LDX"; break;
			case 0xA7: _opStr = "NOP"; break;
			case 0xA8: _opStr = "TAY"; break;
			case 0xA9: _opStr = "LDA"; break;
			case 0xAA: _opStr = "TAX"; break;
			case 0xAB: _opStr = "NOP"; break;
			case 0xAC: _opStr = "LDY"; break;
			case 0xAD: _opStr = "LDA"; break;
			case 0xAE: _opStr = "LDX"; break;
			case 0xAF: _opStr = "NOP"; break;
			case 0xB0: _opStr = "BCS"; break;
			case 0xB1: _opStr = "LDA"; break;
			case 0xB2: _opStr = "LDA"; break;
			case 0xB3: _opStr = "NOP"; break;
			case 0xB4: _opStr = "LDY"; break;
			case 0xB5: _opStr = "LDA"; break;
			case 0xB6: _opStr = "LDX"; break;
			case 0xB7: _opStr = "NOP"; break;
			case 0xB8: _opStr = "CLV"; break;
			case 0xB9: _opStr = "LDA"; break;
			case 0xBA: _opStr = "TSX"; break;
			case 0xBB: _opStr = "NOP"; break;
			case 0xBC: _opStr = "LDY"; break;
			case 0xBD: _opStr = "LDA"; break;
			case 0xBE: _opStr = "LDX"; break;
			case 0xBF: _opStr = "NOP"; break;
			case 0xC0: _opStr = "CPY"; break;
			case 0xC1: _opStr = "CMP"; break;
			case 0xC2: _opStr = "NOP"; break;
			case 0xC3: _opStr = "NOP"; break;
			case 0xC4: _opStr = "CPY"; break;
			case 0xC5: _opStr = "CMP"; break;
			case 0xC6: _opStr = "DEC"; break;
			case 0xC7: _opStr = "NOP"; break;
			case 0xC8: _opStr = "INY"; break;
			case 0xC9: _opStr = "CMP"; break;
			case 0xCA: _opStr = "DEX"; break;
			case 0xCB: _opStr = "NOP"; break;
			case 0xCC: _opStr = "CPY"; break;
			case 0xCD: _opStr = "CMP"; break;
			case 0xCE: _opStr = "DEC"; break;
			case 0xCF: _opStr = "NOP"; break;
			case 0xD0: _opStr = "BNE"; break;
			case 0xD1: _opStr = "CMP"; break;
			case 0xD2: _opStr = "CMP"; break;
			case 0xD3: _opStr = "NOP"; break;
			case 0xD4: _opStr = "NOP"; break;
			case 0xD5: _opStr = "CMP"; break;
			case 0xD6: _opStr = "DEC"; break;
			case 0xD7: _opStr = "NOP"; break;
			case 0xD8: _opStr = "CLD"; break;
			case 0xD9: _opStr = "CMP"; break;
			case 0xDA: _opStr = "PHX"; break;
			case 0xDB: _opStr = "NOP"; break;
			case 0xDC: _opStr = "LDD"; break;
			case 0xDD: _opStr = "CMP"; break;
			case 0xDE: _opStr = "DEC"; break;
			case 0xDF: _opStr = "NOP"; break;
			case 0xE0: _opStr = "CPX"; break;
			case 0xE1: _opStr = "SBC"; break;
			case 0xE2: _opStr = "NOP"; break;
			case 0xE3: _opStr = "NOP"; break;
			case 0xE4: _opStr = "CPX"; break;
			case 0xE5: _opStr = "SBC"; break;
			case 0xE6: _opStr = "INC"; break;
			case 0xE7: _opStr = "NOP"; break;
			case 0xE8: _opStr = "INX"; break;
			case 0xE9: _opStr = "SBC"; break;
			case 0xEA: _opStr = "NOP"; break;
			case 0xEB: _opStr = "NOP"; break;
			case 0xEC: _opStr = "CPX"; break;
			case 0xED: _opStr = "SBC"; break;
			case 0xEE: _opStr = "INC"; break;
			case 0xEF: _opStr = "NOP"; break;
			case 0xF0: _opStr = "BEQ"; break;
			case 0xF1: _opStr = "SBC"; break;
			case 0xF2: _opStr = "SBC"; break;
			case 0xF3: _opStr = "NOP"; break;
			case 0xF4: _opStr = "NOP"; break;
			case 0xF5: _opStr = "SBC"; break;
			case 0xF6: _opStr = "INC"; break;
			case 0xF7: _opStr = "NOP"; break;
			case 0xF8: _opStr = "SED"; break;
			case 0xF9: _opStr = "SBC"; break;
			case 0xFA: _opStr = "PLX"; break;
			case 0xFB: _opStr = "NOP"; break;
			case 0xFC: _opStr = "LDD"; break;
			case 0xFD: _opStr = "SBC"; break;
			case 0xFE: _opStr = "INC"; break;
			case 0xFF: _opStr = "NOP"; break;
			}

			char _buf[300];
			sprintf_s(_buf, 300, "%04X: %s  %02X %02X%02X - %02X %02X %02X\n", _origPC, _opStr.c_str(), 
				MemGetRealMainPtr(_origPC), MemGetRealMainPtr(_origPC + 1)[0], MemGetRealMainPtr(_origPC + 2)[0],
				regs.a, regs.x, regs.y);
			OutputDebugStringA(_buf);
		}
#endif

		switch (iOpcode)
		{
			// TODO-MP Optimization Note: ?? Move CYC(#) to array ??
		case 0x00:            BRK  CYC(7)  break;
		case 0x01: idx        ORA  CYC(6)  break;
		case 0x02: IMM        NOP  CYC(2)  break;	// invalid
		case 0x03:            NOP  CYC(1)  break;	// invalid
		case 0x04: ZPG        TSB  CYC(5)  break;
		case 0x05: ZPG        ORA  CYC(3)  break;
		case 0x06: ZPG        ASLc CYC(5)  break;
		case 0x07:            NOP  CYC(1)  break;	// invalid
		case 0x08:            PHP  CYC(3)  break;
		case 0x09: IMM        ORA  CYC(2)  break;
		case 0x0A:            asl  CYC(2)  break;
		case 0x0B:            NOP  CYC(1)  break;	// invalid
		case 0x0C: ABS        TSB  CYC(6)  break;
		case 0x0D: ABS        ORA  CYC(4)  break;
		case 0x0E: ABS        ASLc CYC(6)  break;
		case 0x0F:            NOP  CYC(1)  break;	// invalid
		case 0x10: REL        BPL  CYC(2)  break;
		case 0x11: INDY_OPT   ORA  CYC(5)  break;
		case 0x12: izp        ORA  CYC(5)  break;
		case 0x13:            NOP  CYC(1)  break;	// invalid
		case 0x14: ZPG        TRB  CYC(5)  break;
		case 0x15: zpx        ORA  CYC(4)  break;
		case 0x16: zpx        ASLc CYC(6)  break;
		case 0x17:            NOP  CYC(1)  break;	// invalid
		case 0x18:            CLC  CYC(2)  break;
		case 0x19: ABSY_OPT   ORA  CYC(4)  break;
		case 0x1A:            INA  CYC(2)  break;
		case 0x1B:            NOP  CYC(1)  break;	// invalid
		case 0x1C: ABS        TRB  CYC(6)  break;
		case 0x1D: ABSX_OPT   ORA  CYC(4)  break;
		case 0x1E: ABSX_OPT   ASLc CYC(6)  break;
		case 0x1F:            NOP  CYC(1)  break;	// invalid
		case 0x20: ABS        JSR  CYC(6)  break;
		case 0x21: idx        AND  CYC(6)  break;
		case 0x22: IMM        NOP  CYC(2)  break;	// invalid
		case 0x23:            NOP  CYC(1)  break;	// invalid
		case 0x24: ZPG        BIT  CYC(3)  break;
		case 0x25: ZPG        AND  CYC(3)  break;
		case 0x26: ZPG        ROLc CYC(5)  break;
		case 0x27:            NOP  CYC(1)  break;	// invalid
		case 0x28:            PLP  CYC(4)  break;
		case 0x29: IMM        AND  CYC(2)  break;
		case 0x2A:            rol  CYC(2)  break;
		case 0x2B:            NOP  CYC(1)  break;	// invalid
		case 0x2C: ABS        BIT  CYC(4)  break;
		case 0x2D: ABS        AND  CYC(4)  break;
		case 0x2E: ABS        ROLc CYC(6)  break;
		case 0x2F:            NOP  CYC(1)  break;	// invalid
		case 0x30: REL        BMI  CYC(2)  break;
		case 0x31: INDY_OPT   AND  CYC(5)  break;
		case 0x32: izp        AND  CYC(5)  break;
		case 0x33:            NOP  CYC(1)  break;	// invalid
		case 0x34: zpx        BIT  CYC(4)  break;
		case 0x35: zpx        AND  CYC(4)  break;
		case 0x36: zpx        ROLc CYC(6)  break;
		case 0x37:            NOP  CYC(1)  break;	// invalid
		case 0x38:            SEC  CYC(2)  break;
		case 0x39: ABSY_OPT   AND  CYC(4)  break;
		case 0x3A:            DEA  CYC(2)  break;
		case 0x3B:            NOP  CYC(1)  break;	// invalid
		case 0x3C: ABSX_OPT   BIT  CYC(4)  break;
		case 0x3D: ABSX_OPT   AND  CYC(4)  break;
		case 0x3E: ABSX_OPT   ROLc CYC(6)  break;
		case 0x3F:            NOP  CYC(1)  break;	// invalid
		case 0x40:            RTI  CYC(6)  DoIrqProfiling(uExecutedCycles); break;
		case 0x41: idx        EOR  CYC(6)  break;
		case 0x42: IMM        NOP  CYC(2)  break;	// invalid
		case 0x43:            NOP  CYC(1)  break;	// invalid
		case 0x44: ZPG        NOP  CYC(3)  break;	// invalid
		case 0x45: ZPG        EOR  CYC(3)  break;
		case 0x46: ZPG        LSRc CYC(5)  break;
		case 0x47:            NOP  CYC(1)  break;	// invalid
		case 0x48:            PHA  CYC(3)  break;
		case 0x49: IMM        EOR  CYC(2)  break;
		case 0x4A:            lsr  CYC(2)  break;
		case 0x4B:            NOP  CYC(1)  break;	// invalid
		case 0x4C: ABS        JMP  CYC(3)  break;
		case 0x4D: ABS        EOR  CYC(4)  break;
		case 0x4E: ABS        LSRc CYC(6)  break;
		case 0x4F:            NOP  CYC(1)  break;	// invalid
		case 0x50: REL        BVC  CYC(2)  break;
		case 0x51: INDY_OPT   EOR  CYC(5)  break;
		case 0x52: izp        EOR  CYC(5)  break;
		case 0x53:            NOP  CYC(1)  break;	// invalid
		case 0x54: zpx        NOP  CYC(4)  break;	// invalid
		case 0x55: zpx        EOR  CYC(4)  break;
		case 0x56: zpx        LSRc CYC(6)  break;
		case 0x57:            NOP  CYC(1)  break;	// invalid
		case 0x58:            CLI  CYC(2)  break;
		case 0x59: ABSY_OPT   EOR  CYC(4)  break;
		case 0x5A:            PHY  CYC(3)  break;
		case 0x5B:            NOP  CYC(1)  break;	// invalid
		case 0x5C: ABS        NOP  CYC(8)  break;	// invalid
		case 0x5D: ABSX_OPT   EOR  CYC(4)  break;
		case 0x5E: ABSX_OPT   LSRc CYC(6)  break;
		case 0x5F:            NOP  CYC(1)  break;	// invalid
		case 0x60:            RTS  CYC(6)  break;
		case 0x61: idx        ADCc CYC(6)  break;
		case 0x62: IMM        NOP  CYC(2)  break;	// invalid
		case 0x63:            NOP  CYC(1)  break;	// invalid
		case 0x64: ZPG        STZ  CYC(3)  break;
		case 0x65: ZPG        ADCc CYC(3)  break;
		case 0x66: ZPG        RORc CYC(5)  break;
		case 0x67:            NOP  CYC(1)  break;	// invalid
		case 0x68:            PLA  CYC(4)  break;
		case 0x69: IMM        ADCc CYC(2)  break;
		case 0x6A:            ror  CYC(2)  break;
		case 0x6B:            NOP  CYC(1)  break;	// invalid
		case 0x6C: IABS_CMOS  JMP  CYC(6)  break;
		case 0x6D: ABS        ADCc CYC(4)  break;
		case 0x6E: ABS        RORc CYC(6)  break;
		case 0x6F:            NOP  CYC(1)  break;	// invalid
		case 0x70: REL        BVS  CYC(2)  break;
		case 0x71: INDY_OPT   ADCc CYC(5)  break;
		case 0x72: izp        ADCc CYC(5)  break;
		case 0x73:            NOP  CYC(1)  break;	// invalid
		case 0x74: zpx        STZ  CYC(4)  break;
		case 0x75: zpx        ADCc CYC(4)  break;
		case 0x76: zpx        RORc CYC(6)  break;
		case 0x77:            NOP  CYC(1)  break;	// invalid
		case 0x78:            SEI  CYC(2)  break;
		case 0x79: ABSY_OPT   ADCc CYC(4)  break;
		case 0x7A:            PLY  CYC(4)  break;
		case 0x7B:            NOP  CYC(1)  break;	// invalid
		case 0x7C: IABSX      JMP  CYC(6)  break;
		case 0x7D: ABSX_OPT   ADCc CYC(4)  break;
		case 0x7E: ABSX_OPT   RORc CYC(6)  break;
		case 0x7F:            NOP  CYC(1)  break;	// invalid
		case 0x80: REL        BRA  CYC(2)  break;
		case 0x81: idx        STA  CYC(6)  break;
		case 0x82: IMM        NOP  CYC(2)  break;	// invalid
		case 0x83:            NOP  CYC(1)  break;	// invalid
		case 0x84: ZPG        STY  CYC(3)  break;
		case 0x85: ZPG        STA  CYC(3)  break;
		case 0x86: ZPG        STX  CYC(3)  break;
		case 0x87:            NOP  CYC(1)  break;	// invalid
		case 0x88:            DEY  CYC(2)  break;
		case 0x89: IMM        BITI CYC(2)  break;
		case 0x8A:            TXA  CYC(2)  break;
		case 0x8B:            NOP  CYC(1)  break;	// invalid
		case 0x8C: ABS        STY  CYC(4)  break;
		case 0x8D: ABS        STA  CYC(4)  break;
		case 0x8E: ABS        STX  CYC(4)  break;
		case 0x8F:            NOP  CYC(1)  break;	// invalid
		case 0x90: REL        BCC  CYC(2)  break;
		case 0x91: INDY_CONST STA  CYC(6)  break;
		case 0x92: izp        STA  CYC(5)  break;
		case 0x93:            NOP  CYC(1)  break;	// invalid
		case 0x94: zpx        STY  CYC(4)  break;
		case 0x95: zpx        STA  CYC(4)  break;
		case 0x96: zpy        STX  CYC(4)  break;
		case 0x97:            NOP  CYC(1)  break;	// invalid
		case 0x98:            TYA  CYC(2)  break;
		case 0x99: ABSY_CONST STA  CYC(5)  break;
		case 0x9A:            TXS  CYC(2)  break;
		case 0x9B:            NOP  CYC(1)  break;	// invalid
		case 0x9C: ABS        STZ  CYC(4)  break;
		case 0x9D: ABSX_CONST STA  CYC(5)  break;
		case 0x9E: ABSX_CONST STZ  CYC(5)  break;
		case 0x9F:            NOP  CYC(1)  break;	// invalid
		case 0xA0: IMM        LDY  CYC(2)  break;
		case 0xA1: idx        LDA  CYC(6)  break;
		case 0xA2: IMM        LDX  CYC(2)  break;
		case 0xA3:            NOP  CYC(1)  break;	// invalid
		case 0xA4: ZPG        LDY  CYC(3)  break;
		case 0xA5: ZPG        LDA  CYC(3)  break;
		case 0xA6: ZPG        LDX  CYC(3)  break;
		case 0xA7:            NOP  CYC(1)  break;	// invalid
		case 0xA8:            TAY  CYC(2)  break;
		case 0xA9: IMM        LDA  CYC(2)  break;
		case 0xAA:            TAX  CYC(2)  break;
		case 0xAB:            NOP  CYC(1)  break;	// invalid
		case 0xAC: ABS        LDY  CYC(4)  break;
		case 0xAD: ABS        LDA  CYC(4)  break;
		case 0xAE: ABS        LDX  CYC(4)  break;
		case 0xAF:            NOP  CYC(1)  break;	// invalid
		case 0xB0: REL        BCS  CYC(2)  break;
		case 0xB1: INDY_OPT   LDA  CYC(5)  break;
		case 0xB2: izp        LDA  CYC(5)  break;
		case 0xB3:            NOP  CYC(1)  break;	// invalid
		case 0xB4: zpx        LDY  CYC(4)  break;
		case 0xB5: zpx        LDA  CYC(4)  break;
		case 0xB6: zpy        LDX  CYC(4)  break;
		case 0xB7:            NOP  CYC(1)  break;	// invalid
		case 0xB8:            CLV  CYC(2)  break;
		case 0xB9: ABSY_OPT   LDA  CYC(4)  break;
		case 0xBA:            TSX  CYC(2)  break;
		case 0xBB:            NOP  CYC(1)  break;	// invalid
		case 0xBC: ABSX_OPT   LDY  CYC(4)  break;
		case 0xBD: ABSX_OPT   LDA  CYC(4)  break;
		case 0xBE: ABSY_OPT   LDX  CYC(4)  break;
		case 0xBF:            NOP  CYC(1)  break;	// invalid
		case 0xC0: IMM        CPY  CYC(2)  break;
		case 0xC1: idx        CMP  CYC(6)  break;
		case 0xC2: IMM        NOP  CYC(2)  break;	// invalid
		case 0xC3:            NOP  CYC(1)  break;	// invalid
		case 0xC4: ZPG        CPY  CYC(3)  break;
		case 0xC5: ZPG        CMP  CYC(3)  break;
		case 0xC6: ZPG        DEC  CYC(5)  break;
		case 0xC7:            NOP  CYC(1)  break;	// invalid
		case 0xC8:            INY  CYC(2)  break;
		case 0xC9: IMM        CMP  CYC(2)  break;
		case 0xCA:            DEX  CYC(2)  break;
		case 0xCB:            NOP  CYC(1)  break;	// invalid
		case 0xCC: ABS        CPY  CYC(4)  break;
		case 0xCD: ABS        CMP  CYC(4)  break;
		case 0xCE: ABS        DEC  CYC(6)  break;
		case 0xCF:            NOP  CYC(1)  break;	// invalid
		case 0xD0: REL        BNE  CYC(2)  break;
		case 0xD1: INDY_OPT   CMP  CYC(5)  break;
		case 0xD2: izp        CMP  CYC(5)  break;
		case 0xD3:            NOP  CYC(1)  break;	// invalid
		case 0xD4: zpx        NOP  CYC(4)  break;	// invalid
		case 0xD5: zpx        CMP  CYC(4)  break;
		case 0xD6: zpx        DEC  CYC(6)  break;
		case 0xD7:            NOP  CYC(1)  break;	// invalid
		case 0xD8:            CLD  CYC(2)  break;
		case 0xD9: ABSY_OPT   CMP  CYC(4)  break;
		case 0xDA:            PHX  CYC(3)  break;
		case 0xDB:            NOP  CYC(1)  break;	// invalid
		case 0xDC: ABS        LDD  CYC(4)  break;	// invalid
		case 0xDD: ABSX_OPT   CMP  CYC(4)  break;
		case 0xDE: ABSX_CONST DEC  CYC(7)  break;
		case 0xDF:            NOP  CYC(1)  break;	// invalid
		case 0xE0: IMM        CPX  CYC(2)  break;
		case 0xE1: idx        SBCc CYC(6)  break;
		case 0xE2: IMM        NOP  CYC(2)  break;	// invalid
		case 0xE3:            NOP  CYC(1)  break;	// invalid
		case 0xE4: ZPG        CPX  CYC(3)  break;
		case 0xE5: ZPG        SBCc CYC(3)  break;
		case 0xE6: ZPG        INC  CYC(5)  break;
		case 0xE7:            NOP  CYC(1)  break;	// invalid
		case 0xE8:            INX  CYC(2)  break;
		case 0xE9: IMM        SBCc CYC(2)  break;
		case 0xEA:            NOP  CYC(2)  break;
		case 0xEB:            NOP  CYC(1)  break;	// invalid
		case 0xEC: ABS        CPX  CYC(4)  break;
		case 0xED: ABS        SBCc CYC(4)  break;
		case 0xEE: ABS        INC  CYC(6)  break;
		case 0xEF:            NOP  CYC(1)  break;	// invalid
		case 0xF0: REL        BEQ  CYC(2)  break;
		case 0xF1: INDY_OPT   SBCc CYC(5)  break;
		case 0xF2: izp        SBCc CYC(5)  break;
		case 0xF3:            NOP  CYC(1)  break;	// invalid
		case 0xF4: zpx        NOP  CYC(4)  break;	// invalid
		case 0xF5: zpx        SBCc CYC(4)  break;
		case 0xF6: zpx        INC  CYC(6)  break;
		case 0xF7:            NOP  CYC(1)  break;	// invalid
		case 0xF8:            SED  CYC(2)  break;
		case 0xF9: ABSY_OPT   SBCc CYC(4)  break;
		case 0xFA:            PLX  CYC(4)  break;
		case 0xFB:            NOP  CYC(1)  break;	// invalid
		case 0xFC: ABS        LDD  CYC(4)  break;	// invalid
		case 0xFD: ABSX_OPT   SBCc CYC(4)  break;
		case 0xFE: ABSX_CONST INC  CYC(7)  break;
		case 0xFF:            NOP  CYC(1)  break;	// invalid
		}


AFTERFETCH:
		CheckSynchronousInterruptSources(uExecutedCycles - uPreviousCycles, uExecutedCycles);
		NMI(uExecutedCycles, flagc, flagn, flagv, flagz);
		IRQ(uExecutedCycles, flagc, flagn, flagv, flagz);

		// NTSC_BEGIN
		if (bVideoUpdate)
		{
			ULONG uElapsedCycles = uExecutedCycles - uPreviousCycles;
			NTSC_VideoUpdateCycles(uElapsedCycles);
		}
		// NTSC_END

	} while (uExecutedCycles < uTotalCycles);

	EF_TO_AF // Emulator Flags to Apple Flags

		return uExecutedCycles;
}

//===========================================================================
