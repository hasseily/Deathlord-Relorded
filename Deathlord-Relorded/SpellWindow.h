#pragma once

static INT_PTR CALLBACK SpellProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

class SpellWindow
{
public:
	SpellWindow::SpellWindow(HINSTANCE app, HWND hMainWindow);

	void ShowSpellWindow();
	void HideSpellWindow();
	bool IsSpellWindowDisplayed();

	HWND hwndSpell;				// handle to spell window

private:
	bool isDisplayed;
};

// defined in Main.cpp
extern std::shared_ptr<SpellWindow> GetSpellWindow();