#pragma once
#include <wtypes.h>

const double _14M_NTSC = (157500000.0 / 11.0);	// 14.3181818... * 10^6
const double _14M_PAL = 14.25045e6;				// UTAIIe:3-17
const double CLK_6502_NTSC = (_14M_NTSC * 65.0) / (65.0*14.0+2.0); // 65 cycles per 912 14M clocks
const double CLK_6502_PAL  = (_14M_PAL  * 65.0) / (65.0*14.0+2.0);
//const double CLK_6502 = 23 * 44100;			// 1014300

#define NUM_SLOTS 8

#define  MAX(a,b)          (((a) > (b)) ? (a) : (b))
#define  MIN(a,b)          (((a) < (b)) ? (a) : (b))

// #define  RAMWORKS			// 8MB RamWorks III support

// Use a base freq so that DirectX (or sound h/w) doesn't have to up/down-sample
// Assume base freqs are 44.1KHz & 48KHz
const DWORD SPKR_SAMPLE_RATE = 44100;

enum class AppMode_e
{
	MODE_LOGO = 0
	, MODE_PAUSED
	, MODE_RUNNING  // 6502 is running at normal/full speed
	, MODE_UNKNOWN = -1
};

#define  SPEED_MIN         0
#define  SPEED_NORMAL      10
#define  SPEED_MAX         100

enum eIRQSRC {IS_6522=0, IS_SPEECH, IS_SSC, IS_MOUSE};

#define APPLE2E_MASK	0x10

// NB. These get persisted to the Registry & save-state file, so don't change the values for these enums!
enum eApple2Type {
					A2TYPE_APPLE2E=APPLE2E_MASK,
					A2TYPE_APPLE2EENHANCED,
					A2TYPE_MAX
				};

extern eApple2Type g_Apple2Type;
inline bool IsEnhancedIIE(void)
{
	return ( g_Apple2Type == A2TYPE_APPLE2EENHANCED );
}

enum eBUTTON {BUTTON0=0, BUTTON1};

enum eBUTTONSTATE {BUTTON_UP=0, BUTTON_DOWN};

enum {IDEVENT_TIMER_MOUSE=1, IDEVENT_TIMER_100MSEC};
