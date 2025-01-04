//
// Main.cpp
//

#include "pch.h"
#include "Game.h"
#include "resource.h"
#include <WinUser.h>
#include "RemoteControl/GameLink.h"
#include "Keyboard.h"
#include "LogWindow.h"
#include "SpellWindow.h"
#include "Emulator/AppleWin.h"
#include "Emulator/CardManager.h"
#include "Emulator/Disk.h"
#include "Emulator/SoundCore.h"
#include "Emulator/Keyboard.h"
#include "Emulator/RGBMonitor.h"
#include "DeathlordHacks.h"
#include "AutoMap.h"
#include "TilesetCreator.h"
#include "InvOverlay.h"
#include "AppleWinDXVideo.h"
// For WinPixGpuCapture
#include <filesystem>
#include <shlobj.h>
#include <shellapi.h>

using namespace DirectX;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE g_hInstance;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Initial window width and height, so we can't reduce the size further, and
// user can always go back to "original" size
int m_initialWindowWidth = 1920;
int m_initialWindowHeight = 1080;

bool shouldSendKeystrokesToAppleWin = true;

namespace
{
	std::unique_ptr<Game> g_game;
	std::shared_ptr<LogWindow> g_logW;
	std::shared_ptr<SpellWindow> g_spellW;
	std::shared_ptr<DeathlordHacks> g_dlHacks;
}

void ExitGame() noexcept;

void SetSendKeystrokesToAppleWin(bool shouldSend)
{
	shouldSendKeystrokesToAppleWin = shouldSend;
}

static void ExceptionHandler(LPCSTR pError)
{
	// Need to convert char to wchar for MessageBox
	wchar_t wc[4000];
	size_t ctConverted;
	mbstowcs_s(&ctConverted, wc, (const char *)pError, 4000);
	MessageBox(HWND_TOP,
		wc,
		TEXT("Runtime Exception"),
		MB_ICONEXCLAMATION | MB_SETFOREGROUND);
}

static std::wstring GetLatestWinPixGpuCapturerPath()
{
	LPWSTR programFilesPath = nullptr;
	SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

	std::filesystem::path pixInstallationPath = programFilesPath;
	pixInstallationPath /= "Microsoft PIX";

	std::wstring newestVersionFound;

	for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath))
	{
		if (directory_entry.is_directory())
		{
			if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().c_str())
			{
				newestVersionFound = directory_entry.path().filename().c_str();
			}
		}
	}

	if (newestVersionFound.empty())
	{
		// TODO: Error, no PIX installation found
	}

	return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}

std::unique_ptr<Game>* GetGamePtr()
{
	return &g_game;
}

std::shared_ptr<LogWindow> GetLogWindow()
{
	return g_logW;
}


std::shared_ptr<SpellWindow> GetSpellWindow()
{
	return g_spellW;
}

std::shared_ptr<DeathlordHacks> GetDeathlordHacks()
{
	return g_dlHacks;
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_BUTTON_DOWNLOADDOCS)
		{
			ShellExecute(NULL, L"open", L"https://rikkles.itch.io/deathlord-relorded", NULL, NULL, SW_SHOWNORMAL);
		}
		break;
	}
	return (INT_PTR)FALSE;
}

