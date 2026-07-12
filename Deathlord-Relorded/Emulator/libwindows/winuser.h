#pragma once

#include "winhandles.h"

typedef void *HCURSOR;

#define IDC_WAIT "IDC_WAIT"
#define CB_ERR (-1)
#define CB_ADDSTRING 0x0143
#define CB_RESETCONTENT 0x014b
#define CB_SETCURSEL 0x014e

HCURSOR LoadCursor(HINSTANCE hInstance, LPCSTR lpCursorName);
HCURSOR SetCursor(HCURSOR hCursor);

typedef VOID(CALLBACK *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);

UINT_PTR SetTimer(HWND, UINT_PTR, UINT, TIMERPROC);
BOOL KillTimer(HWND hWnd, UINT uIDEvent);
HWND WINAPI GetDlgItem(HWND, INT);
LRESULT WINAPI SendMessage(HWND, UINT, WPARAM, LPARAM);
void WINAPI PostQuitMessage(INT);

#define VK_CANCEL  0x03
#define VK_BACK    0x08
#define VK_TAB     0x09
#define VK_RETURN  0x0D
#define VK_SHIFT   0x10
#define VK_CONTROL 0x11
#define VK_MENU    0x12
#define VK_ESCAPE  0x1B
#define VK_SPACE   0x20
#define VK_PRIOR   0x21
#define VK_NEXT    0x22
#define VK_END     0x23
#define VK_HOME    0x24
#define VK_LEFT    0x25
#define VK_UP      0x26
#define VK_RIGHT   0x27
#define VK_DOWN    0x28
#define VK_SELECT  0x29
#define VK_PRINT   0x2A
#define VK_EXECUTE 0x2B
#define VK_SNAPSHOT 0x2C
#define VK_INSERT  0x2D
#define VK_DELETE  0x2E
#define VK_HELP    0x2F

#define VK_NUMPAD0 0x60
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_DIVIDE  0x6F

#define VK_F1  0x70
#define VK_F2  0x71
#define VK_F3  0x72
#define VK_F4  0x73
#define VK_F5  0x74
#define VK_F6  0x75
#define VK_F7  0x76
#define VK_F8  0x77
#define VK_F9  0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B

#define VK_LWIN    0x5B
#define VK_RWIN    0x5C
#define VK_APPS    0x5D

#define VK_NUMLOCK 0x90
#define VK_SCROLL  0x91
#define VK_LMENU   0xA4
#define VK_RMENU   0xA5

#define VK_OEM_1   0xBA
#define VK_OEM_2   0xBF
#define VK_OEM_3   0xC0 // '`~' for US
#define VK_OEM_8   0xDF

#define KF_EXTENDED 0x0100
