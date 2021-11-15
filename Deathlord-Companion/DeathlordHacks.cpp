#include "pch.h"
#include <shobjidl_core.h> 
#include "DeathlordHacks.h"
#include "Emulator/Memory.h"
#include "HAUtils.h"
#include "resource.h"
#include <string>
#include <fstream>
#include <filesystem>
#include "Emulator/AppleWin.h"
#include "Game.h"
#include "TilesetCreator.h"
#include "AutoMap.h"

const wchar_t CLASS_NAME[] = L"Deathlord Hacks Class";

static HINSTANCE appInstance = nullptr;
static HWND hwndMain = nullptr;				// handle to main window


#pragma region Static Methods

INT_PTR CALLBACK HacksProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	std::shared_ptr<DeathlordHacks>hw = GetDeathlordHacks();
	switch (message)
	{
	case WM_INITDIALOG:
	{
		return TRUE;
	}
	case WM_ACTIVATE:
	{
		HWND hdlBExportMap = GetDlgItem(hwndDlg, IDC_BUTTON_EXPORT_MAP);
		EnableWindow(hdlBExportMap, g_isInGameMap);
		break;
	}
	case WM_NCDESTROY:
		break;
	case WM_CLOSE:
	{
		hw->HideHacksWindow();
		return false;   // don't destroy the window
	}
	case WM_COMMAND:
	{
		UINT8* memPtr = MemGetMainPtr(0);
		switch (LOWORD(wParam))
		{
		case IDC_EDIT_MEMLOC:
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

				/*
				std::wstring wbuf = L"Mem: ";
				wchar_t xx[5];
				wsprintf(xx, L"%4X", iMemLoc);
				wbuf.append(xx);
				OutputDebugString(wbuf.c_str());
				*/
			}
			break;
		}
		case IDC_BUTTON_MEMSET:
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
			break;
		}
		case IDC_BUTTON_RESURRECT_ALL:
		{
			for (size_t i = 0xFD36; i < 0xFD3C; i++)
			{
				// set state to healthy
				memPtr[i] = 0;
				// set HP to maxHP
				memPtr[i + (0xFD4E - 0xFD36)] = memPtr[i + (0xFD42 - 0xFD36)];	// HP low
				memPtr[i + (0xFD54 - 0xFD36)] = memPtr[i + (0xFD48 - 0xFD36)];	// HP high
				// set SP to maxSP
				memPtr[i + (0xFD96 - 0xFD36)] = memPtr[i + (0xFD9C - 0xFD36)];
			}
			break;
		}
		case IDC_BUTTON_EXPORT_MAP:
		{
			hw->SaveMapDataToDisk();
			break;
		}
		default:
			break;
		}
		break;
	}
	default:
		break;
	};
	return false;
}

bool PartyLeaderIsOfClass(DeathlordClasses aClass)
{
	return (MemGetMainPtr(PARTY_CLASS_START + MemGetMainPtr(PARTY_CHAR_LEADER)[0])[0] == (UINT16)aClass);
}

bool PartyLeaderIsOfClass(DeathlordClasses aClass1, DeathlordClasses aClass2)
{
	UINT8 classLeader = MemGetMainPtr(PARTY_CLASS_START + MemGetMainPtr(PARTY_CHAR_LEADER)[0])[0];
	if (classLeader == (UINT16)aClass1)
		return true;
	if (classLeader == (UINT16)aClass2)
		return true;
	return false;
}

bool PartyLeaderIsOfRace(DeathlordRaces aRace)
{
	return (MemGetMainPtr(PARTY_RACE_START + MemGetMainPtr(PARTY_CHAR_LEADER)[0])[0] == (UINT16)aRace);
}

bool PartyHasClass(DeathlordClasses aClass)
{
	for (size_t i = 0; i < 6; i++)
	{
		if (MemGetMainPtr(PARTY_CLASS_START + i)[0] == (UINT16)aClass)
			return true;
	}
	return false;
}

bool PartyHasClass(DeathlordClasses aClass1, DeathlordClasses aClass2)
{
	for (size_t i = 0; i < 6; i++)
	{
		if (MemGetMainPtr(PARTY_CLASS_START + i)[0] == (UINT16)aClass1)
			return true;
		if (MemGetMainPtr(PARTY_CLASS_START + i)[0] == (UINT16)aClass2)
			return true;
	}
	return false;
}

bool PartyHasRace(DeathlordRaces aRace)
{
	for (size_t i = 0; i < 6; i++)
	{
		if (MemGetMainPtr(PARTY_RACE_START + i)[0] == (UINT16)aRace)
			return true;
	}
	return false;
}

#pragma endregion

