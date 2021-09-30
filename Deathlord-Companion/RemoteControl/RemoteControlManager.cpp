/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2019, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Remote Control Manager
 *
 * Handles remote control and interfaces with GameLink and potentially other interfaces
 *
 * Author: Henri Asseily
 *
 */

#include "pch.h"
#include "resource.h"
#include "RemoteControl/RemoteControlManager.h"
#include "Emulator/Applewin.h"
#include <Windows.h>		// to inject the incoming Gamelink inputs into Applewin
#include "Emulator/Memory.h"
#include "Emulator/CPU.h"
#include "Emulator/Video.h"
#include "Emulator/CardManager.h"	// for Gamelink in and out
#include "Emulator/Speaker.h"		// for Gamelink::In()
#include "Emulator/Mockingboard.h"	// for sound
#include "Emulator/DiskImageHelper.h"
#include "Gamelink.h"
#include "HAUtils.h"
#include <unordered_set>
#include <filesystem>
#include "nlohmann/json.hpp"

using namespace std;
namespace fs = std::filesystem;

#define UNKNOWN_VOLUME_NAME "Unknown Volume"
#define UNKNOWN_VERSION		"0.0.0"
#define VERSION_STRING_FILE_OFFSET 0x37CA
#define VERSION_STRING_LENGTH	5

 // The GameLink I/O structure
struct Gamelink_Block {
	// UINT pitch;	// not implemented here
	GameLink::sSharedMMapInput_R2 input_prev;
	GameLink::sSharedMMapInput_R2 input;
	GameLink::sSharedMMapAudio_R1 audio;
	UINT repeat_last_tick[256];
	bool want_mouse;
};
Gamelink_Block g_gamelink;

struct Info_HDV {
	std::string VolumeName;
	std::string sVersion;
	UINT32 sig;
};
Info_HDV g_infoHdv = { std::string(UNKNOWN_VOLUME_NAME), std::string(UNKNOWN_VERSION), 0 };

UINT64 iCurrentTicks;						// Used to check the repeat interval
UINT8 iOldVolumeLevel;
static std::unordered_set<UINT8> exclusionSet;		// list of VK codes that will not be passed through to Applewin

static bool bVideoNativeFormat = false;

bool bHardDiskIsLoaded = false;			// If HD is loaded, use it instead of floppy
bool bFloppyIsLoaded = false;

// Private Prototypes
static void reverseScanlines(uint8_t* destination, uint8_t* source, uint32_t width, uint32_t height, uint8_t depth);

//===========================================================================
// Global functions

bool RemoteControlManager::isRemoteControlEnabled()
{
	return GameLink::GetGameLinkEnabled();
}

//===========================================================================

void RemoteControlManager::setRemoteControlEnabled(bool bEnabled)
{
	return GameLink::SetGameLinkEnabled(bEnabled);
}

//===========================================================================

bool RemoteControlManager::isTrackOnlyEnabled()
{
	return GameLink::GetTrackOnlyEnabled();
}

//===========================================================================

void RemoteControlManager::setTrackOnlyEnabled(bool bEnabled)
{
	return GameLink::SetTrackOnlyEnabled(bEnabled);
}

//===========================================================================
LPBYTE RemoteControlManager::initializeMem(UINT size)
{
	if (GameLink::GetGameLinkEnabled())
	{
		iOldVolumeLevel = (UINT8)SpkrGetVolume();
		LPBYTE _mem = (LPBYTE)GameLink::AllocRAM(size);

		// initialize the gamelink previous input to 0
		memset(&g_gamelink.input_prev, 0, sizeof(GameLink::sSharedMMapInput_R2));

		(GameLink::Init(isTrackOnlyEnabled()));
		setKeypressExclusionList(aDefaultKeyExclusionList, sizeof(aDefaultKeyExclusionList));
		updateRunningProgramInfo();	// Disks might have been loaded before the shm was ready
		return _mem;
	}
	return NULL;
}

//===========================================================================

bool RemoteControlManager::destroyMem()
{
	if (GameLink::GetGameLinkEnabled())
	{
		GameLink::Term();
		return true;
	}
	return false;
}

//===========================================================================

void RemoteControlManager::setKeypressExclusionList(UINT8 _exclusionList[], UINT8 length)
{
	exclusionSet.clear();
	for (UINT8 i = 0; i < length; i++)
	{
		exclusionSet.insert(_exclusionList[i]);
	}
}

//===========================================================================

