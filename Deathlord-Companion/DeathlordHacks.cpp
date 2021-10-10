#include "pch.h"
#include <shobjidl_core.h> 
#include "DeathlordHacks.h"
#include "Emulator/Memory.h"
#include "HAUtils.h"
#include "resource.h"
#include <string>
#include <fstream>

const wchar_t CLASS_NAME[] = L"Deathlord Hacks Class";

constexpr int MAP_IS_OVERLAND = 0xFC04;		// is set to 1 if on the overland area
constexpr int MAP_ID = 0xFC4E;				// ID of the map
constexpr int MAP_OVERLAND_X = 0xFC4B;		// X position of the overland submap
constexpr int MAP_OVERLAND_Y = 0xFC4C;		// Y position of the overland submap
constexpr int MAP_START_MEM = 0xC00;		// Start of memory area of the in-game map
constexpr UINT8 MAP_WIDTH = 64;
constexpr int MAP_LENGTH = MAP_WIDTH * MAP_WIDTH;			// Size of map (in bytes)

static HINSTANCE appInstance = nullptr;
static HWND hwndMain = nullptr;				// handle to main window

INT_PTR CALLBACK HacksProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_NCDESTROY:
		break;
	case WM_CLOSE:
	{
		std::shared_ptr<DeathlordHacks>hw = GetDeathlordHacks();
		hw->HideHacksWindow();
		return false;   // don't destroy the window
	}
	case WM_COMMAND:
	{
		UINT8* memPtr = MemGetMainPtr(0);
		if (LOWORD(wParam) == IDC_EDIT_MEMLOC)
		{
			HWND hdlMemLoc = GetDlgItem(hwndDlg, IDC_EDIT_MEMLOC);
			HWND hdlMemVal = GetDlgItem(hwndDlg, IDC_MEMCURRENTVAL);
			if (HIWORD(wParam) == EN_CHANGE)
			{
				wchar_t cMemLoc[5] = L"0000";
				GetWindowTextW(hdlMemLoc, cMemLoc, 5);
				int iMemLoc = 0;
				try {
					iMemLoc = std::stoi(cMemLoc, nullptr, 16);
				}
				catch (std::invalid_argument& e) {
					// std::cout << e.what();
					SetWindowText(hdlMemVal, L"");
					break;
				}
				wchar_t cMemVal[3] = L"00";
				_itow_s(memPtr[iMemLoc], cMemVal, 16);
				SetWindowText(hdlMemVal, cMemVal);

				std::wstring wbuf = L"Mem: ";
				wchar_t xx[5];
				wsprintf(xx, L"%4X", iMemLoc);
				wbuf.append(xx);
				OutputDebugString(wbuf.c_str());
			}
		} else if (LOWORD(wParam) == IDC_BUTTON_MEMSET)
		{
			HWND hdlMemLoc = GetDlgItem(hwndDlg, IDC_EDIT_MEMLOC);
			HWND hdlMemValNew = GetDlgItem(hwndDlg, IDC_EDIT_MEMVAL);
			if (HIWORD(wParam) == BN_CLICKED)
			{
				// Get the memory byte to change
				wchar_t cMemLoc[5] = L"0000";
				GetWindowTextW(hdlMemLoc, cMemLoc, 5);
				int iMemLoc = 0;
				try {
					iMemLoc = std::stoi(cMemLoc, nullptr, 16);
				}
				catch (std::invalid_argument& e) {
					// std::cout << e.what();
					SetWindowText(hdlMemValNew, L"");
					break;
				}

				// Get what value we want it to change to
				wchar_t cNewVal[3] = L"00";
				GetWindowTextW(hdlMemValNew, cNewVal, 3);
				int iNewVal = 0;
				try {
					iNewVal = std::stoi(cNewVal, nullptr, 16);
					_itow_s(iNewVal, cNewVal, 16);
					SetWindowText(hdlMemValNew, cNewVal);
				}
				catch (std::invalid_argument& e) {
					// std::cout << e.what();
					SetWindowText(hdlMemValNew, L"");
					break;
				}

				// affect the change
				memPtr[iMemLoc] = iNewVal;
				// and trigger an update to IDC_MEMCURRENTVAL
				SetWindowText(hdlMemLoc, cMemLoc);
			}
		}
		break;
	}
	default:
		break;
	};
	return false;
}

