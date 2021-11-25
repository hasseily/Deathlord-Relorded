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
	if (PlayerIsOverland())
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
	if (PlayerIsOverland())
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
	constexpr int tilesTotal = 0x10;
	constexpr int tileHeight = 16;	// number of rows per tile
	constexpr int bytesPerRow = 2;
	constexpr int dotsPerByte = 7;	// 7 dots in each byte, 2 dots per pixel
	constexpr int sqPixelsPerRow = 14;	// There are really 7 pixels, but they're rectangular so we duplicate square pixels
	constexpr UINT32 color_black		= 0xFF000000;	// ABGR
	constexpr UINT32 color_white		= 0xFFFFFFFF;
	constexpr UINT32 color_blue			= 0xFFFFA10D;
	constexpr UINT32 color_orange		= 0xFF005EF2;
	constexpr UINT32 color_green		= 0xFF00CB38;
	constexpr UINT32 color_purple		= 0xFFFF34C7;

#define  SETRGBCOLOR(r,g,b) {r,g,b,0xff}
	static RGBQUAD pPalette[] =
	{
	SETRGBCOLOR(/*HGR_BLACK, */ 0x00,0x00,0x00),
	SETRGBCOLOR(/*HGR_WHITE, */ 0xFF,0xFF,0xFF),
	SETRGBCOLOR(/*BLUE,      */ 0x00,0x8A,0xB5),
	SETRGBCOLOR(/*ORANGE,    */ 0xFF,0x72,0x47),
	SETRGBCOLOR(/*GREEN,     */ 0x6F,0xE6,0x2C),
	SETRGBCOLOR(/*MAGENTA,   */ 0xAA,0x1A,0xD1),
// TV emu
	SETRGBCOLOR(/*HGR_GREY1, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_GREY2, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_YELLOW,*/ 0x9E,0x9E,0x00), // 0xD0,0xB0,0x10 -> 0x9E,0x9E,0x00
	SETRGBCOLOR(/*HGR_AQUA,  */ 0x00,0xCD,0x4A), // 0x20,0xB0,0xB0 -> 0x00,0xCD,0x4A
	SETRGBCOLOR(/*HGR_PURPLE,*/ 0x61,0x61,0xFF), // 0x60,0x50,0xE0 -> 0x61,0x61,0xFF
	SETRGBCOLOR(/*HGR_PINK,  */ 0xFF,0x32,0xB5), // 0xD0,0x40,0xA0 -> 0xFF,0x32,0xB5
	};

	UINT32* transposed = new UINT32[tilesTotal * tileHeight * sqPixelsPerRow];	// ARGB pixels, 7 per 2 bytes of mem
	memset(transposed, 0, tilesTotal * tileHeight * sqPixelsPerRow * sizeof(UINT32));
	int memStart = 0xD000;
	int transStart = 0;
	bool isGroup2 = false;	// Group 1: 0b01: green (odd), 0b10: violet (even).     Group 2: 0b01: orange (odd), 0b10: blue (even)
	bool bitPairColor[2] = { false, false };
	bool isBW = false;

	for (size_t iTile = 0; iTile < tilesTotal; iTile++)
	{
		for (size_t iRow = 0; iRow < tileHeight; iRow++)
		{
			int xoffset = 0; // offset to start of the 2 bytes
			uint8_t* pMain = MemGetMainPtr(memStart);

			// We need all 28 bits because each pixel needs a three bit evaluation
			// We use black around the 2 bytes
			uint8_t byteval1 = 0;
			uint8_t byteval2 = *pMain;
			uint8_t byteval3 = *(pMain + 1);
			uint8_t byteval4 = 0;

			// all 28 bits chained
			DWORD dwordval = (byteval1 & 0x7F) | ((byteval2 & 0x7F) << 7) | ((byteval3 & 0x7F) << 14) | ((byteval4 & 0x7F) << 21);

			// Extraction of 14 color pixels
			UINT32 colors[14];
			int color = 0;
			DWORD dwordval_tmp = dwordval;
			dwordval_tmp = dwordval_tmp >> 7;
			bool offset = (byteval2 & 0x80) ? true : false;
			for (int i = 0; i < 14; i++)
			{
				if (i == 7) offset = (byteval3 & 0x80) ? true : false;
				color = dwordval_tmp & 0x3;
				// Two cases because AppleWin's palette is in a strange order
				if (offset)
					colors[i] = *reinterpret_cast<const UINT32*>(&pPalette[1 + color]);
				else
					colors[i] = *reinterpret_cast<const UINT32*>(&pPalette[6 - color]);
				if (i % 2) dwordval_tmp >>= 2;
			}
			// Black and White
			UINT32 bw[2];
			bw[0] = *reinterpret_cast<const UINT32*>(&pPalette[0]);
			bw[1] = *reinterpret_cast<const UINT32*>(&pPalette[1]);

			DWORD mask = 0x01C0; //  00|000001 1|1000000
			DWORD chck1 = 0x0140; //  00|000001 0|1000000
			DWORD chck2 = 0x0080; //  00|000000 1|0000000

			// HIRES render in RGB works on a pixel-basis (1-bit data in framebuffer)
			// The pixel can be 'color', if it makes a 101 or 010 pattern with the two neighbour bits
			// In all other cases, it's black if 0 and white if 1
			// The value of 'color' is defined on a 2-bits basis

			dwordval_tmp = dwordval;
			for (size_t byteOfPair = 0; byteOfPair < 2; byteOfPair++)
			{
				dwordval = dwordval_tmp;
				if (byteOfPair == 1)
				{
					// Second byte of the 14 pixels block
					dwordval = dwordval >> 7;
					xoffset = 7;
				}

				for (int i = xoffset; i < xoffset + 7; i++)
				{
					if (((dwordval & mask) == chck1) || ((dwordval & mask) == chck2))
					{
						// Color pixel
						transposed[transStart] = colors[i];
						transStart++;
					}
					else
					{
						// B&W pixel
						transposed[transStart] = bw[(dwordval & chck2 ? 1 : 0)];
						transStart++;
					}
					// Next pixel
					dwordval = dwordval >> 1;
				}
			}
			memStart+=2;
		}
	}

	std::fstream fsFile("Deathlord Player Sprites.data", std::ios::out | std::ios::binary);
	fsFile.write((char *)transposed, tilesTotal * tileHeight * sqPixelsPerRow * sizeof(UINT32));
	fsFile.close();
	delete[] transposed;
	return;
	//
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