void RemoteControlManager::setLoadedHDInfo(ImageInfo* imageInfo)
{
	// pass in NULL to remove it
	if (imageInfo)
	{
		bHardDiskIsLoaded = true;
		// Get the title, and convert from wstring to string
		constexpr UINT8 maxTitleLength = 100;
		WCHAR szTitle[maxTitleLength];
		LoadStringW(HINSTANCE(0), IDS_GAMELINK_TITLE, szTitle, maxTitleLength);
		std::wstring wsTitle(szTitle);
		HA::ConvertWStrToStr(&wsTitle, &g_infoHdv.VolumeName);
		g_infoHdv.sig = 0x8ce1a6e3;		// Deathlord

		// Now find the version string that is at offset 0x37CA of the HDV file
		BYTE blockWithVersion[HD_BLOCK_SIZE];
		imageInfo->pImageType->Read(imageInfo, VERSION_STRING_FILE_OFFSET / HD_BLOCK_SIZE, blockWithVersion);
		g_infoHdv.sVersion = std::string(
			std::begin(blockWithVersion) + (VERSION_STRING_FILE_OFFSET % HD_BLOCK_SIZE),
			std::begin(blockWithVersion) + (VERSION_STRING_FILE_OFFSET % HD_BLOCK_SIZE) + VERSION_STRING_LENGTH);
		HA::ConvertUpperAsciiToStr(g_infoHdv.sVersion);
	}
	else {
		bHardDiskIsLoaded = false;
		g_infoHdv.VolumeName = std::string(UNKNOWN_VOLUME_NAME);
		g_infoHdv.sVersion = std::string(UNKNOWN_VERSION);
		g_infoHdv.sig = 0;
	}
	setVersionCPUConstants();
	updateRunningProgramInfo();
}

//===========================================================================

void RemoteControlManager::setVersionCPUConstants()
{
	fs::path constantsPath = fs::current_path();
	constantsPath += "\\Assets\\Versions.json";
	try
	{
		std::ifstream i(constantsPath);
		nlohmann::json j;
		i >> j;
		const char* version = g_infoHdv.sVersion.c_str();
		nlohmann::json ccv = j[version];
		if (ccv.empty())
			return;
		cpuconstants.A_PRINT_RIGHT			= static_cast<USHORT>(std::stoi(ccv["A_PRINT_RIGHT"].get<std::string>(), nullptr, 0));
		cpuconstants.PC_CARRIAGE_RETURN1	= static_cast<USHORT>(std::stoi(ccv["PC_CARRIAGE_RETURN1"].get<std::string>(), nullptr, 0));
		cpuconstants.PC_CARRIAGE_RETURN2	= static_cast<USHORT>(std::stoi(ccv["PC_CARRIAGE_RETURN2"].get<std::string>(), nullptr, 0));
		cpuconstants.PC_COUT				= static_cast<USHORT>(std::stoi(ccv["PC_COUT"].get<std::string>(), nullptr, 0));
		cpuconstants.PC_END_COMBAT			= static_cast<USHORT>(std::stoi(ccv["PC_END_COMBAT"].get<std::string>(), nullptr, 0));
		cpuconstants.PC_INITIATE_COMBAT		= static_cast<USHORT>(std::stoi(ccv["PC_INITIATE_COMBAT"].get<std::string>(), nullptr, 0));
		cpuconstants.PC_PRINTSTR			= static_cast<USHORT>(std::stoi(ccv["PC_PRINTSTR"].get<std::string>(), nullptr, 0));
	}
	catch (nlohmann::detail::parse_error err) {
		char buf[sizeof(err.what()) + 500];
		snprintf(buf, 500, "Error parsing version CPU constants: %s\n", err.what());
		OutputDebugStringA(buf);
	}

}
//===========================================================================

void RemoteControlManager::updateRunningProgramInfo()
{
	// Updates which program is running
	// Should only be called on re/boot

	// first, generate a version hash of the version string
	// 1.1.9 becomes 0x00010109
	// 1004  becomes 0x01000004
	UINT32 versionHash = 0;
	int ix = 0;
	for (std::string::reverse_iterator i = g_infoHdv.sVersion.rbegin(); i != g_infoHdv.sVersion.rend(); ++i) {
		if ((*i >= '0') && (*i <= '9'))		// digit between 0 and 9
		{
			versionHash += (*i - '0') << 8*ix;	// put each digit as one UINT8 of the UINT
			ix++;
		}
		if (ix == sizeof(versionHash))
			break;
	}
	//wchar_t szDbg[200];
	//wsprintf(szDbg, L"version: 0x%08x\n", versionHash);
	//OutputDebugString(szDbg);

	if (bHardDiskIsLoaded)
	{
		GameLink::SetProgramInfo(g_infoHdv.VolumeName, 0, 0, versionHash, g_infoHdv.sig);
	}
	else
	{
		g_infoHdv.VolumeName = "";
		GameLink::SetProgramInfo(g_infoHdv.VolumeName, 0, 0, 0, 0);
	}

}

