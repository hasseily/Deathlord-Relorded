#pragma once

#include "Emulator/Memory.h"
#include <string>
#include <utility>

/// <summary>
/// The DeathlordHacks class includes methods to do special Deathlord things, and a related window.
/// </summary>
/// 

constexpr UINT8 DEATHLORD_PARTY_SIZE = 6;				// DLRL DOES NOT SUPPORT A PARTY SIZE NOT 6!!!!
constexpr UINT8 DEATHLORD_INVENTORY_SLOTS = 8;

constexpr UINT16 MAP_IS_IN_GAME_MAP = 0xFCE0;			// If set to 0xE5, player is in-game and not on title screen or utilities
constexpr UINT16 MAP_IS_OVERLAND = 0xFC10;				// Is set to 0x80 if on the overland area
constexpr UINT16 MAP_ID = 0xFC4E;						// ID of the map
constexpr UINT16 MAP_FLOOR = 0xFC4F;					// Which floor.
constexpr UINT16 MAP_TYPE = 0xFC04;						// Level "2" is the default ground for towers and dungeons. "1" for overland, "0" for towns
constexpr UINT16 MAP_XPOS = 0xFC06;						// X position of the player on a map
constexpr UINT16 MAP_YPOS = 0xFC07;						// Y position of the player on a map
constexpr UINT16 MAP_OVERLAND_X = 0xFC4B;				// X position of the overland submap
constexpr UINT16 MAP_OVERLAND_Y = 0xFC4C;				// Y position of the overland submap
constexpr UINT16 MAP_VISIBILITY_RADIUS = 0xFC05;		// Visibility radius in squares around the player
constexpr UINT16 MAP_IS_HIDDEN = 0xFC25;				// 0 if visible, otherwise the party member number (1-BASED!) that succeeded in hiding

constexpr UINT16 PARTY_PARTYNAME = 0xFCF0;				// Name of the party! Fixed length 16, centered with space padding
constexpr UINT16 PARTY_NAME_START = 0xFD00;				// Names of the party members, fixed length of 9
constexpr UINT16 PARTY_CLASS_START = 0xFD60;			// Start of the array of party classes
constexpr UINT16 PARTY_RACE_START = 0xFD5A;				// Start of the array of party races
constexpr UINT16 PARTY_GENDER_START = 0xFDC0;			// 0: Male, 1: Female
constexpr UINT16 PARTY_ALIGN_START = 0xFDC6;			// 0: Good, 1: Neutral, 2: Evil
constexpr UINT16 PARTY_WEAP_READY_START = 0xFDEA;		// Start of the array of weapon ready status: ff = fists, 00 = melee, 01 = ranged
constexpr UINT16 PARTY_INVENTORY_START = 0xFE20;		// Start of inventory. 8 items / char, and 8 charges. Next char is 0x20 down.
constexpr UINT16 PARTY_STATUS_START = 0xFD36;			// Start of the status bitmask: 0x1:???, 0x2:STV, 0x4:TOX, 0x8:DIS, 0x10:PAR, 0x20:STN 0x40:RIP, 0x80:STO
constexpr UINT16 PARTY_ARMORCLASS_START = 0xFD3C;		// Start of Armor Class. When displayed, the AC is substracted from 10 to display
constexpr UINT16 PARTY_FOOD_START = 0xFDD2;				// Food. Max 100 (0x64)
constexpr UINT16 PARTY_TORCHES_START = 0xFDE4;			// Torches. Max 10
constexpr UINT16 PARTY_GOLD_LOBYTE_START = 0xFDD8;		// Gold. Max 10,000 (0x2710)
constexpr UINT16 PARTY_GOLD_HIBYTE_START = 0xFDDE;
constexpr UINT16 PARTY_XP_LOBYTE_START = 0xFDF6;		// XP needed to level: L2-5: 200. L6-9: 400. L10-13:600
constexpr UINT16 PARTY_XP_HIBYTE_START = 0xFDFC;		// etc... every 4 levels until L22 where it is fixed at 1200
constexpr UINT16 PARTY_LEVEL_START = 0xFD66;
constexpr UINT16 PARTY_LEVELPLUS_START = 0xFD6C;		// Max 2 additional "levels in waiting"
constexpr UINT16 PARTY_HEALTH_LOBYTE_START = 0xFD4E;
constexpr UINT16 PARTY_HEALTH_HIBYTE_START = 0xFD54;
constexpr UINT16 PARTY_HEALTH_MAX_LOBYTE_START = 0xFD42;
constexpr UINT16 PARTY_HEALTH_MAX_HIBYTE_START = 0xFD48;
constexpr UINT16 PARTY_POWER_START = 0xFD96;
constexpr UINT16 PARTY_POWER_MAX_START = 0xFD9C;
constexpr UINT16 PARTY_ATTR_STR_START = 0xFD72;
constexpr UINT16 PARTY_ATTR_CON_START = 0xFD78;
constexpr UINT16 PARTY_ATTR_SIZ_START = 0xFD7E;
constexpr UINT16 PARTY_ATTR_INT_START = 0xFD84;
constexpr UINT16 PARTY_ATTR_DEX_START = 0xFD8A;
constexpr UINT16 PARTY_ATTR_CHA_START = 0xFD90;
constexpr UINT16 PARTY_MAGIC_USER_TYPE_START = 0xFDA2;	// FF is not a magic user. 0 is priest, etc...