DeathlordHacks::DeathlordHacks(HINSTANCE app, HWND hMainWindow)
{
	isDisplayed = false;
	hwndMain = hMainWindow;
	hwndHacks = CreateDialog(app,
		MAKEINTRESOURCE(IDD_HACKS),
		hMainWindow,
		(DLGPROC)HacksProc);
	/*
	WNDCLASS wc = { };

	wc.lpfnWndProc = HacksProc;
	wc.hInstance = app;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);


	hwndHacks = CreateWindowExW(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		L"Hacks",						// Window text
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,           // Window style

		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, 400, 600,

		hMainWindow,// Parent window    
		nullptr,       // Menu
		app,        // Instance handle
		nullptr        // Additional application data
	);
	*/

	if (hwndHacks != nullptr)
	{
		RECT cR;
		GetClientRect(hwndHacks, &cR);
		HWND hdlMemLoc = GetDlgItem(hwndHacks, IDC_EDIT_MEMLOC);
		SendMessage(hdlMemLoc, EM_SETLIMITTEXT, 4, 0);	// set the limit to a max of ffff
		HWND hdlNewVal = GetDlgItem(hwndHacks, IDC_EDIT_MEMVAL);
		SendMessage(hdlNewVal, EM_SETLIMITTEXT, 2, 0);	// set the limit to a max of ff
	}
}
void DeathlordHacks::ShowHacksWindow()
{
	HMENU hm = GetMenu(hwndMain);
	SetForegroundWindow(hwndHacks);
	ShowWindow(hwndHacks, SW_SHOWNORMAL);
	CheckMenuItem(hm, ID_COMPANION_HACKS, MF_BYCOMMAND | MF_CHECKED);
	isDisplayed = true;
}

void DeathlordHacks::HideHacksWindow()
{
	HMENU hm = GetMenu(hwndMain);
	ShowWindow(hwndHacks, SW_HIDE);
	CheckMenuItem(hm, ID_COMPANION_HACKS, MF_BYCOMMAND | MF_UNCHECKED);
	isDisplayed = false;
}

bool DeathlordHacks::IsHacksWindowDisplayed()
{
	return isDisplayed;
}

void DeathlordHacks::saveMapDataToDisk()
{
	std::string sMapData = "";
	UINT8 *memPtr = MemGetBankPtr(0);
	char tileId[3] = "00";
	for (size_t i = 0; i < MAP_LENGTH; i++)
	{
		sprintf_s(tileId, 3, "%02X", memPtr[MAP_START_MEM+i]);
		if (i != 0)
		{
			if ((i % MAP_WIDTH) == 0)
				sMapData.append("\n");
			else
				sMapData.append(" ");
		}
		sMapData.append(tileId, 2);
	}
	
	// std::wstring wbuf;
	// HA::ConvertStrToWStr(&sMapData, &wbuf);
	// OutputDebugString(wbuf.c_str());
	std::string fileName = "Deathlord Map ";
	char nameSuffix[100];
	if (memPtr[MAP_IS_OVERLAND] == 1)
		sprintf_s(nameSuffix, 100, "%02X Overland %02X %02X.txt", memPtr[MAP_ID], memPtr[MAP_OVERLAND_X], memPtr[MAP_OVERLAND_Y]);
	else
		sprintf_s(nameSuffix, 100, "%02X.txt", memPtr[MAP_ID]);
	fileName.append(nameSuffix);
	std::fstream fsFile(fileName, std::ios::out | std::ios::binary);
	fsFile.write(sMapData.c_str(), sMapData.size());
	fsFile.close();

}
