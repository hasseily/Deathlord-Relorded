#pragma once

#include "Emulator/Memory.h"
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

constexpr UINT16 PARTY_CLASS_START = 0xFD60;			// Start of the array of party classes
constexpr UINT16 PARTY_RACE_START = 0xFD5A;				// Start of the array of party races
constexpr UINT16 PARTY_WEAP_READY_START = 0xFDEA;		// Start of the array of weapon ready status: ff = fists, 00 = melee, 01 = ranged
constexpr UINT16 PARTY_STATUS_START = 0xFD36;			// Start of the status bitmask: 0x1:???, 0x2:STV, 0x4:TOX, 0x8:DIS, 0x10:PAR, 0x20:STN 0x40:RIP, 0x80:STO
constexpr UINT16 PARTY_CHAR_LEADER = 0xFC19;			// Leader of the party (0-based)
constexpr UINT16 PARTY_CURRENT_CHAR_POS = 0xFC21;		// Char we're getting info on (0-based)

constexpr UINT16 TILEVIEW_CURRENT_START = 0x0300;		// Start of the current tiles in the viewport. Ends at 0x0350. There are 9x9 tiles
constexpr UINT16 TILEVIEW_NEW_START = 0x0351;			// Start of the new tileset. Ends 0x03A1.
constexpr UINT16 TILEVIEW_CURRENT_PLAYERTILE = 0x0329;	// The player is in the center at tile 41
constexpr UINT16 TILEVIEW_NEW_PLAYERTILE = 0x037A;		// The player is in the center at tile 41


// All damage tiles
// NOTE: The snow swamp does no damage
constexpr UINT16 TILE_OVERLAND_CACTUS_OFFSET = 0x03;
constexpr UINT16 TILE_OVERLAND_SWAMP_OFFSET = 0x2C;
constexpr UINT16 TILE_OVERLAND_FIRE_OFFSET = 0x3C;
constexpr UINT16 TILE_DUNGEON_SWAMP_OFFSET = 0x36;
constexpr UINT16 TILE_DUNGEON_FIRE_OFFSET = 0x2D;
constexpr UINT16 TILE_DUNGEON_ACID_OFFSET = 0x2C;
constexpr UINT16 TILE_DUNGEON_MAGIC_OFFSET = 0x38;		// Magic damage tile in dungeon tileset

enum class DeathlordClasses		// in memory at 0xFD60-0xFD65
{
	Fighter = 0,
	Paladin,
	Ranger,
	Barbarian,
	Berzerker,
	Samurai,
	Brute,
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