constexpr UINT16 PARTY_CHAR_LEADER = 0xFC19;			// Leader of the party (0-based)
// The below also works in combat
constexpr UINT16 PARTY_CURRENT_CHAR_POS = 0xFC21;		// Char we're getting info on (0-based), or active battle char
constexpr UINT16 PARTY_CURRENT_CHAR_CLASS = 0xFC22;		// Class of the party leader or active char in battle (determines icon)
constexpr UINT16 PARTY_ICON_TYPE = 0xFC1D;				// 0: leader icon, 1: boat

// Monsters
// The monster list starts with 0xFF
// The entries are variable length due to monster names being variable
// Monster name as usual gets its high bit set at the end.
// First byte is enemy type
// Second byte is THAC0
// Third byte is health
constexpr UINT16 MONSTER_ATTR_LIST_START = 0xD000;		// Start of the full monster list attributes
constexpr UINT16 MONSTER_ATTR_IDX_LO = 0xA4;			// Current monster's attributes memptr in MEM_MSTRATTRLIST, lo byte
constexpr UINT16 MONSTER_ATTR_IDX_HI = 0xA5;			// Current monster's attributes memptr in MEM_MSTRATTRLIST, hi byte
constexpr UINT16 MONSTER_CURR_ATTR_START = 0xAF70;		// Current monster's copy of attributes as battle starts.
constexpr UINT16 MONSTER_CURR_TYPE = 0xAF70;
constexpr UINT16 MONSTER_CURR_THAC0 = 0xAF71;
constexpr UINT16 MONSTER_CURR_HPMULT = 0xAF72;			// HP multiplier (multiplier * d7) where d7 is 7 Sided dice (rand(1-7))
constexpr UINT16 MONSTER_CURR_ARMORCLASS = 0xAF73;		// AC starts at zero and increases
constexpr UINT16 MONSTER_CURR_NUM = 0xAF74;				// Max number of enemies at start
constexpr UINT16 MONSTER_CURR_ATTACKS = 0xAF75;			// # of attacks per round
constexpr UINT16 MONSTER_CURR_DAMAGE = 0xAF76;			// Damage is 1 to this number
constexpr UINT16 MONSTER_CURR_XP = 0xAF7C;				// Amount of XP enemy gives
constexpr UINT16 MONSTER_CURR_SPACES = 0xAF7D;			// Amount of spaces to add in order to center enemy name on screen
constexpr UINT16 MONSTER_CURR_NAME = 0xAF7E;			// Name, ends with char with high bit set



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


// Printing characters on screen
constexpr UINT16 MEM_PRINT_INVERSE	= 0x00B4;			// 00 for regular, 7F for inverse glyph
constexpr UINT16 MEM_PRINT_X_ORIGIN = 0x00AD;			// Starting X when printing a string of glyphs
constexpr UINT16 MEM_PRINT_Y_ORIGIN = 0x00AA;			// Starting Y when printing a string of glyphs
constexpr UINT16 MEM_PRINT_X = 0x00AE;					// Current X when printing a string of glyphs
constexpr UINT16 MEM_PRINT_Y = 0x00AF;					// Current Y when printing a string of glyphs
constexpr UINT16 MEM_PRINT_WIDTH = 0x00AB;				// Area width for printing current string
constexpr UINT16 MEM_PRINT_HEIGHT = 0x00AC;				// Area height for printing current string

constexpr wchar_t ARRAY_DEATHLORD_CHARSET[128]
{
	'.','.','.','.','.','.','.','.','.','.','.','.','.','\n','.','.',
	'.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
	' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
	'0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
	'@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W','X','Y','Z','―',' ',' ',' ',' ',	// 0x5B is wide dash that spans the whole glyph, to draw full lines
	'`','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w','x','y','z','◆',' ',' ','[',']'	// 0x7B is the cursor
};

constexpr wchar_t ARRAY_DEATHLORD_CHARSET_EOR[128]
{	// The array EOR'd with 0xE5, which Deathlord uses to obfuscate strings in RAM
	'e','d','g','f','a','`','c','b','m','l','o','n','i','h','k','j',
	'u','t','w','v','q','p','s','r',' ',' ',']','[','y','x','◆','z',	// 0x1E is the cursor
	'E','D','G','F','A','@','C','B','M','L','O','N','I','H','K','J',
	'U','T','W','V','Q','P','S','R',' ',' ',' ',' ','Y','X','―','Z',	// 0x3E is wide dash­­
	'%','$','\'','&','!',' ','#','"','-',',','/','.',')','(','+','*',
	'5','4','7','6','1','0','3','2','=','<','?','>','9','8',';',':',
	'.','.','.','.','.','.','.','.','\n','.','.','.','.','.','.','.',	// mostly unused
	'.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.'		// unused
};

constexpr int ARRAY_XP_REQUIREMENTS[22]
{	// necessary xp to increase level
	0,							// level 1
	200, 200, 200, 200,			// levels 2-5
	400, 400, 400, 400,			// levels 6-9
	600, 600, 600, 600,			// levels 10-13
	800, 800, 800, 800,			// levels 14-17
	1000, 1000, 1000, 1000,		// levels 19-21
	1200						// levels 22+
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
	STV = 0x02,		// starving
	TOX = 0x04,		// toxified
	ILL = 0x08,		// illness
	PAR = 0x10,		// paralyzed
	STN = 0x20,		// stone
	RIP = 0x40,		// 6 feet under
	RIP2 = 0x80		// 12 feet under
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

// defined in Main.cpp/Game.cpp
extern std::shared_ptr<DeathlordHacks> GetDeathlordHacks();
extern bool g_isInGameMap;
extern bool g_isInGameTransition;