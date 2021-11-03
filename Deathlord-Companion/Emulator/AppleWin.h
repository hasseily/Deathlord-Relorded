#pragma once

#include "Card.h"
#include "Common.h"
#include "Memory.h"
#include "Mockingboard.h"
#include "SoundCore.h"
#include "Speaker.h"
#include "Video.h"
#include <string>
#include "RemoteControl/RemoteControlManager.h"

static std::wstring g_pAppTitle = L"Deathlord Companion";

extern RemoteControlManager g_RemoteControlMgr;			// RIK -- Necessary to have a single instance only

extern eApple2Type g_Apple2Type;
eApple2Type GetApple2Type(void);
void SetApple2Type(eApple2Type type);

double Get6502BaseClock(void);
void SetCurrentCLK6502(void);

void SingleStep(bool bReinit);

UINT GetFrameBufferWidth();
UINT GetFrameBufferHeight();
UINT GetFrameBufferBorderWidth();
UINT GetFrameBufferBorderHeight();

void ApplyNonVolatileConfig();
void EmulatorOneTimeInitialization(HWND window);
void EmulatorRepeatInitialization();
void EmulatorReboot();
void EmulatorMessageLoopProcessing();
void EmulatorSetSpeed(WORD speed);

void UseClockMultiplier(double clockMultiplier);

extern bool       g_bFullSpeed;
extern UINT64	  g_debug_video_field;	// defined in Game.cpp // shows debug video info on screen
extern UINT64	  g_debug_video_data;	// defined in Game.cpp // shows debug video info on screen


//===========================================

// Win32
extern HINSTANCE  g_hInstance;
extern HWND g_hFrameWindow;

extern AppMode_e g_nAppMode;

extern std::string g_sProgramDir;
extern std::string g_sCurrentDir;

extern bool       g_bRestart;

extern DWORD      g_dwSpeed;
extern double     g_fCurrentCLK6502;

extern int        g_nCpuCyclesFeedback;
extern DWORD      g_dwCyclesThisFrame;

extern bool       g_bDisableDirectSound;				// Cmd line switch: don't init DS (so no MB/Speaker support)
extern bool       g_bDisableDirectSoundMockingboard;	// Cmd line switch: don't init MB support
extern int        g_nMemoryClearType;					// Cmd line switch: use specific MIP (Memory Initialization Pattern)

extern class CardManager& GetCardMgr(void);
extern class SynchronousEventManager g_SynchronousEventMgr;
