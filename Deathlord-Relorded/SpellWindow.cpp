#include "pch.h"
#include "SpellWindow.h"
#include "NonVolatile.h"
#include "resource.h"

static HINSTANCE appInstance = nullptr;
static HWND hwndMain = nullptr;				// handle to main window

extern NonVolatile g_nonVolatile;

INT_PTR CALLBACK SpellProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	std::shared_ptr<SpellWindow>spw = GetSpellWindow();
	switch (message)
	{
	case WM_INITDIALOG:
		break;
	case WM_NCDESTROY:
		break;
	case WM_CLOSE:
	{
		spw->HideSpellWindow();
		return 0;   // don't destroy the window
		break;
	}
	default:
		break;
	}
	return false;
}

SpellWindow::SpellWindow(HINSTANCE app, HWND hMainWindow)
{
	isDisplayed = false;
	hwndMain = hMainWindow;
	hwndSpell = CreateDialog(app,
		MAKEINTRESOURCE(IDD_SPELLSVIEW),
		hMainWindow,
		(DLGPROC)SpellProc);

	if (hwndSpell != nullptr)
	{
		RECT cR;
		GetClientRect(hwndSpell, &cR);
	}
}


void SpellWindow::ShowSpellWindow()
{
	HMENU hm = GetMenu(hwndMain);
	SetForegroundWindow(hwndSpell);
	ShowWindow(hwndSpell, SW_SHOWNORMAL);
	CheckMenuItem(hm, ID_COMPANION_SPELLWINDOW, MF_BYCOMMAND | MF_CHECKED);
	isDisplayed = true;
	g_nonVolatile.showSpells = isDisplayed;
	g_nonVolatile.SaveToDisk();
}

void SpellWindow::HideSpellWindow()
{
	HMENU hm = GetMenu(hwndMain);
	ShowWindow(hwndSpell, SW_HIDE);
	CheckMenuItem(hm, ID_COMPANION_SPELLWINDOW, MF_BYCOMMAND | MF_UNCHECKED);
	isDisplayed = false;
	g_nonVolatile.showSpells = isDisplayed;
	g_nonVolatile.SaveToDisk();
}

bool SpellWindow::IsSpellWindowDisplayed()
{
	return isDisplayed;
}