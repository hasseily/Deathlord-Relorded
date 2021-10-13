#pragma once

/// <summary>
/// The DeathlordHacks class includes methods to do special Deathlord things, and a related window.
/// </summary>

static INT_PTR CALLBACK HacksProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

class DeathlordHacks
{

public:
	DeathlordHacks(HINSTANCE app, HWND hMainWindow);

	void SaveMapDataToDisk();
	void BackupScenarioImages();
	void RestoreScenarioImages();
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