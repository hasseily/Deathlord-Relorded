#pragma once

/// <summary>
/// The DeathlordHacks class includes methods to do special Deathlord things, and a related window.
/// </summary>
/// 

constexpr int MAP_IS_OVERLAND = 0xFC04;		// is set to 1 if on the overland area
constexpr int MAP_ID = 0xFC4E;				// ID of the map
constexpr int MAP_LEVEL = 0xFC04;			// Level "2" is the default ground for towers and dungeons. "1" for overland, "0" for towns
constexpr int MAP_XPOS = 0xFC06;			// X position of the player on a map
constexpr int MAP_YPOS = 0xFC07;			// Y position of the player on a map
constexpr int MAP_OVERLAND_X = 0xFC4B;		// X position of the overland submap
constexpr int MAP_OVERLAND_Y = 0xFC4C;		// Y position of the overland submap

static INT_PTR CALLBACK HacksProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

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