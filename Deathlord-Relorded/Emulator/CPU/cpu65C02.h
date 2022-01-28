#pragma once
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
#include "TextOutput.h"
#include "InvManager.h"
#include "AutoMap.h"
#include "BattleOverlay.h"
#include "resource.h"
#include <string>
//===========================================================================

static int __posXOrigin = 0xff;
static int __posY = 0xff;
static std::string interactiveTextOutput;
static int interactiveTextOutputLineCt = 0;
static std::shared_ptr<LogWindow> __logWindow;
extern std::shared_ptr<LogWindow> GetLogWindow();
static TextOutput* __textOutput;
static bool hasTriedInsertingScenarii = false;

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
				// The FINISH_TRANSITION trigger will only run after the game is back in the player wait loop
				// There used to be instead a PARSE_TILES trigger but that's now obsolete
				auto memT = MemoryTriggers::GetInstance();
				memT->DelayedTriggerInsert(DelayedTriggersFunction::FINISH_TRANSITION, 100);
				break;
			}
			case PC_MAP_KEY_PRESS:
			{
				// A key has been pressed when in map
				// Could be 'C' to cast, or a number for the "Cast-" prompt, or the spell menu, etc...
				// What we want is to intercept the arrow keys ONLY when this subroutine was called
				// from when we're not in a menu selection situation.
				// Arrow keys are used in menus, so can't just blindly remap them.
				if ((MemGetMainPtr(regs.sp)[1] != SP_MAP_KEY_PRESS_1)
					|| (MemGetMainPtr(regs.sp)[2] != SP_MAP_KEY_PRESS_2))
				{
					if ((MemGetMainPtr(regs.sp)[1] == SP_MAP_KEY_PRESS_SPELL_1)
						&& (MemGetMainPtr(regs.sp)[2] == SP_MAP_KEY_PRESS_SPELL_2))
					{
						// here we're in spell casting mode, but not on the menu
						// Player is typing spell, disable the mapping
						break;
					}
					// The keycode is in A. See asciicode[][] in Keyboard.cpp
					// The high bit is set
					switch (regs.a)
					{
					case 0x8B:	//up arrow
						regs.a = 'I' + 0x80;
						break;
					case 0x88:	//left arrow
						regs.a = 'J' + 0x80;
						break;
					case 0x95:	//right arrow
						regs.a = 'K' + 0x80;
						break;
					case 0x8A:	//down arrow
						regs.a = 'M' + 0x80;
						break;
					default:
						break;
					};
				};
				break;
			}
			case PC_BATTLE_AMBUSH:
				[[fallthrough]];
			case PC_BATTLE_ENTER:	// Doesn't work in dungeons
			{
				g_isInBattle = true;
				break;
			}
			case PC_BATTLE_ENEMY_HP_SET: // Works everywhere, including dungeons
			{
				// The enemy HP array has just been filled. Tell that to the battle overlay.
				g_isInBattle = true;
				BattleOverlay::GetInstance()->BattleEnemyHPIsSet();
				break;
			}
			case PC_DECREMENT_TIMER:
			{
				// Process memory triggers every time the game processes the turn pass decrement timer
				// We know here that we're in a safe place to parse the video screen, etc...
				// And we know we're not in battle
				// And furthermore we disable that timer because who in his right mind wants turns to pass when not doing anything?

				g_isInBattle = false;
				auto memT = MemoryTriggers::GetInstance();
				g_hasBeenIdleOnce = true;
				if (memT != NULL)
					memT->Process();
				CYC(6);		// NOP 6 cycles. The DEC instruction takes 6 cycles
				regs.pc = _origPC + 3;
				goto AFTERFETCH;
				break;
			}
			case PC_ALL_CHARS_DEAD:
			{
				// Prints that all players are dead.
				// TODO: Show Game Over screen?
				// TODO: Stop the game from saving when all chars are dead?
				g_isInGameTransition = false;
				g_isInBattle = false;
				g_isInGameMap = true;
				g_hasBeenIdleOnce = true;
				break;
			}
			case PC_END_DRAWING_TILES:
			{
				auto aM = AutoMap::GetInstance();
				aM->ShouldCalcTileVisibility();
				break;
			}
			case PC_SCROLL_WINDOW:
			{
				if (!__textOutput)
					__textOutput = TextOutput::GetInstance();
				TextWindows tw = __textOutput->AreaForCoordinates(
					MemGetMainPtr(MEM_PRINT_X_ORIGIN)[0], MemGetMainPtr(MEM_PRINT_WIDTH)[0],
					MemGetMainPtr(MEM_PRINT_Y_ORIGIN)[0], MemGetMainPtr(MEM_PRINT_HEIGHT)[0]
				);
				switch (tw)
				{
				case TextWindows::Log:
					__textOutput->ScrollWindow(TextWindows::Log);
					break;
				case TextWindows::Billboard:
					[[fallthrough]];
				case TextWindows::Keypress:
					[[fallthrough]];
				case TextWindows::ModuleBillboardKeypress:
					__textOutput->ScrollWindow(TextWindows::Billboard);
					break;
				default:
				{
#ifdef _DEBUG
					OutputDebugStringA("Scrolling window unknown\n");
					char _buf[300];
					sprintf_s(_buf, 300, "%04X: %02X%02X - %02X %02X %02X - %02d %02d %02d %02d\n", _origPC,
						MemGetRealMainPtr(_origPC + 2)[0], MemGetRealMainPtr(_origPC + 1)[0],
						regs.a, regs.x, regs.y,
						MemGetRealMainPtr(MEM_PRINT_X_ORIGIN)[0],
						MemGetRealMainPtr(MEM_PRINT_WIDTH)[0],
						MemGetRealMainPtr(MEM_PRINT_Y_ORIGIN)[0],
						MemGetRealMainPtr(MEM_PRINT_HEIGHT)[0]);
					OutputDebugStringA(_buf);
#endif
				}
				}
				break;
			}
			case PC_INVERSE_CHAR:
				[[fallthrough]];
			case PC_PRINT_CHAR:
			{
				// First check if it's the topright area. If so, do nothing. We don't need to display changes
				// in the list of characters
				if (MemGetMainPtr(MEM_PRINT_Y)[0] < PRINT_Y_MIN)
					break;

				unsigned int _glyph = regs.a;
#ifdef _DEBUG_XXX
				char _bufPrint[200];
				sprintf_s(_bufPrint, 200, "%c/%2X (%2X) == XOrig: %2d, YOrig: %2d, X:%2d, Y:%2d, Width: %2d, Height %2d\n",
					ARRAY_DEATHLORD_CHARSET[_glyph & 0x7F], regs.a, MemGetMainPtr(MEM_PRINT_INVERSE)[0],
					MemGetMainPtr(MEM_PRINT_X_ORIGIN)[0], MemGetMainPtr(MEM_PRINT_Y_ORIGIN)[0],
					MemGetMainPtr(MEM_PRINT_X)[0], MemGetMainPtr(MEM_PRINT_Y)[0],
					MemGetMainPtr(MEM_PRINT_WIDTH)[0], MemGetMainPtr(MEM_PRINT_HEIGHT)[0]);
				OutputDebugStringA(_bufPrint);
#endif
				if (!__textOutput)
					__textOutput = TextOutput::GetInstance();
				TextWindows tw = __textOutput->AreaForCoordinates(
					MemGetMainPtr(MEM_PRINT_X_ORIGIN)[0], MemGetMainPtr(MEM_PRINT_WIDTH)[0],
					MemGetMainPtr(MEM_PRINT_Y_ORIGIN)[0], MemGetMainPtr(MEM_PRINT_HEIGHT)[0]
				);
				__textOutput->PrintCharRaw(_glyph, tw,
					MemGetMainPtr(MEM_PRINT_X)[0], MemGetMainPtr(MEM_PRINT_Y)[0],
					MemGetMainPtr(MEM_PRINT_INVERSE)[0]);

				break;
			}
			case PC_PRINT_CLEAR_AREA:
			{
				if (!__textOutput)
					__textOutput = TextOutput::GetInstance();
				if (regs.a == 0x03)
					__textOutput->ClearLog();
				else
					__textOutput->ClearBillboard();
#ifdef _DEBUGXXX
				OutputDebugStringA("Clearing Print Area\n");
				char _buf[300];
				sprintf_s(_buf, 300, "%04X: %02X%02X - %02X %02X %02X - %02d %02d %02d %02d\n", _origPC,
					MemGetRealMainPtr(_origPC + 2)[0], MemGetRealMainPtr(_origPC + 1)[0],
					regs.a, regs.x, regs.y,
					MemGetRealMainPtr(MEM_PRINT_X_ORIGIN)[0],
					MemGetRealMainPtr(MEM_PRINT_WIDTH)[0],
					MemGetRealMainPtr(MEM_PRINT_Y_ORIGIN)[0],
					MemGetRealMainPtr(MEM_PRINT_HEIGHT)[0]);
				OutputDebugStringA(_buf);
#endif
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
				regs.pc = regs.pc + 2 + MemGetMainPtr(regs.pc + 1)[0];	// Always branch to the new instruction
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
			case PC_CHAR_TILE_DAMAGE:
			{
				// this is a JSR to a tile damage routine that triggers for each char (regs.x has the char position)

				// First handle the case of the peasant, who in Deathlord Relorded is hyper-resilient
				// and incurs no damage from tiles
				if ((DeathlordClasses)(MemGetMainPtr(PARTY_CLASS_START)[regs.x]) == DeathlordClasses::Peasant)
					goto AVOID_DAMAGE;

				// Now get certain classes and races to avoid poison only (cactii and swamp)
				// In overland and in dungeons. The other damage types always do damage
				UINT8 _tileId = MemGetMainPtr(TILEVIEW_CURRENT_PLAYERTILE)[0];
				if (PlayerIsOverland())
				{
					if (_tileId == TILE_OVERLAND_FIRE_OFFSET)
						break;
				}
				else
				{
					if (_tileId == TILE_DUNGEON_FIRE_OFFSET)
						break;
					if (_tileId == TILE_DUNGEON_ACID_OFFSET)
						break;
					if (_tileId == TILE_DUNGEON_MAGIC_OFFSET)
						break;
				}
				bool avoidsDamage = false;
				// the following types never get damaged by swamp
				switch ((DeathlordClasses)(MemGetMainPtr(PARTY_CLASS_START)[regs.x]))
				{
				case DeathlordClasses::Barbarian:
					[[fallthrough]];
				case DeathlordClasses::Druid:
					[[fallthrough]];
				case DeathlordClasses::Peasant:
					[[fallthrough]];
				case DeathlordClasses::Ranger:
					avoidsDamage = true;
					break;
				default:
					avoidsDamage = false;
					break;
				}
				switch ((DeathlordRaces)(MemGetMainPtr(PARTY_RACE_START)[regs.x]))
				{
				case DeathlordRaces::Elf:
					[[fallthrough]];
				case DeathlordRaces::HalfElf:
					[[fallthrough]];
				case DeathlordRaces::Orc:
					[[fallthrough]];
				case DeathlordRaces::HalfOrc:
					avoidsDamage = true;
					break;
				default:
					avoidsDamage = false;
					break;
				}
				if (avoidsDamage)
				{
				AVOID_DAMAGE:
					// This instruction is a JSR to the damage.
					// The next instruction is a JSR to highlight the character and beep. We bypass both
					CYC(12); // JSR (0x20) uses 6 cycles;
					regs.pc = _origPC + 6;	// Skip to the next instruction
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
			case PC_CHECK_READY_WEAPON:
			{
				// We want to use our own InventoryList.csv file to check if someone can use a weapon
				// or not. Especially the peasants, they should be able to use crossbows!
				// That was the original idea for a crossbow: for anyone to kill a knight in armor.
				// A has weapon id, X has char index, Y has Melee/Range (0/1)
				InvItem* _item = InvManager::GetInstance()->ItemWithId(regs.a);
				if (_item->canEquip((DeathlordClasses)MemGetMainPtr(PARTY_CLASS_START)[regs.x],
					(DeathlordRaces)MemGetMainPtr(PARTY_RACE_START)[regs.x]))
				{
					CYC(6); // JSR (0x20) uses 6 cycles;
					regs.pc = _origPC + 3;	// Skip to the next instruction
				}
				break;
			}
			case PC_BATTLE_CHAR_BEGIN_TURN:
			{
				BattleOverlay::GetInstance()->SetActiveActor(regs.x);
				break;
			}
			case PC_BATTLE_ENEMY_BEGIN_TURN:
			{
				BattleOverlay::GetInstance()->SetActiveActor(regs.x + 6);
				break;
			}
			case PC_BATTLE_ENEMY_BEGIN_ATK:
			{
				BattleOverlay::GetInstance()->SpriteBeginAttack(6 + MemGetMainPtr(MEM_BATTLE_ENEMY_INDEX)[0]);
				break;
			}
			case PC_BATTLE_CHAR_BEGIN_ATK:
			{
				BattleOverlay::GetInstance()->SpriteBeginAttack(MemGetMainPtr(PARTY_CURRENT_CHAR_POS)[0]);
				break;
			}
			case PC_BATTLE_ENEMY_MISSED:
			{
				BattleOverlay::GetInstance()->SpriteDodge(MemGetMainPtr(PARTY_CURRENT_CHAR_POS)[0]);
				break;
			}
			case PC_BATTLE_ENEMY_HAS_HIT:
			{
				BattleOverlay::GetInstance()->SpriteIsHit(MemGetMainPtr(PARTY_CURRENT_CHAR_POS)[0],
					MemGetMainPtr(MEM_DAMAGE_AMOUNT)[0]);
				break;
			}
			case PC_BATTLE_CHAR_HAS_HIT:
			{
				BattleOverlay::GetInstance()->SpriteIsHit(regs.x + 6, regs.a);
				break;
			}
			case PC_BATTLE_CHAR_HAS_KILLED:
			{
				BattleOverlay::GetInstance()->SpriteDied(regs.x + 6);
				break;
			}
			case PC_BATTLE_CHAR_HAS_BANISHED:
			{
				BattleOverlay::GetInstance()->SpriteDied(regs.x + 6 - 1);	// 1-based!
				break;
			}
			case PC_BATTLE_CHAR_HAS_HEALED:
			{
				BattleOverlay::GetInstance()->SpriteIsHealed(regs.x, (regs.a << 8) + regs.y);
				break;
			}
			case PC_BATTLE_BEGIN_XP_ALLOC:
			{
				// As you fight, Deathlord will increment [PC=0xA86B] for each party member a number every time
				// there's an "XP-generating event". [ This array starts at 0xAFE8]
				// When the battle is done, it will take the full XP generated by the fight 
				// and start going down the list of party members.
				// [Total battle XP is at 0xAFEE-AFEF]
				// It'll assign 1 XP to each member who has a non-zero number. [PC=0xA30C checks if zero]
				// When it reaches the end, it will loop again and keep going until the XP is all allocated.

				// this LDX #$00 starts the XP alloc routine at char 0.
				// we randomize the LDX value instead
				// and make every member be given XP!
				MemGetMainPtr(PC_BATTLE_BEGIN_XP_ALLOC)[1] = rand() % DEATHLORD_PARTY_SIZE;
				int getsXP;
				for (size_t i = 0; i < DEATHLORD_PARTY_SIZE; i++)
				{
					getsXP = MemGetMainPtr(MEM_BATTLE_GETXP_START)[i];
					if (getsXP == 0)
						MemGetMainPtr(MEM_BATTLE_GETXP_START)[i] = 1;
				}
				break;
			}
			case PC_INCREMENT_LEVEL:
			{
				// When incrementing levels, give the peasant every 5 levels a random attribute increase
				// between STR, DEX and CON
				if ((DeathlordClasses)MemGetMainPtr(PARTY_CLASS_START)[regs.x] == DeathlordClasses::Peasant)
				{
					if ((MemGetMainPtr(PARTY_LEVEL_START)[regs.x] % 5) == 4)
					{
						int _rndAtt = rand() % 3;
						int _attToChange;
						if (_rndAtt == 0)
							_attToChange = PARTY_ATTR_STR_START;
						else if (_rndAtt == 1)
							_attToChange = PARTY_ATTR_DEX_START;
						else if (_rndAtt == 2)
							_attToChange = PARTY_ATTR_CON_START;
						MemGetMainPtr(_attToChange)[regs.x] = MemGetMainPtr(_attToChange)[regs.x] + 1;
					}
				}
				break;
			}
			case PC_RESET_XP_ON_LEVELUP:
			{
				// Do not reset XP on levelup
				// Previously it did LDA #$0, and now it will STA on both low and high bytes
				CYC(5 * 2); // STA uses 5 cycles;
				regs.pc = _origPC + 6;	// Jump to after the 2 STA
				break;
			}
			case PC_LEVELUP_CHECK:
			{
				// Display "remaining XP to level" if one can't level at the academy
				if (regs.a == 0)	// No levelup points
				{
					if (!__textOutput)
						__textOutput = TextOutput::GetInstance();
					// Calculate remaining XP for leveling
					UINT8 _lvl = MemGetMainPtr(PARTY_LEVEL_START)[regs.x];
					if (_lvl > 21)
						_lvl = 21;
					int _xpNeeded = ARRAY_XP_REQUIREMENTS[_lvl];
					int _currXP = ((int)MemGetMainPtr(PARTY_XP_HIBYTE_START)[regs.x] << 8) +
						MemGetMainPtr(PARTY_XP_LOBYTE_START)[regs.x];
					std::wstring _wsLvl = L"Missing ";
					_wsLvl.append(to_wstring(_xpNeeded - _currXP) + std::wstring(L" XP"));
					__textOutput->PrintWStringToLog(_wsLvl, true);
				}
				break;
			}
			default:
				break;
			}	// switch

		} // if is in game map
		else
		{
			switch (_origPC)
			{
			case PC_CHECK_KEYPRESS_TITLE:
			{
				if (EmulatorGetSpeed() != 1)
					EmulatorSetSpeed(1);
				g_startMenuState = StartMenuState::Title;
				break;
			}
			case PC_CHECK_KEYPRESS_MENU:
			{
				if (regs.a > 0x7F)	// key was pressed
					g_startMenuState = StartMenuState::Other;	// Could be anything, including character creation
				else
					g_startMenuState = StartMenuState::Menu;
				break;
			}
			case PC_PROMPT_USE_2_DRIVES:
			{
				EmulatorSetSpeed(5);	// Accelerate the loading;
				regs.pc = 0x8738;	// Bypass the 2 drives prompt (use 2 drives)
				hasTriedInsertingScenarii = false;
				break;
			}
			case PC_PROMPT_INSERT_SCENARIOS:
			{
				g_startMenuState = StartMenuState::PromptScenarios;
				if (!hasTriedInsertingScenarii)
				{
					// let's auto-insert scenarii
					CYC(6); // JSR (0x20) uses 6 cycles;
					regs.pc = _origPC + 3;	// Skip to the next instruction
					// Synchronously insert scenarii
					SendMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_EMULATOR_INSERTSCENARIODISKS, 1);
					hasTriedInsertingScenarii = true;
				}
				break;
			}
			case PC_SCENARIOS_ARE_IN_DRIVES:
			{
				g_isInGameTransition = true;
				g_startMenuState = StartMenuState::LoadingGame;
				break;
			}
			default:
				break;
			}
		}

		/// END DEATHLORD HOOKS


		HEATMAP_X(regs.pc);
		Fetch(iOpcode, uExecutedCycles);

#ifdef _DEBUG

		if (g_debugLogInstructions > 0)
		{
			--g_debugLogInstructions;
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
			sprintf_s(_buf, 300, "%04X: %s  %02X%02X - %02X %02X %02X\n", _origPC, _opStr.c_str(),
				MemGetRealMainPtr(_origPC + 1)[0], MemGetRealMainPtr(_origPC + 2)[0],
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
