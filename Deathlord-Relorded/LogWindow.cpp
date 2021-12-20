#include "pch.h"
#include <shobjidl_core.h> 
#include "LogWindow.h"
#include "resource.h"
#define LF_FACESIZE 32      // Missing define for Richedit.h
#include <Richedit.h>
#include <filesystem>

static HINSTANCE appInstance = nullptr;
static HWND hwndMain = nullptr;				// handle to main window

HWND hwndEdit = nullptr;			// handle to rich edit window
std::wstring currentBuffer;			// buffer that is used until there's a newline and then it prints and empties

std::wstring m_prevLogString;

// Register the window class.
const wchar_t CLASS_NAME[] = L"Window Log Class";

/*
///////////////// Utility methods ////////////////////////
*/

/// <summary>
/// Create a rich edit control
/// </summary>
HWND CreateRichEdit(HWND hwndOwner,        // Dialog box handle.
    int x, int y,          // Location.
    int width, int height, // Dimensions.
    HINSTANCE hinst)       // Application or DLL instance.
{
    LoadLibrary(TEXT("Msftedit.dll"));

    HWND _hwndEdit = CreateWindowEx(0, MSFTEDIT_CLASS, TEXT(""),
        ES_MULTILINE | ES_NOHIDESEL | ES_AUTOVSCROLL | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | WS_VSCROLL,
        x, y, width, height,
        hwndOwner, nullptr, hinst, nullptr);

	CHARFORMAT cf = {};
	cf.cbSize = sizeof(CHARFORMAT);
	SendMessage(_hwndEdit, EM_GETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
	wchar_t wcface[LF_FACESIZE] = L"Courier New\0";
	memcpy_s(cf.szFaceName, LF_FACESIZE, wcface, LF_FACESIZE);
	SendMessage(_hwndEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    return _hwndEdit;
}

// A helper function that converts UNICODE data to ANSI and writes it to the given file.
// We write in ANSI format to make it easier to open the output file in Notepad.
// https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/winui/shell/appplatform/commonfiledialog/CommonFileDialogApp.cpp

HRESULT _WriteDataToFile(HANDLE hFile, PCWSTR pszDataIn)
{
	// First figure out our required buffer size.
	DWORD cbData = WideCharToMultiByte(CP_ACP, 0, pszDataIn, -1, NULL, 0, NULL, NULL);
	HRESULT hr = (cbData == 0) ? HRESULT_FROM_WIN32(GetLastError()) : S_OK;
	if (SUCCEEDED(hr))
	{
		// Now allocate a buffer of the required size, and call WideCharToMultiByte again to do the actual conversion.
		char* pszData = new (std::nothrow) CHAR[cbData];
		hr = pszData ? S_OK : E_OUTOFMEMORY;
		if (SUCCEEDED(hr))
		{
			hr = WideCharToMultiByte(CP_ACP, 0, pszDataIn, -1, pszData, cbData, NULL, NULL)
				? S_OK
				: HRESULT_FROM_WIN32(GetLastError());
			if (SUCCEEDED(hr))
			{
				DWORD dwBytesWritten = 0;
				hr = WriteFile(hFile, pszData, cbData - 1, &dwBytesWritten, NULL)
					? S_OK
					: HRESULT_FROM_WIN32(GetLastError());
			}
			delete[] pszData;
		}
	}
	return hr;
}

INT_PTR CALLBACK LogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    switch (message)
    {
    case WM_INITDIALOG:
        break;
    case WM_NCDESTROY:
        break;
    case WM_CLOSE:
    {
        std::shared_ptr<LogWindow>lw = GetLogWindow();
        lw->HideLogWindow();
        return 0;   // don't destroy the window
        break;
    }
    case WM_SIZING:
        // when resizing the log window, resize the edit control as well
        RECT rc;
        GetClientRect(hwndDlg, &rc);
        SetWindowPos(hwndEdit, HWND_TOP, rc.left, rc.top, rc.right, rc.bottom, 0);
        break;
    case EN_REQUESTRESIZE:
        auto* pReqResize = (REQRESIZE*)lParam;
        SetWindowPos(hwndEdit, HWND_TOP, pReqResize->rc.left, pReqResize->rc.top, pReqResize->rc.right, pReqResize->rc.bottom, 0);
    }
    return DefWindowProc(hwndDlg, message, wParam, lParam);
}

/*
///////////////// LogWindow Class ////////////////////////
*/

LogWindow::LogWindow(HINSTANCE app, HWND hMainWindow)
{
	isDisplayed = false;
    hwndMain = hMainWindow;
    WNDCLASS wc = { };

    wc.lpfnWndProc = LogProc;
    wc.hInstance = app;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    hwndLog = CreateWindowExW(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Game Log",                    // Window text
        WS_OVERLAPPED | WS_CAPTION |  WS_SYSMENU | WS_THICKFRAME,           // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 600,

        hMainWindow,// Parent window    
        nullptr,       // Menu
        app,        // Instance handle
        nullptr        // Additional application data
    );

    //if (hwndLog == NULL)
    //{
    //    LPVOID lpMsgBuf;
    //    LPVOID lpDisplayBuf;
    //    DWORD dw = GetLastError();

    //    FormatMessage(
    //        FORMAT_MESSAGE_ALLOCATE_BUFFER |
    //        FORMAT_MESSAGE_FROM_SYSTEM |
    //        FORMAT_MESSAGE_IGNORE_INSERTS,
    //        NULL,
    //        dw,
    //        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    //        (LPTSTR)&lpMsgBuf,
    //        0, NULL);

    //    // Display the error message and exit the process

    //    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
    //        (lstrlen((LPCTSTR)lpMsgBuf) + 70) * sizeof(TCHAR));
    //    StringCchPrintf((LPTSTR)lpDisplayBuf,
    //        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
    //        TEXT("CreateDialogW failed with error %d: %s"),
    //        dw, lpMsgBuf);
    //    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);
    //}
    
    if (hwndLog != nullptr)
    {
        RECT cR;
        GetClientRect(hwndLog, &cR);
        hwndEdit = CreateRichEdit(hwndLog, cR.left, cR.top, cR.right, cR.bottom, app);
    }
}

void LogWindow::LoadFromFile()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		IFileOpenDialog* pFileOpen;

		// Create the FileOpenDialog object.
		hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
			IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr))
		{
			COMDLG_FILTERSPEC rgSpec[] =
			{
				{ L"Text Files", L"*.txt" },
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
					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
					// Display the file name to the user.
					if (SUCCEEDED(hr))
					{
                        std::filesystem::directory_entry dir = std::filesystem::directory_entry(pszFilePath);
						if (dir.is_regular_file() && (dir.path().extension().compare("txt")))
						{
							std::wifstream hFile(dir.path());
							std::vector<std::string> lines;
							std::wstring line;
							ClearLog();			// Clear the log at the last instant in case the open file was canceled
							while (std::getline(hFile, line))
								PrintLog(line + L'\n');
						}
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}
}

void LogWindow::SaveToFile()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		IFileSaveDialog* pfsd;

		// Create the FileOpenDialog object.
		hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
			IID_IFileSaveDialog, reinterpret_cast<void**>(&pfsd));

		if (SUCCEEDED(hr))
		{
			COMDLG_FILTERSPEC rgSpec[] =
			{
				{ L"Text Files", L"*.txt" }
			};
			hr = pfsd->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);
			{
				hr = pfsd->SetFileTypeIndex(0);
				if (SUCCEEDED(hr))
				{
					// Set default file extension.
					hr = pfsd->SetDefaultExtension(L"txt");
				}
			}
			if (SUCCEEDED(hr))
			{
				// Show the Save dialog box.
				hr = pfsd->Show(nullptr);
				// Get the file name from the dialog box.
				if (SUCCEEDED(hr))
				{
					IShellItem* pItem;
					hr = pfsd->GetResult(&pItem);
					if (SUCCEEDED(hr))
					{
						PWSTR pszFilePath;
						hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
						// Display the file name to the user.
						if (SUCCEEDED(hr))
						{
							HANDLE hFile = CreateFileW(pszFilePath,
								GENERIC_READ | GENERIC_WRITE,
								FILE_SHARE_READ,
								NULL,
								CREATE_ALWAYS,  // Let's always create this file.
								FILE_ATTRIBUTE_NORMAL,
								NULL);

							hr = (hFile == INVALID_HANDLE_VALUE) ? HRESULT_FROM_WIN32(GetLastError()) : S_OK;
							if (SUCCEEDED(hr))
							{
								GETTEXTLENGTHEX stlen = { GTL_NUMBYTES | GTL_CLOSE, 1200 };	// unicode bytes
								DWORD numTchars = (DWORD)SendMessage(hwndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&stlen, 0);
								GETTEXTEX stext =
								{
									numTchars,
									GT_DEFAULT,
									1200,
									NULL,
									nullptr
								};
								TCHAR* tbuf = (TCHAR*)malloc(numTchars);
								SendMessage(hwndEdit, EM_GETTEXTEX, (WPARAM)&stext, (LPARAM)tbuf);   // insert new string at end
								hr = _WriteDataToFile(hFile, tbuf);
								CloseHandle(hFile);
								free(tbuf);
							}
						}
						pItem->Release();
					}
				}
				pfsd->Release();
			}
		}
		CoUninitialize();
	}
}