/*
//	The character set can be extracted wit the following code
//	Each glyph is a sequence of 8 bytes which are the rows
//	Each bit is a pixel, except for the high bit which is always set
//	but then it's unset for displaying.
//  Hence each glyph is exactly 7 pixels wide by 8 pixels high.
//	The charset starts at 0x4B00, and runs for 96 glyphs.
//	This code creates a b/w bitmap of the glyphs, 16 wide by 6 high
//	The bitmap is 128x48 pixels.
//
//	Note: the first glyph is actually at position 0x20, so Deathlord
//	      discards the first 2 top rows

	constexpr int tilesTotal = 0x60;
	constexpr int tilesAcross = 16;
	constexpr int tilesDown = 6;
	constexpr int tileHeight = 8;
	char* transposed = new char[tilesTotal * tileHeight];
	int memStart = 0x4b00;
	int transByteStart = 0;
	for (size_t j = 0; j < tilesDown; j++)
	{
		for (size_t i = 0; i < tilesAcross * tileHeight; i++)
		{
			int tilePos = i / tileHeight;
			int tileLayer = i % tileHeight;
			char b = MemGetMainPtr(memStart)[i] ^ 0x7F;
			// Reverses the bits, because the first bit is the leftmost pixel
			b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
			b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
			b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
			transposed[transByteStart + tileLayer * tilesAcross + tilePos] = b;
		}
		memStart += tilesAcross * tileHeight;
		transByteStart += tilesAcross * tileHeight;
	}
	std::fstream fsFile("Deathlord Charset.data", std::ios::out | std::ios::binary);
	fsFile.write(transposed, PNGBUFFERSIZE);
	fsFile.close();
	delete[] transposed;
*/