//===========================================================================
void RemoteControlManager::getInput()
{
	if (
		GameLink::GetGameLinkEnabled()
		&& g_hFrameWindow != GetFocus()
		&& GameLink::In(&g_gamelink.input, &g_gamelink.audio)
		) {

		// -- Audio input
		UINT iVolMax = VOLUME_MAX;
		
		UINT iVolNow;

		if ( g_gamelink.audio.master_vol_l == 0 )
		{
			// 0 = "-Inf" dB ; which maps to iVolMax
			iVolNow = iVolMax;
		}
		else
		{
			float fVolumeLinear, fVolumedB, fVolumeForSpeaker;

			fVolumeLinear = static_cast< float >( g_gamelink.audio.master_vol_l ) / 100.0f;
			fVolumedB = 20.0f * log10f( fVolumeLinear );
			fVolumeForSpeaker = fVolumedB * iVolMax / -48.0f; // note: 1% gives a value of about -40dB, so we scale to fit 0 to iVolMax
			iVolNow = static_cast< UINT >( fVolumeForSpeaker );
		}
		
		if (iVolNow != iOldVolumeLevel)
		{
			SpkrSetVolume(iVolNow, iVolMax);
			MB_SetVolume(iVolNow, iVolMax);
			iOldVolumeLevel = (UINT8)SpkrGetVolume();
		}

		// -- Keyboard input
		{
			// Gamelink sets in shm 8 UINT32s, for a total of $FF bits
			// Using some kid of DIK keycodes.
			// Each bit will state if the scancode at that position is pressed (1) or released (0)
			// We keep a cache of the previous state, so we'll know if a key has changed state
			// and trigger the event
			// This is a map from the custom DIK scancodes to VK codes
			//HKL hKeyboardLayout = GetKeyboardLayout(0);
			UINT8 aDIKtoVK[256] = { 0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0xBD, 0xBB,
									0x08, 0x09, 0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0xDB, 0xDD, 0x0D,
									0xA2, 0x41, 0x53, 0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0xBA, 0xDE, 0xC0, 0xA0, 0xDC,
									0x5A, 0x58, 0x43, 0x56, 0x42, 0x4E, 0x4D, 0xBC, 0xBE, 0xBF, 0xA1, 0x6A, 0xA4, 0x20, 0x14,
									0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x13, 0x91, 0x24, 0x26, 0x21,
									0x6D, 0x25, 0x0C, 0x27, 0x6B, 0x23, 0x28, 0x22, 0x2D, 0x2E, 0x2C, 0x00, 0xE2, 0x7A, 0x7B,
									0x0C, 0xEE, 0xF1, 0xEA, 0xF9, 0xF5, 0xF3, 0x00, 0x00, 0xFB, 0x2F, 0x7C, 0x7D, 0x7E, 0x7F,
									0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0xED, 0x00, 0xE9, 0x00, 0xC1, 0x00, 0x00, 0x87,
									0x00, 0x00, 0x00, 0x00, 0xEB, 0x09, 0x00, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB1, 0x00, 0x00, 0x00, 0x00,
									0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x0D, 0xA3, 0x00, 0x00, 0xAD, 0xB6, 0xB3, 0x00,
									0xB2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAE, 0x00, 0xAF, 0x00, 0xB7,
									0x00, 0x00, 0xBF, 0x00, 0x2A, 0xA5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									0x00, 0x00, 0x00, 0x90, 0x00, 0x24, 0x26, 0x21, 0x00, 0x25, 0x00, 0x27, 0x00, 0x23, 0x28,
									0x22, 0x2D, 0x2E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5B, 0x5C, 0x5D, 0x00, 0x5F,
									0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0xAB, 0xA8, 0xA9, 0xA7, 0xA6, 0xAC, 0xB4, 0xB5, 0x00,
									0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									0x00, 0x00 };

			// We need to handle long keypresses
			// We set up a do-nothing timer for each key so that the first repeat isn't too fast

			iCurrentTicks = GetTickCount64();
			for (UINT8 blk = 0; blk < 8; ++blk)
			{
				const UINT old = g_gamelink.input_prev.keyb_state[blk];
				const UINT key = g_gamelink.input.keyb_state[blk];
				UINT8 scancode;
				UINT32 mask;
				UINT iKeyState;
				UINT8 iVK_Code;
				LPARAM lparam;
				UINT16 repeat;
				bool bCD, bPD;	// is current key down? is previous key down?

				for (UINT8 bit = 0; bit < 32; ++bit)
				{
					scancode = static_cast<UINT8>((blk * 32) + bit);
					mask = 1 << bit;
					bCD = (key & mask);	// key is down (bCD)
					bPD = (old & mask);	// key was down previously (bPD)
					if ((!bCD) && (!bPD))
						continue;	// the key was neither pressed previously nor now
					if (bCD && bPD)
					{
						// it's a repeat key
						if ((iCurrentTicks - g_gamelink.repeat_last_tick[scancode]) < kMinRepeatInterval)
							continue;	// drop repeat messages within kMinRepeatInterval ms
					}
					if (!bPD)	// This is the first time we're pressing the key, set the no-repeat interval
						g_gamelink.repeat_last_tick[scancode] = iCurrentTicks;
					else		// The key is already in repeat mode. Let it repeat as fast as it wants
						g_gamelink.repeat_last_tick[scancode] = 0;
					// Set up message
					iKeyState = bCD ? WM_KEYDOWN : WM_KEYUP;
					iVK_Code = aDIKtoVK[scancode];
					repeat = 1;		// Managed with ticks above
					{	// set up lparam
						lparam = repeat;
						lparam = lparam | ((LPARAM)scancode << 16);				// scancode
						lparam = lparam | ((LPARAM)(scancode > 0x7F) << 24);	// extended
						lparam = lparam | ((LPARAM)bPD << 30);				// previous key state
						lparam = lparam | ((LPARAM)!bCD << 31);		// transition state (1 for keyup)
					}
					// With PostMessage, the message goes to the highest level of AppleWin, hence controlling
					// all of AppleWin's behavior, including opening popups, etc...
					// It's not at all ideal, so we filter which keystrokes to pass in with a configurable exclusion list
					if (!exclusionSet.count(iVK_Code))
					{
						PostMessageW(g_hFrameWindow, iKeyState, iVK_Code, lparam);
#ifdef DEBUG
						LogOutput("SCANCODE, iVK, LPARAM: %04X, %04X, %04X\n", scancode, iVK_Code, lparam);
#endif DEBUG
					}
				}
			}
		}

		// We're done parsing the input. Store it as the previous state
		memcpy(&g_gamelink.input_prev, &g_gamelink.input, sizeof(GameLink::sSharedMMapInput_R2));
	}
}