void LogWindow::ShowLogWindow()
{
    HMENU hm = GetMenu(hwndMain);
    SetForegroundWindow(hwndLog);
    ShowWindow(hwndLog, SW_SHOWNORMAL);
    CheckMenuItem(hm, ID_LOGWINDOW_SHOW, MF_BYCOMMAND | MF_CHECKED);
	isDisplayed = true;
}

void LogWindow::HideLogWindow()
{
	HMENU hm = GetMenu(hwndMain);
    ShowWindow(hwndLog, SW_HIDE);
	CheckMenuItem(hm, ID_LOGWINDOW_SHOW, MF_BYCOMMAND | MF_UNCHECKED);
	isDisplayed = false;
}

bool LogWindow::IsLogWindowDisplayed()
{
    return isDisplayed;
}

void LogWindow::ClearLog()
{
	SendMessage(hwndEdit, EM_SETSEL, 0, -1);					// Select all
	SendMessage(hwndEdit, EM_REPLACESEL, TRUE, (LPARAM)L"");    // Clear
}

void LogWindow::AppendLog(const wchar_t wchar, bool shouldPrint)
{
	bool printIt = false;
	currentBuffer.append(1, wchar);
	if (shouldPrint)
	{
		printIt = true;
	}
	/*
	else if (wchar == L'\n')
	{
		printIt = true;
	}
	*/

	if (printIt)
	{
		if (currentBuffer != L"\n")
			PrintLog(currentBuffer);
		currentBuffer.clear();
	}
}

void LogWindow::PrintLog(std::wstring str)
{
	// if (m_prevLogString == str)	// Avoid duplicates
	//	return;
	// m_prevLogString.assign(str);
	// if (str == L"key>")
	//	str = L"key>\n\n";
    // the edit window uses ES_AUTOVSCROLL so it will automatically scroll the bottom text
    CHARRANGE selectionRange = { -1, -1 };
    SendMessage(hwndEdit, EM_EXSETSEL, 0, (LPARAM)&selectionRange);			// remove selection
    SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)str.c_str());		// insert new string at end
    SendMessage(hwndEdit, EM_REQUESTRESIZE, 0, 0);							// resize for text wrapping
}

