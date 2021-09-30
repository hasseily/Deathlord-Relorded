#pragma once

enum SoundType_e
{
	SOUND_NONE = 0,
	SOUND_WAVE
};

extern SoundType_e soundtype;
extern double     g_fClksPerSpkrSample;
extern bool       g_bQuieterSpeaker;
extern short      g_nSpeakerData;

void    SpkrDestroy ();
void    SpkrInitialize ();
void    SpkrReinitialize ();
void    SpkrReset();
BOOL    SpkrSetEmulationType (HWND window, SoundType_e newSoundType);
void    SpkrUpdate (DWORD);
void    SpkrUpdate_Timer();
DWORD   SpkrGetVolume();
void    SpkrSetVolume(DWORD dwVolume, DWORD dwVolumeMax);
void    Spkr_Mute();
void    Spkr_Demute();
bool    Spkr_IsActive();
bool    Spkr_DSInit();

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