DeathlordHacks::DeathlordHacks(HINSTANCE app, HWND hMainWindow)
{
	isDisplayed = false;
	hwndMain = hMainWindow;
	hwndHacks = CreateDialog(app,
		MAKEINTRESOURCE(IDD_HACKS),
		hMainWindow,
		(DLGPROC)HacksProc);

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

void DeathlordHacks::SaveMapDataToDisk()
{
	char nameSuffix[100];
	std::string fileName;
	std::fstream fsFile;

	TCHAR szAppPath[3000];
	GetModuleFileNameW(NULL, szAppPath, 3000);
	std::filesystem::path wsAppPath(szAppPath);
	std::filesystem::path mapsDir = std::filesystem::path(wsAppPath);
	mapsDir.replace_filename("Maps");
	std::filesystem::create_directory(mapsDir);

	// Do the map file directly from memory
	std::string sMapData = "";
	
	auto tileset = TilesetCreator::GetInstance();
	auto automap = AutoMap::GetInstance();
	UINT8 *memPtr = automap->GetCurrentGameMap();
	char tileId[3] = "00";
	for (size_t i = 0; i < MAP_LENGTH; i++)
	{
		sprintf_s(tileId, 3, "%02X", memPtr[i]);
		if (i != 0)
		{
			if ((i % MAP_WIDTH) == 0)
				sMapData.append("\n");
			else
				sMapData.append(" ");
		}
		sMapData.append(tileId, 2);
	}
	// Save the map file
	memPtr = MemGetMainPtr(0);
	fileName = "Maps\\Map ";
	if (memPtr[MAP_IS_OVERLAND] == 0x80)
		sprintf_s(nameSuffix, 100, "Overland %02X %02X.txt", memPtr[MAP_OVERLAND_X], memPtr[MAP_OVERLAND_Y]);
	else
		sprintf_s(nameSuffix, 100, "%02X-%02X.txt", memPtr[MAP_ID], memPtr[MAP_FLOOR]);
	fileName.append(nameSuffix);
	std::filesystem::path mapPath = std::filesystem::path(wsAppPath);
	mapPath.replace_filename(fileName);
	fsFile = std::fstream(mapPath, std::ios::out | std::ios::binary);
	fsFile.write(sMapData.c_str(), sMapData.size());
	fsFile.close();

	// Do the tileset
	LPBYTE tilesetRGBAData = tileset->GetCurrentTilesetBuffer();
	// Save the tile file
	fileName = "Maps\\Tileset ";
	if (memPtr[MAP_IS_OVERLAND] == 0x80)
		sprintf_s(nameSuffix, 100, "Overland %02X %02X.data", memPtr[MAP_OVERLAND_X], memPtr[MAP_OVERLAND_Y]);
	else
		sprintf_s(nameSuffix, 100, "%02X-%02X.data", memPtr[MAP_ID], memPtr[MAP_FLOOR]);
	fileName.append(nameSuffix);
	std::filesystem::path tilesetPath = std::filesystem::path(wsAppPath);
	tilesetPath.replace_filename(fileName);
	fsFile = std::fstream(tilesetPath, std::ios::out | std::ios::binary);
	fsFile.write((char *)tilesetRGBAData, PNGBUFFERSIZE);
	fsFile.close();

	std::wstring msg(L"Saved level map: ");
	msg.append(mapPath.c_str());
	msg.append(L"\nSaved tileset  : ");
	msg.append(tilesetPath.c_str());
	msg.append(L"\n\nTileset format data is RGBA 448x512 pixels");
	MessageBox(g_hFrameWindow, msg.c_str(), TEXT("Deathlord Map Data"), MB_OK);
}

void DeathlordHacks::BackupScenarioImages()
{
	std::wstring imageName;
	std::wstring imageExtension;
	std::wstring imageDirPath;
	std::wstring backupsDirPath;
	std::wstring backupImageFullPath;
	std::wstring images[2] = { g_nonVolatile.diskScenAPath, g_nonVolatile.diskScenBPath };
	SYSTEMTIME st;
	wchar_t formattedTime[50] = L"";
	bool inExtension = true;
	bool inName = false;
	bool res;
	DWORD err;

	GetSystemTime(&st);
	for each (std::wstring pathname in images)
	{
		imageName = L"";
		imageExtension = L"";
		imageDirPath = L"";
		backupsDirPath = L"";
		backupImageFullPath = L"";
		inExtension = true;
		inName = false;
		for (std::wstring::reverse_iterator rit = pathname.rbegin(); rit != pathname.rend(); ++rit)
		{
			if (inExtension)
			{
				if (*rit == '.')
				{
					inExtension = false;
					inName = true;
				}
				else
				{
					imageExtension.insert(0, std::wstring(1, *rit));
				}
			}
			else if (inName)
			{
				if (*rit == '\\')
				{
					inName = false;
				}
				else
				{
					imageName.insert(0, std::wstring(1, *rit));
				}
			}
			else
			{
				imageDirPath.insert(0, std::wstring(1, *rit));
			}
		}
		backupsDirPath = imageDirPath + L"\\Backups";
		res = CreateDirectory(backupsDirPath.c_str(), NULL);
		if (res == 0)
		{
			err = GetLastError();
			if (err == ERROR_PATH_NOT_FOUND)
			{
				HA::AlertIfError(g_hFrameWindow);
				return;
			}
		}
		backupImageFullPath = backupsDirPath + L"\\" + imageName;
		wsprintf(formattedTime, L"_%04d%02d%02d-%02d%02d%02d.%s", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, imageExtension.c_str());
		backupImageFullPath.append(formattedTime);
		res = CopyFile(pathname.c_str(), backupImageFullPath.c_str(), false);
		if (res == 0)
		{
			err = GetLastError();
			HA::AlertIfError(g_hFrameWindow);
			return;
		}
	}
	MessageBox(g_hFrameWindow, std::wstring(L"Successfully backed up scenario disks with postfix ").append(formattedTime).c_str(), L"Backup Successful", MB_ICONINFORMATION | MB_OK);
}

bool DeathlordHacks::RestoreScenarioImages()
{
	std::wstring imageName;
	std::wstring imageDirPath;
	std::wstring backupsDirPath;
	std::wstring backupImageFullPath;
	std::wstring images[2] = { g_nonVolatile.diskScenAPath, g_nonVolatile.diskScenBPath };
	bool res;
	DWORD err;

	HRESULT hr = CoInitializeEx(nullptr, COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		IFileOpenDialog* pFileOpen;

		// Create the FileOpenDialog object.
		hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
			IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr))
		{
			pFileOpen->SetTitle(L"Choose Scenario A backup image");
			COMDLG_FILTERSPEC rgSpec[] =
			{
				{ L"All Images", L"*.bin;*.do;*.dsk;*.nib;*.po;*.gz;*.woz;*.zip;*.2mg;*.2img;*.iie;*.apl" },
				{ L"Disk Images (*.bin,*.do,*.dsk,*.nib,*.po,*.gz,*.woz,*.zip,*.2mg,*.2img,*.iie)", L"*.bin;*.do;*.dsk;*.nib;*.po;*.gz;*.woz;*.zip;*.2mg;*.2img;*.iie" },
				{ L"All Files", L"*.*" },
			};
			pFileOpen->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);
			// Show the Open dialog box.
			hr = pFileOpen->Show(nullptr);
			// Get the file name from the dialog box.
			if (SUCCEEDED(hr))
			{
				IShellItem* pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					// TODO: Grab the file and its extension. Then also grab the scenario B or exit with error
					// Once we know both are good, remove existing scenario disks from drives
					// and copy the backups over the existing files, overwriting them (using their filenames)
					PWSTR pszFilePathA;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePathA);
					if (SUCCEEDED(hr))
					{
						std::filesystem::directory_entry dir = std::filesystem::directory_entry(pszFilePathA);
						if (!dir.is_regular_file())
						{
							MessageBox(g_hFrameWindow, L"This is not a regular file!", L"Alert", MB_ICONASTERISK | MB_OK);
							return false;
						}
						std::wstring pathname(pszFilePathA);
						std::wstring sA(L" Scenario A_2");	// piece of "Deathlord Scenario A_2021...
						size_t fx = pathname.find(sA);
						if (fx == std::string::npos)
						{
							MessageBox(g_hFrameWindow, L"The program cannot parse the backup filename. Please leave the image filenames alone, thank you :)", L"Alert", MB_ICONASTERISK | MB_OK);
							return false;
						}
						pathname.replace(fx, sA.length(), L" Scenario B_2");
						// Now copy both files
						if (!CopyFile(pszFilePathA, g_nonVolatile.diskScenAPath.c_str(), FALSE))
						{
							err = GetLastError();
							HA::AlertIfError(g_hFrameWindow);
							return false;
						}
						if (!CopyFile(pathname.c_str(), g_nonVolatile.diskScenBPath.c_str(), FALSE))
						{
							err = GetLastError();
							HA::AlertIfError(g_hFrameWindow);
							return false;
						}
					}
					pItem->Release();
				}
				else
				{
					err = GetLastError();
					HA::AlertIfError(g_hFrameWindow);
					return false;
				}
			}
			else
			{
				err = GetLastError();
				HA::AlertIfError(g_hFrameWindow);
				return false;
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}
	else
	{
		return false;
	}
	return true;
}