void UpdateMenuBarStatus(HWND hwnd)
{
	
	HMENU topMenu = GetMenu(hwnd);
	HMENU emuMenu = GetSubMenu(topMenu, 1);	// Emulator menu
	HMENU relordedMenu = GetSubMenu(topMenu, 2);	// Relorded menu
	HMENU videoMenu = GetSubMenu(emuMenu, 10);
	HMENU volumeMenu = GetSubMenu(emuMenu, 11);
	HMENU autoMapMenu = GetSubMenu(relordedMenu, 4);
	HMENU logMenu = GetSubMenu(relordedMenu, 6);

	CheckMenuRadioItem(videoMenu, 0, 3, g_nonVolatile.video, MF_BYPOSITION);
	CheckMenuRadioItem(volumeMenu, 0, 4, g_nonVolatile.volumeSpeaker, MF_BYPOSITION);
	// For All and quadrants
	CheckMenuRadioItem(autoMapMenu, 4, 8, (int)g_nonVolatile.mapQuadrant+4, MF_BYPOSITION);
	// For FollowPlayer which is the same as All
	if (g_nonVolatile.mapQuadrant == AutoMapQuadrant::FollowPlayer)
		CheckMenuRadioItem(autoMapMenu, 4, 4, 4, MF_BYPOSITION);

	CheckMenuItem(emuMenu, ID_EMULATOR_GAMELINK,
		MF_BYCOMMAND | (g_nonVolatile.useGameLink ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(videoMenu, ID_VIDEO_SCANLINES,
		MF_BYCOMMAND | (g_nonVolatile.scanlines ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(videoMenu, ID_VIDEO_VIDEO2X,
		MF_BYCOMMAND | (g_nonVolatile.applewinScale == 2.0f ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(autoMapMenu, ID_AUTOMAP_REMOVEFOG,
		MF_BYCOMMAND | (g_nonVolatile.removeFog ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(autoMapMenu, ID_AUTOMAP_SHOWFOOTSTEPS,
		MF_BYCOMMAND | (g_nonVolatile.showFootsteps ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(autoMapMenu, ID_AUTOMAP_SHOWHIDDEN,
		MF_BYCOMMAND | (g_nonVolatile.showHidden ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(relordedMenu, ID_COMPANION_SPELLWINDOW,
		MF_BYCOMMAND | (g_nonVolatile.showSpells ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(logMenu, ID_LOGWINDOW_ALSOLOGCOMBAT,
		MF_BYCOMMAND | (g_nonVolatile.logCombat ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(relordedMenu, ID_RELORDED_ENGLISHNAMES,
		MF_BYCOMMAND | (g_nonVolatile.englishNames ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(relordedMenu, ID_RELORDED_INVENTORY,
		MF_BYCOMMAND | (InvOverlay::GetInstance()->ShouldRenderOverlay() ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(relordedMenu, ID_RELORDED_ORIGINALINTERFACE,
		MF_BYCOMMAND | (AppleWinDXVideo::GetInstance()->IsApple2VideoDisplayed() ? MF_CHECKED : MF_UNCHECKED));

	ApplyNonVolatileConfig();
}

void ToggleFullScreen(HWND hWnd)
{
	static bool s_fullscreen = false;
	if (s_fullscreen)
	{
		SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);

		ShowWindow(hWnd, SW_SHOWNORMAL);
		SetWindowPos(hWnd, HWND_TOP, 0, 0, m_initialWindowWidth, m_initialWindowHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}
	else
	{
		SetWindowLongPtr(hWnd, GWL_STYLE, 0);
		SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

		SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		ShowWindow(hWnd, SW_SHOWMAXIMIZED);
	}

	s_fullscreen = !s_fullscreen;
}

// Entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	HWND hwnd;
	HACCEL haccel;      // handle to accelerator table 
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

#if defined(_DEBUG)
	// Check to see if a copy of WinPixGpuCapturer.dll has already been injected into the application.
	// This may happen if the application is launched through the PIX UI. 
	if (GetModuleHandle(L"WinPixGpuCapturer.dll") == 0)
	{
		LoadLibrary(GetLatestWinPixGpuCapturerPath().c_str());
	}
#endif

	try {
		// Initialize global strings
		LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
		LoadStringW(hInstance, IDC_DeathlordRelorded, szWindowClass, MAX_LOADSTRING);

		if (!XMVerifyCPUSupport())
			return 1;

		Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
		if (FAILED(initialize))
			return 1;

		g_game = std::make_unique<Game>();

		// Register class and create window
		{
			// Register class
			WNDCLASSEXW wcex = {};
			wcex.cbSize = sizeof(WNDCLASSEXW);
			wcex.style = CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = WndProc;
			wcex.hInstance = hInstance;
			wcex.hIcon = LoadIconW(hInstance, L"IDI_ICON");
			wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
			wcex.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
			wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DeathlordRelorded);
			wcex.lpszClassName = L"DeathlordRelordedWindowClass";
			wcex.hIconSm = LoadIconW(wcex.hInstance, L"IDI_ICON");
			if (!RegisterClassExW(&wcex))
				return 1;

			g_hInstance = hInstance; // Store instance handle in our global variable

			// Create window

			RECT rc = { 0, 0, m_initialWindowWidth, m_initialWindowHeight };		// Static game window size

			// Now need to set the size to the actual usable size
			AdjustWindowRect(&rc, WS_OVERLAPPED | WS_SYSMENU, TRUE);
			m_initialWindowWidth = rc.right - rc.left;
			m_initialWindowHeight = rc.bottom - rc.top;

			// TODO: enable resizing the window by using WS_OVERLAPPEDWINDOW
			//       disable resizing the window by using WS_OVERLAPPED | WS_SYSMENU
			//		 make it fullscreen with WS_POPUP
			hwnd = CreateWindowExW(0, L"DeathlordRelordedWindowClass", L"Deathlord Relorded", WS_OVERLAPPED | WS_SYSMENU,
				CW_USEDEFAULT, CW_USEDEFAULT, m_initialWindowWidth, m_initialWindowHeight, nullptr, nullptr, hInstance, nullptr);

			if (!hwnd)
				return 1;

			// Set up the accelerators
			haccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

			SetWindowLongPtr(hwnd, GWL_EXSTYLE, 0);	// set it to WS_EX_TOPMOST for fullscreen
			WINDOWINFO wi;
			wi.cbSize = sizeof(WINDOWINFO);
			GetWindowInfo(hwnd, &wi);

			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(g_game.get()));
			g_game->Initialize(hwnd);

			// create the instances of important blocks at the start
			g_logW = std::make_unique<LogWindow>(g_hInstance, hwnd);
			g_spellW = std::make_unique<SpellWindow>(g_hInstance, hwnd);
			g_dlHacks = std::make_unique<DeathlordHacks>(g_hInstance, hwnd);

			// And open the spells window if necessary
			if (g_nonVolatile.showSpells)
				g_game->MenuShowSpellWindow();
			
			// Game has now loaded the saved/default settings
			// Update the menu bar with the settings
			UpdateMenuBarStatus(hwnd);
			//ShowWindow(hwnd, nCmdShow);
			ToggleFullScreen(hwnd);

		}

		// Main message loop
		MSG msg = {};
		while (WM_QUIT != msg.message)
		{
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				if (!g_dlHacks->IsHacksWindowDisplayed() || !IsDialogMessage(g_dlHacks->hwndHacks, &msg))
				{
					if (haccel && !TranslateAccelerator(
						hwnd,		// handle to receiving window 
						haccel,    // handle to active accelerator table 
						&msg))         // message data 
					{
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
				// UpdateMenuBarStatus(hwnd);
			}
			else
			{
				g_game->Tick();
			}
		}

		g_game.reset();

		return static_cast<int>(msg.wParam);
	}
	catch (std::runtime_error exception)
	{
		ExceptionHandler(exception.what());
		return 1;
	}

}

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static bool s_in_sizemove = false;
	static bool s_in_suspend = false;
	static bool s_minimized = false;
	static bool s_menu_visible = true; 

	auto game = reinterpret_cast<Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	// Disallow saving by default when not in the game map
	// It is allowed temporarily when hitting 'q'
	// 	   TODO: DISABLED. It can corrupt save games
	// g_wantsToSave = !(g_isInGameMap);

	switch (message)
	{
	case WM_PAINT:
		//PAINTSTRUCT ps;
		//(void)BeginPaint(hWnd, &ps);
		// Don't bother filling the window background as it'll be all replaced by Direct3D
		//EndPaint(hWnd, &ps);
		break;

	case WM_MOVE:
		// Gives UPPER LEFT CORNER of client Rect
		if (game)
		{
			game->OnWindowMoved();
		}
		break;

	case WM_SIZE:
		// Gives WIDTH and HEIGHT of client area
		if (wParam == SIZE_MINIMIZED)
		{
			if (!s_minimized)
			{
				s_minimized = true;
				if (!s_in_suspend && game)
					game->OnSuspending();
				s_in_suspend = true;
			}
		}
		else if (s_minimized)
		{
			s_minimized = false;
			if (s_in_suspend && game)
				game->OnResuming();
			s_in_suspend = false;
		}
		else if (game)
		{
			// char buf[500];
			// sprintf_s(buf, "In Main WM_SIZE Width %d, Height %d\n", LOWORD(lParam), HIWORD(lParam));
			// OutputDebugStringA(buf);
			game->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
		}
		break;

	case WM_SIZING:
		// Force the size to have a fixed aspect ratio
	{
		break;
	}

	case WM_ENTERSIZEMOVE:
		MB_Mute();
		Spkr_Mute();
		s_in_sizemove = true;
		break;

	case WM_EXITSIZEMOVE:
		MB_Demute();
		Spkr_Demute();
		s_in_sizemove = false;
		break;

	case WM_ENTERMENULOOP:
		MB_Mute();
		Spkr_Mute();
		break;

	case WM_EXITMENULOOP:
		MB_Demute();
		Spkr_Demute();
		break;

	case WM_GETMINMAXINFO:
		if (lParam)
		{
			// Set the minimum size to be at least the size of the original Apple screen with borders
			auto info = reinterpret_cast<MINMAXINFO*>(lParam);
			info->ptMinTrackSize.x = 700;
			info->ptMinTrackSize.y = 500;
		}
		break;

	case WM_ACTIVATEAPP:
		if (game)
		{
			Keyboard::ProcessMessage(message, wParam, lParam);
			Mouse::ProcessMessage(message, wParam, lParam);

			if (wParam)
			{
				game->OnActivated();
			}
			else
			{
				game->OnDeactivated();
			}
		}
		break;

	case WM_POWERBROADCAST:
		switch (wParam)
		{
		case PBT_APMQUERYSUSPEND:
			if (!s_in_suspend && game)
				game->OnSuspending();
			s_in_suspend = true;
			return TRUE;

		case PBT_APMRESUMESUSPEND:
			if (!s_minimized)
			{
				if (s_in_suspend && game)
					game->OnResuming();
				s_in_suspend = false;
			}
			return TRUE;
		}
		break;

	case WM_MOUSEMOVE:
		if (!s_menu_visible) {
			// Re-activate the menu if the mouse goes to the top of the window
			POINT cursorPos;
			GetCursorPos(&cursorPos);
			ScreenToClient(hWnd, &cursorPos);

			if (cursorPos.y <= 10) { // Near the top of the window
				HMENU hNewMenu = LoadMenu(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_DeathlordRelorded)); // Replace with your menu resource
				SetMenu(hWnd, hNewMenu);
				s_menu_visible = true;
				DrawMenuBar(hWnd); // Redraw menu bar
			}
		}
		[[fallthrough]];
	case WM_INPUT:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
		Mouse::ProcessMessage(message, wParam, lParam);
		break;
	case WM_CHAR:			// Send to the applewin emulator
		if (!shouldSendKeystrokesToAppleWin)
			break;
		if (g_isInGameMap)
		{
			switch (wParam)
			{
			case 'q':
				//__fallthrough;
			case 'Q':
			{
				AutoMap* automap = AutoMap::GetInstance();
				automap->SaveCurrentMapInfo();
				break;
			}
			default:
				break;
			}
		}
		KeybQueueKeypress(wParam, ASCII);
		break;
	case WM_KEYUP:
		Keyboard::ProcessMessage(message, wParam, lParam);
		break;
	case WM_KEYDOWN:
		Keyboard::ProcessMessage(message, wParam, lParam);
		if (shouldSendKeystrokesToAppleWin)		// Send to the applewin emulator
			KeybQueueKeypress(wParam, NOT_ASCII);
		break;

	case WM_SYSKEYUP:
		Keyboard::ProcessMessage(message, wParam, lParam);
		break;
	case WM_SYSKEYDOWN:
		if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
		{
			// Implements the classic ALT+ENTER fullscreen toggle
			ToggleFullScreen(hWnd);
		}
		Keyboard::ProcessMessage(message, wParam, lParam);
		break;

	case WM_MENUCHAR:
		// A menu is active and the user presses a key that does not correspond
		// to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
		return MAKELRESULT(0, MNC_CLOSE);

	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case ID_SCENARIOS_BACKUP:
		{
			g_dlHacks->BackupScenarioImages();
			break;
		}
		case ID_SCENARIOS_RESTORE:
		{
			Disk2InterfaceCard& diskCard = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));

			diskCard.EjectDisk(DRIVE_1);
			diskCard.EjectDisk(DRIVE_2);
			if (g_dlHacks->RestoreScenarioImages())
			{
				MessageBox(hWnd, L"Scenario disks restored from backup. The system will now reboot. Please stand by.", L"Restore completed", MB_ICONINFORMATION | MB_OK);
				PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_EMULATOR_RESET, 1);
			}
			else
			{
				MessageBox(hWnd, L"Scenario disks were not restored. Drives are empty.", L"Restore failed", MB_ICONWARNING | MB_OK);
			}
			break;
		}
		case ID_EMULATOR_SELECTDISKBOOT:
		{
			Disk2InterfaceCard& diskCard = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
			if (diskCard.UserSelectNewDiskImage(DRIVE_1, &g_nonVolatile.diskBootPath, L"Select Deathlord Boot Disk Image"))
				diskCard.SaveLastDiskImage(FLOPPY_BOOT);
			break;
		}
		case ID_EMULATOR_SELECTSCENARIOA:
		{
			Disk2InterfaceCard& diskCard = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
			if (diskCard.UserSelectNewDiskImage(DRIVE_1, &g_nonVolatile.diskScenAPath, L"Select Deathlord Scenario A Disk Image"))
				diskCard.SaveLastDiskImage(FLOPPY_SCENA);
			break;
		}
		case ID_EMULATOR_SELECTSCENARIOB:
		{
			Disk2InterfaceCard& diskCard = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
			if (diskCard.UserSelectNewDiskImage(DRIVE_2, &g_nonVolatile.diskScenBPath, L"Select Deathlord Scenario B Disk Image"))
				diskCard.SaveLastDiskImage(FLOPPY_SCENB);
			break;
		}
		case ID_EMULATOR_INSERTBOOTDISK:
		{
			Disk2InterfaceCard& diskCard = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
			diskCard.EjectDisk(DRIVE_2);
			ImageError_e bRes = diskCard.InsertDisk(DRIVE_1, g_nonVolatile.diskBootPath.c_str(), false, false);
			if (bRes != eIMAGE_ERROR_NONE)
			{
				if (diskCard.UserSelectNewDiskImage(DRIVE_1, &g_nonVolatile.diskBootPath, L"Select Deathlord Boot Disk"))
					diskCard.SaveLastDiskImage(FLOPPY_BOOT);
			}
			break;
		}
		case ID_EMULATOR_INSERTSCENARIODISKS:
		{
			Disk2InterfaceCard& diskCard = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
			ImageError_e res = diskCard.InsertDisk(DRIVE_1, g_nonVolatile.diskScenAPath.c_str(), false, false);
			if (res != eIMAGE_ERROR_NONE)
				MessageBox(hWnd, L"Could not insert Scenario A. Please make sure you've selected a correct image.", L"Alert", MB_ICONASTERISK | MB_OK);
			res = diskCard.InsertDisk(DRIVE_2, g_nonVolatile.diskScenBPath.c_str(), false, false);
			if (res != eIMAGE_ERROR_NONE)
				MessageBox(hWnd, L"Could not insert Scenario B. Please make sure you've selected a correct image.", L"Alert", MB_ICONASTERISK | MB_OK);
			break;
		}
		case ID_EMULATOR_INSERTINTODISK1:
		{
			Disk2InterfaceCard& diskCard = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
			diskCard.UserSelectNewDiskImage(DRIVE_1, &g_nonVolatile.diskBootPath, L"Select Image to Insert into Disk 1");
			break;
		}
		case ID_EMULATOR_INSERTINTODISK2:
		{
			Disk2InterfaceCard& diskCard = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
			diskCard.UserSelectNewDiskImage(DRIVE_2, &g_nonVolatile.diskBootPath, L"Select Image to Insert into Disk 2");
			break;
		}

		case ID_EMULATOR_PAUSE:
		{
			switch (g_nAppMode)
			{
			case AppMode_e::MODE_RUNNING:
				g_nAppMode = AppMode_e::MODE_PAUSED;
				SoundCore_SetFade(FADE_OUT);
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), 1), ID_EMULATOR_PAUSE, MF_BYCOMMAND | MF_CHECKED);
				break;
			case AppMode_e::MODE_PAUSED:
				g_nAppMode = AppMode_e::MODE_RUNNING;
				SoundCore_SetFade(FADE_IN);
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), 1), ID_EMULATOR_PAUSE, MF_BYCOMMAND | MF_UNCHECKED);
				break;
			}
			break;
		}
		case ID_EMULATOR_RESET:
		{
			switch (g_nAppMode)
			{
				// TODO: There is absolutely no need for a AppMode_e::MODE_LOGO. When the game is launched, if the HDV can't be found then
				//			open the file dialog to select an hdv
			case AppMode_e::MODE_LOGO:
				g_nAppMode = AppMode_e::MODE_RUNNING;
				break;
			case AppMode_e::MODE_RUNNING:
				[[fallthrough]];
			case AppMode_e::MODE_PAUSED:
				if (lParam == 0)
				{
					Spkr_Mute();
					MB_Mute();
					if (MessageBox(HWND_TOP, TEXT("Deathlord hates you.\nAre you sure you want to reboot?"), TEXT("Reboot Deathlord"), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_SYSTEMMODAL) == IDYES)
					{
						// Uncheck the pause in all cases
						CheckMenuItem(GetSubMenu(GetMenu(hWnd), 1), ID_EMULATOR_PAUSE, MF_BYCOMMAND | MF_UNCHECKED);
						PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_EMULATOR_INSERTBOOTDISK, 1);
						EmulatorReboot();
						Spkr_Demute();
						MB_Demute();
					}
					else
					{
						Spkr_Demute();
						MB_Demute();
					}
				}
				else	// gamelink requests will pass 1 here, so no message box
				{
					// Uncheck the pause
					CheckMenuItem(GetSubMenu(GetMenu(hWnd), 1), ID_EMULATOR_PAUSE, MF_BYCOMMAND | MF_UNCHECKED);
					PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_EMULATOR_INSERTBOOTDISK, 1);
					EmulatorReboot();
					Spkr_Demute();
					MB_Demute();
				}
				break;
			}
			VideoRedrawScreen();
			break;
		}
		case ID_VIDEO_IDEALIZED:
			g_nonVolatile.video = 0;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VIDEO_MONITOR:
			g_nonVolatile.video = 1;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VIDEO_COMPOSITEMONITOR:
			g_nonVolatile.video = 2;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VIDEO_TVSCREEN:
			g_nonVolatile.video = 3;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VIDEO_INCREMENT:
			g_nonVolatile.video = (g_nonVolatile.video + 1) % 4;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VOLUME_OFF:
			g_nonVolatile.volumeSpeaker = 0;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VOLUME_25:
			g_nonVolatile.volumeSpeaker = 1;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VOLUME_50:
			g_nonVolatile.volumeSpeaker = 2;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VOLUME_75:
			g_nonVolatile.volumeSpeaker = 3;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VOLUME_100:
			g_nonVolatile.volumeSpeaker = 4;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_VIDEO_SCANLINES:
			g_nonVolatile.scanlines = !g_nonVolatile.scanlines;
			g_nonVolatile.SaveToDisk();
			break;		
		case ID_VIDEO_VIDEO2X:
			g_nonVolatile.applewinScale = (g_nonVolatile.applewinScale == 1.0f ? 2.0f : 1.0f);
			g_nonVolatile.SaveToDisk();
			break;
		case ID_EMULATOR_GAMELINK:
			g_nonVolatile.useGameLink = !g_nonVolatile.useGameLink;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_AUTOMAP_REMOVEFOG:
			g_nonVolatile.removeFog = !g_nonVolatile.removeFog;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_AUTOMAP_SHOWFOOTSTEPS:
			g_nonVolatile.showFootsteps = !g_nonVolatile.showFootsteps;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_AUTOMAP_SHOWHIDDEN:
			g_nonVolatile.showHidden = !g_nonVolatile.showHidden;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_AUTOMAP_DISPLAYFULL:
			if (g_nonVolatile.mapQuadrant == AutoMapQuadrant::All)
				g_nonVolatile.mapQuadrant = AutoMapQuadrant::FollowPlayer;
			else if (g_nonVolatile.mapQuadrant == AutoMapQuadrant::FollowPlayer)
				g_nonVolatile.mapQuadrant = AutoMapQuadrant::All;
			else
				g_nonVolatile.mapQuadrant = AutoMapQuadrant::All;
			break;
		case ID_AUTOMAP_DISPLAYTOPLEFTQUADRANT:
			g_nonVolatile.mapQuadrant = AutoMapQuadrant::TopLeft;
			break;
		case ID_AUTOMAP_DISPLAYTOPRIGHTQUADRANT:
			g_nonVolatile.mapQuadrant = AutoMapQuadrant::TopRight;
			break;
		case ID_AUTOMAP_DISPLAYBOTTOMLEFTQUADRANT:
			g_nonVolatile.mapQuadrant = AutoMapQuadrant::BottomLeft;
			break;
		case ID_AUTOMAP_DISPLAYBOTTOMRIGHTQUADRANT:
			g_nonVolatile.mapQuadrant = AutoMapQuadrant::BottomRight;
			break;
		case ID_AUTOMAP_ERASE:
			if (MessageBox(HWND_TOP, TEXT("Are you sure you want to erase all knowledge of this map?\nFootsteps and seen tiles will be forgotten.\n"), TEXT("Erase Map Knowledge"), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_SYSTEMMODAL) == IDYES)
			{
				auto _aM = AutoMap::GetInstance();
				_aM->ClearMapArea();
			}
			break;
		case ID_COMPANION_SPELLWINDOW:
		{
			if (game)
			{
				game->MenuToggleSpellWindow();
			}
			break;
		}
		case ID_LOGWINDOW_ALSOLOGCOMBAT:
			g_nonVolatile.logCombat = !g_nonVolatile.logCombat;
			g_nonVolatile.SaveToDisk();
			break;
		case ID_LOGWINDOW_SHOW:
		{
			if (game)
			{
				game->MenuToggleLogWindow();
			}
			break;
		}
		case ID_LOGWINDOW_LOAD:
		{
			g_logW->LoadFromFile();
			if (game)
			{
				game->MenuShowLogWindow();
			}
			break;
		}
		case ID_LOGWINDOW_SAVE:
		{
			g_logW->SaveToFile();
			break;
		}
		case ID_COMPANION_HACKS:
			game->MenuToggleHacksWindow();
			break;
		case ID_RELORDED_INVENTORY:
		{
			InvOverlay::GetInstance()->ToggleOverlay();
			break;
		}
		case ID_RELORDED_ORIGINALINTERFACE:
		{
			AppleWinDXVideo::GetInstance()->ToggleApple2Video();
			break;
		}
		case ID_RELORDED_ENGLISHNAMES:
		{
			g_nonVolatile.englishNames = !g_nonVolatile.englishNames;
			g_nonVolatile.SaveToDisk();
			break;
		}
		case ID_TOGGLE_TOP_MENU:
		{
			HMENU hMenu = GetMenu(hWnd);
			if (hMenu && s_menu_visible) {
				// Hide the menu
				SetMenu(hWnd, nullptr);
				s_menu_visible = false;
			}
			else {
				// Restore the menu
				HMENU hNewMenu = LoadMenu(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_DeathlordRelorded)); // Replace with your menu resource
				SetMenu(hWnd, hNewMenu);
				s_menu_visible = true;
			}
			DrawMenuBar(hWnd); // Redraw menu bar
			break;
		}
		case IDM_ABOUT:
			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			ExitGame();
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		UpdateMenuBarStatus(hWnd);
	}
	break;

	case WM_CLOSE:
	case WM_DESTROY:
		ExitGame();
		return (LRESULT)0;
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);

		break;

	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

// Exit helper
void ExitGame() noexcept
{
	// Display exit confirmation
	Spkr_Mute();
	MB_Mute();
	if (MessageBox(HWND_TOP, TEXT("Are you sure you want to quit?\nSave your game first!\n"), TEXT("Quit Deathlord"), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_SYSTEMMODAL) == IDYES)
	{
		GameLink::Term();
		PostQuitMessage(0);
	}
	else
	{
		// Only demute when the player doesn't quit, otherwise there's a lingering sound before the app quits
		Spkr_Demute();
		MB_Demute();
	}
}
