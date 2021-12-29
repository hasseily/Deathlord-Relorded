#pragma once

#include "Emulator/Memory.h"
#include <string>
#include <utility>

/// <summary>
/// The DeathlordHacks class includes methods to do special Deathlord things, and a related window.
/// </summary>
/// 

constexpr UINT16 MAP_IS_IN_GAME_MAP = 0xFCE0;			// If set to 0xE5, player is in-game and not on title screen or utilities
constexpr UINT16 MAP_IS_OVERLAND = 0xFC10;				// Is set to 0x80 if on the overland area
constexpr UINT16 MAP_ID = 0xFC4E;						// ID of the map
constexpr UINT16 MAP_FLOOR = 0xFC4F;					// Which floor.
constexpr UINT16 MAP_TYPE = 0xFC04;						// Level "2" is the default ground for towers and dungeons. "1" for overland, "0" for towns
constexpr UINT16 MAP_XPOS = 0xFC06;						// X position of the player on a map
constexpr UINT16 MAP_YPOS = 0xFC07;						// Y position of the player on a map
constexpr UINT16 MAP_OVERLAND_X = 0xFC4B;				// X position of the overland submap
constexpr UINT16 MAP_OVERLAND_Y = 0xFC4C;				// Y position of the overland submap

constexpr UINT16 PARTY_NAME_START = 0xFD00;				// Names of the party members, fixed length of 9
constexpr UINT16 PARTY_CLASS_START = 0xFD60;			// Start of the array of party classes
constexpr UINT16 PARTY_RACE_START = 0xFD5A;				// Start of the array of party races
constexpr UINT16 PARTY_WEAP_READY_START = 0xFDEA;		// Start of the array of weapon ready status: ff = fists, 00 = melee, 01 = ranged
constexpr UINT16 PARTY_INVENTORY_START = 0xFE20;		// Start of inventory. 8 items / char, and 8 charges. Next char is 0x20 down.
constexpr UINT16 PARTY_STATUS_START = 0xFD36;			// Start of the status bitmask: 0x1:???, 0x2:STV, 0x4:TOX, 0x8:DIS, 0x10:PAR, 0x20:STN 0x40:RIP, 0x80:STO
constexpr UINT16 PARTY_ARMORCLASS_START = 0xFD3C;		// Start of Armor Class. When displayed, the AC is substracted from 10 to display
constexpr UINT16 PARTY_CHAR_LEADER = 0xFC19;			// Leader of the party (0-based)
constexpr UINT16 PARTY_CURRENT_CHAR_POS = 0xFC21;		// Char we're getting info on (0-based)
constexpr UINT16 PARTY_CURRENT_CHAR_CLASS = 0xFC22;		// Class of the party leader or active char in battle (determines icon)

constexpr UINT16 TILEVIEW_CURRENT_START = 0x0300;		// Start of the current tiles in the viewport. Ends at 0x0350. There are 9x9 tiles
constexpr UINT16 TILEVIEW_NEW_START = 0x0351;			// Start of the new tileset. Ends 0x03A1.
constexpr UINT16 TILEVIEW_CURRENT_PLAYERTILE = 0x0328;	// The player is in the center at tile 41
constexpr UINT16 TILEVIEW_NEW_PLAYERTILE = 0x037A;		// The player is in the center at tile 41	(always value 0xFF)


// All damage tiles
// NOTE: The snow swamp does no damage
constexpr UINT16 TILE_OVERLAND_CACTUS_OFFSET = 0x03;
constexpr UINT16 TILE_OVERLAND_SWAMP_OFFSET = 0x2C;
constexpr UINT16 TILE_OVERLAND_FIRE_OFFSET = 0x3C;
constexpr UINT16 TILE_DUNGEON_SWAMP_OFFSET = 0x36;
constexpr UINT16 TILE_DUNGEON_FIRE_OFFSET = 0x2D;
constexpr UINT16 TILE_DUNGEON_ACID_OFFSET = 0x2C;
constexpr UINT16 TILE_DUNGEON_MAGIC_OFFSET = 0x38;		// Magic damage tile in dungeon tileset

// Module State
constexpr UINT16 MEM_MODULE_STATE = 0x0060;
enum class ModuleStates	// in memory at MEM_MODULE_STATE
	// TODO: Any others?
{
	Combat = 0,
	Exploration = 2
};

