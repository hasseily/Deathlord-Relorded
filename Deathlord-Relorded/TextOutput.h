#pragma once
#include "DeviceResources.h"
#include "Descriptors.h"
#include <string>
#include <map>
#include <vector>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;

// The below is to determine where the char is being printed on the screen
// If X_ORIGIN changes, then we're guaranteed the string is finished (it's swapping between left and right areas)
// If Y changes, then we're also guaranteed the string is finished
constexpr UINT8	PRINT_Y_MIN = 12;	// Value of PRINT_Y below which we don't print (it's the char list)
constexpr UINT8 PRINT_CHAR_X_ORIGIN_LEFT = 1;	// Value of X_ORIGIN to check to see if printing on the left bottom area
constexpr UINT8 PRINT_CHAR_X_ORIGIN_RIGHT = 21;	// Value of X_ORIGIN to check to see if printing on any of the right areas

constexpr UINT8 PRINT_CHAR_Y_PARTYNAME = 4;			// Value of Y to check for party name
constexpr UINT8 PRINT_CHAR_X_PARTYNAME_BEGIN = 22;	// Value of X that starts the party name area
constexpr UINT8 PRINT_CHAR_X_PARTYNAME_LENGTH = 16;	// Length of party name string

constexpr UINT8 PRINT_CHAR_Y_MODULE = 13;	// Value of Y to check for right center line print (area, # of enemies)
constexpr UINT8 PRINT_CHAR_X_MODULE_BEGIN = 22;		// Value of X that starts the module area
constexpr UINT8 PRINT_CHAR_X_MODULE_LENGTH = 10;	// Length of module string

constexpr UINT8 PRINT_CHAR_Y_BILLBOARD_BEGIN = 15;	// First line of the billboard
constexpr UINT8 PRINT_CHAR_Y_BILLBOARD_END = 22;	// Last line of the billboard
constexpr UINT8 PRINT_CHAR_X_BILLBOARD_BEGIN = 21;	// Value of X that starts the billboard area
constexpr UINT8 PRINT_CHAR_X_BILLBOARD_LENGTH = 18;	// Length of a billboard string

constexpr UINT8 PRINT_CHAR_Y_KEYPRESS = 23;			// Value of Y to check for right bottom keypress print ("SPACE") ...
constexpr UINT8 PRINT_CHAR_X_KEYPRESS_BEGIN = 26;	// Value of X that starts the keypress area
constexpr UINT8 PRINT_CHAR_X_KEYPRESS_LENGTH = 7;	// Length of keypress area string

constexpr UINT8 PRINT_MAX_LOG_LINES = 30;		// Maximum log lines to display
constexpr UINT8 PRINT_CHAR_X_LOG_BEGIN = 1;	// Value of X that starts the log area
constexpr UINT8 PRINT_CHAR_X_LOG_LENGTH = 18;	// Length of a log string

enum class TextWindows
{
	Log = 0,
	PartyName,
	Player1,
	Player2,
	Player3,
	Player4,
	Player5,
	Player6,
	Module,
	Billboard,
	Keypress,
	ModuleBillboardKeypress,
	Unknown,
	count
};

class TextOutput	// Singleton
{
public:
	// Methods

	// Called from the AppleWin 6502 ASM Deathlord print routine
	// It determines which buffer to put the char in, and where to draw it on screen
	// It also automatically adds \n and clears buffers as necessary
	// The buffers are persistent across frames
	// If ch < 0x80 it's an end-of-string character
	void PrintCharRaw(unsigned char ch, TextWindows tw, UINT8 X, UINT8 Y, bool bInverse);

	// Call it to scroll a specific window
	void ScrollWindow(TextWindows tw);

	// Clears the log area (generally for a fight)
	void ClearLog();

	// Clears the billboard area
	void ClearBillboard();

	// Inverses a char in the billboard area
	void InverseLineInBillboard(UINT8 line);

	// Determines which area we're in
	TextWindows AreaForCoordinates(UINT8 xStart, UINT8 xEnd, UINT8 yStart, UINT8 yEnd);

	// Call Render() at the end of the rendering stage to draw all the strings of the frame
	// r is the rectangle of the game itself
	void Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch);

	// To print a string to the log.
	// This is used when leveling up, to give feedback on XP still needed
	void PrintWStringToLog(std::wstring ws, bool bInverse);

	// public singleton code
	static TextOutput* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new TextOutput(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static TextOutput* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~TextOutput()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();

	static TextOutput* s_instance;
	vector<pair<wstring, XMFLOAT2>>v_linesToPrint;		// lines to print on the next render
	wstring m_strModule;					// Line that shows "indoors", or enemies and # left when in combat
	wstring m_strKeypress;					// The string to ask for keyboard input, as in "[SPACE]"
	vector<pair<wstring, FontDescriptors>>m_vBillboard;	// The string for the billboard area of Deathlord (Bottom Right)
	vector<pair<wstring, FontDescriptors>>m_vLog;		// The strings for the log area of Deathlord
	UINT8 m_XModule = 0;			// Last value of X seen for each string
	UINT8 m_XKeypress = 0;			//		If it resets, it's a new string
	UINT8 m_XBillboard = 0;
	UINT8 m_YBillboard = PRINT_CHAR_Y_BILLBOARD_BEGIN;
	UINT8 m_XLog = 0;

	wchar_t ConvertChar(unsigned char ch);
	void PrintCharToModule(unsigned char ch, UINT8 X, bool bInverse);
	void PrintCharToKeypress(unsigned char ch, UINT8 X, bool bInverse);
	void PrintCharToBillboard(unsigned char ch, UINT8 X, UINT8 Y, bool bInverse);
	void PrintCharToLog(unsigned char ch, UINT8 X, bool bInverse);

	TextOutput(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_TextOutputSpriteSheet;
};