//===========================================================================

void RemoteControlManager::sendOutput(LPBITMAPINFO pFramebufferinfo, UINT8 *pFramebufferbits)
{
	if (GameLink::GetGameLinkEnabled()) {
		// here send the last drawn frame to GameLink
		// We could efficiently send to GameLink g_pFramebufferbits with GetFrameBufferWidth/GetFrameBufferHeight, but the scanlines are reversed
		// We instead memcpy each scanline of the bitmap of the frame in reverse into another buffer, and pass that to GameLink.
		// When GridCartographer/GameLink allows to pass in flags specifying the x/y/w/h etc...,

		if (pFramebufferinfo == NULL)
		{
			// Don't send out video, just handle out-of-band commands
			GameLink::Out(MemGetBankPtr(0));
			return;
		}

		g_gamelink.want_mouse = false;
		// TODO: only send the framebuffer out when not in trackonly_mode

		if (g_pFramebufferbits != NULL)
		{
			GameLink::Out(
				(UINT16)pFramebufferinfo->bmiHeader.biWidth,
				(UINT16)pFramebufferinfo->bmiHeader.biHeight,
				1.0,								// image ratio
				g_gamelink.want_mouse,
				(const UINT8*)pFramebufferbits,
				MemGetBankPtr(0));					// Main memory pointer
		}
	}
}


//===========================================================================

// --------------------------------------------
// Utility
// --------------------------------------------

// The framebuffer might have its scanlines inverted, from bottom to top
// To send a correct bitmap out to a 3rd party program we need to reverse the scanlines
void reverseScanlines(uint8_t* destination, uint8_t* source, uint32_t width, uint32_t height, uint8_t depth)
{
	uint32_t linesize = width * depth;
	uint8_t* loln = source;
	uint8_t* hiln = destination + (height - 1) * (uint_fast64_t)linesize;	// first pixel of the last line
	for (size_t i = 0; i < height; i++)
	{
		memcpy(hiln, loln, linesize);
		loln = loln + linesize;
		hiln = hiln - linesize;
	}
}