// Printing characters on screen
constexpr UINT16 MEM_PRINT_INVERSE	= 0x00B4;			// 00 for regular, 7F for inverse glyph
constexpr UINT16 MEM_PRINT_X_ORIGIN = 0x00AD;			// Starting X when printing a string of glyphs
constexpr UINT16 MEM_PRINT_Y_ORIGIN = 0x00AA;			// Starting Y when printing a string of glyphs
constexpr UINT16 MEM_PRINT_X = 0x00AE;					// Current X when printing a string of glyphs
constexpr UINT16 MEM_PRINT_Y = 0x00AF;					// Current Y when printing a string of glyphs
constexpr UINT16 MEM_PRINT_WIDTH = 0x00AB;				// Area width for printing current string
constexpr UINT16 MEM_PRINT_HEIGHT = 0x00AC;				// Area height for printing current string

constexpr wchar_t ARRAY_DEATHLORD_CHARSET[128]{
	'.','.','.','.','.','.','.','.','.','.','.','.','.','\n','.','.',
	'.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
	' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
	'0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
	'@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W','X','Y','Z','―',' ',' ',' ',' ',	// 0x5B is wide dash that spans the whole glyph, to draw full lines
	'`','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w','x','y','z','◆',' ',' ','[',']'	// 0x7B is the cursor
};

constexpr wchar_t ARRAY_DEATHLORD_CHARSET_EOR[128]{	// The array EOR'd with 0xE5, which Deathlord uses to obfuscate strings in RAM
	'e','d','g','f','a','`','c','b','m','l','o','n','i','h','k','j',
	'u','t','w','v','q','p','s','r',' ',' ',']','[','y','x','◆','z',	// 0x1E is the cursor
	'E','D','G','F','A','@','C','B','M','L','O','N','I','H','K','J',
	'U','T','W','V','Q','P','S','R',' ',' ',' ',' ','Y','X','―','Z',	// 0x3E is wide dash­­
	'%','$','\'','&','!',' ','#','"','-',',','/','.',')','(','+','*',
	'5','4','7','6','1','0','3','2','=','<','?','>','9','8',';',':',
	'.','.','.','.','.','.','.','.','\n','.','.','.','.','.','.','.',	// mostly unused
	'.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.'		// unused
};

enum class DeathlordClasses		// in memory at 0xFD60-0xFD65
{
	Fighter = 0,
	Paladin,
	Ranger,
	Barbarian,
	Berzerker,
	Samurai,
	DarkKnight,
	Thief,
	Assassin,
	Ninja,
	Monk,
	Priest,
	Druid,
	Magician,
	Illusionist,
	Peasant
};

enum class DeathlordRaces
{
	Human = 0,
	Elf,
	HalfElf,
	Dwarf,
	Gnome,
	DarkElf,
	Orc,
	HalfOrc
};

enum class DeathlordCharStatus
{
	OK  = 0x00,
	STV = 0x02,
	TOX = 0x04,
	DIS = 0x08,
	PAR = 0x10,
	STN = 0x20,
	RIP = 0x40,
	STO = 0x80
};

static bool PlayerIsOverland() { return MemGetMainPtr(MAP_IS_OVERLAND)[0] == 0x80; };

static INT_PTR CALLBACK HacksProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

// static inside .cpp
extern bool PartyLeaderIsOfClass(DeathlordClasses aClass);
extern bool PartyLeaderIsOfClass(DeathlordClasses aClass1, DeathlordClasses aClass2);
extern bool PartyLeaderIsOfRace(DeathlordRaces aRace);
extern bool PartyHasClass(DeathlordClasses aClass);
extern bool PartyHasClass(DeathlordClasses aClass1, DeathlordClasses aClass2);
extern bool PartyHasRace(DeathlordRaces aRace);
extern std::wstring NameOfClass(DeathlordClasses aClass, bool inJapan);
extern std::wstring NameOfRace(DeathlordRaces aRace, bool inJapan);
extern std::wstring StringFromMemory(UINT16 startMem, UINT8 maxLength);

extern std::wstring& ltrim(std::wstring& str);
extern std::wstring& rtrim(std::wstring& str);

class DeathlordHacks
{

public:
	DeathlordHacks(HINSTANCE app, HWND hMainWindow);

	void SaveMapDataToDisk();
	void BackupScenarioImages();
	bool RestoreScenarioImages();
	void ShowHacksWindow();
	void HideHacksWindow();
	bool IsHacksWindowDisplayed();
	HWND hwndHacks;				// handle to hacks window

private:
	bool isDisplayed = false;
};

// defined in Main.cpp
extern std::shared_ptr<DeathlordHacks> GetDeathlordHacks();
extern bool g_isInGameMap;