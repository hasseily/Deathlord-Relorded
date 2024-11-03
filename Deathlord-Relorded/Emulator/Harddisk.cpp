/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2015, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Hard drive emulation
 *
 * Author: Copyright (c) 2005, Robert Hoem
 */

#include "pch.h"

#include "Harddisk.h"
#include "AppleWin.h"
#include "CardManager.h"
#include "CPU.h"
#include "DiskImage.h"	// ImageError_e, Disk_Status_e
#include "DiskImageHelper.h"
#include "Memory.h"
#include "resource.h"
#include "../Game.h"
#include <shobjidl_core.h>
#include <filesystem>

/*
Memory map:

    C0F0	(r)   EXECUTE AND RETURN STATUS
	C0F1	(r)   STATUS (or ERROR)
	C0F2	(r/w) COMMAND
	C0F3	(r/w) UNIT NUMBER
	C0F4	(r/w) LOW BYTE OF MEMORY BUFFER
	C0F5	(r/w) HIGH BYTE OF MEMORY BUFFER
	C0F6	(r/w) LOW BYTE OF BLOCK NUMBER
	C0F7	(r/w) HIGH BYTE OF BLOCK NUMBER
	C0F8    (r)   NEXT BYTE
*/

/*
Hard drive emulation in Applewin.

Concept
    To emulate a 32mb hard drive connected to an Apple IIe via Applewin.
    Designed to work with Autoboot Rom and Prodos.

Overview
  1. Hard drive image file
      The hard drive image file (.HDV) will be formatted into blocks of 512
      bytes, in a linear fashion. The internal formatting and meaning of each
      block to be decided by the Apple's operating system (ProDos). To create
      an empty .HDV file, just create a 0 byte file (I prefer the debug method).
  
  2. Emulation code
      There are 4 commands Prodos will send to a block device.
      Listed below are each command and how it's handled:

      1. STATUS
          In the emulation's case, returns only a DEVICE OK (0) or DEVICE I/O ERROR (8).
          DEVICE I/O ERROR only returned if no HDV file is selected.

      2. READ
          Loads requested block into a 512 byte buffer by attempting to seek to
            location in HDV file.
          If seek fails, returns a DEVICE I/O ERROR.  Resets hd_buf_ptr used by HD_NEXTBYTE
          Returns a DEVICE OK if read was successful, or a DEVICE I/O ERROR otherwise.

      3. WRITE
          Copies requested block from the Apple's memory to a 512 byte buffer
            then attempts to seek to requested block.
          If the seek fails (usually because the seek is beyond the EOF for the
            HDV file), the Emulation will attempt to "grow" the HDV file to accomodate.
            Once the file can accomodate, or if the seek did not fail, the buffer is
            written to the HDV file.  NOTE: A2PC will grow *AND* shrink the HDV file.
          I didn't see the point in shrinking the file as this behaviour would require
            patching prodos (to detect DELETE FILE calls).

      4. FORMAT
          Ignored.  This would be used for low level formatting of the device
            (as in the case of a tape or SCSI drive, perhaps).

  3. Bugs
      The only thing I've noticed is that Copy II+ 7.1 seems to crash or stall
      occasionally when trying to calculate how many free block are available
      when running a catalog.  This might be due to the great number of blocks
      available.  Also, DDD pro will not optimise the disk correctally (it's
      doing a disk defragment of some sort, and when it requests a block outside
      the range of the image file, it starts getting I/O errors), so don't
      bother.  Any program that preforms a read before write to an "unwritten"
      block (a block that should be located beyond the EOF of the .HDV, which is
      valid for writing but not for reading until written to) will fail with I/O
      errors (although these are few and far between).

      I'm sure there are programs out there that may try to use the I/O ports in
      ways they weren't designed (like telling Ultima 5 that you have a Phasor
      sound card in slot 7 is a generally bad idea) will cause problems.
*/

struct HDD
{
	HDD()
	{
		clear();
	}

	void clear()
	{
		// This is not a POD (there is a std::string)
		// ZeroMemory does not work
		imagename.clear();
		fullname.clear();
		imagehandle = NULL;
		bWriteProtected = false;
		hd_error = 0;
		hd_memblock = 0;
		hd_diskblock = 0;
		hd_buf_ptr = 0;
		hd_imageloaded = false;
		ZeroMemory(hd_buf, sizeof(hd_buf));
#if HD_LED
		hd_status_next = DISK_STATUS_OFF;
		hd_status_prev = DISK_STATUS_OFF;
#endif
	}

	// From FloppyDisk
	std::string	imagename;	// <FILENAME> (ie. no extension)
	std::string fullname;	// <FILENAME.EXT>
	ImageInfo*	imagehandle;			// Init'd by HD_Insert() -> ImageOpen()
	bool	bWriteProtected;			// Needed for ImageOpen() [otherwise not used]
	//
	BYTE	hd_error;		// NB. Firmware requires that b0=0 (OK) or b0=1 (Error)
	WORD	hd_memblock;
	UINT	hd_diskblock;
	WORD	hd_buf_ptr;
	bool	hd_imageloaded;
	BYTE	hd_buf[HD_BLOCK_SIZE+1];	// Why +1? Probably for erroreous reads beyond the block size (ie. reads from I/O addr 0xC0F8)

#if HD_LED
	Disk_Status_e hd_status_next;
	Disk_Status_e hd_status_prev;
#endif
};

static bool	g_bHD_RomLoaded = false;
static bool g_bHD_Enabled = false;

static BYTE	g_nHD_UnitNum = HARDDISK_1<<7;	// b7=unit

// The HDD interface has a single Command register for both drives:
// . ProDOS will write to Command before switching drives
static BYTE	g_nHD_Command;

static HDD g_HardDisk[NUM_HARDDISKS];

static bool g_bSaveDiskImage = true;	// Save the DiskImage name to conf file (not registry)
static UINT g_uSlot = 7;

//===========================================================================

static void HD_SaveLastDiskImage(const int iDrive);

static void HD_CleanupDrive(const int iDrive)
{
	if (g_HardDisk[iDrive].imagehandle)
	{
		ImageClose(g_HardDisk[iDrive].imagehandle);
		g_HardDisk[iDrive].imagehandle = NULL;
	}

	g_HardDisk[iDrive].hd_imageloaded = false;

	g_HardDisk[iDrive].imagename.clear();
	g_HardDisk[iDrive].fullname.clear();

	HD_SaveLastDiskImage(iDrive);
}

//-----------------------------------------------------------------------------

static void NotifyInvalidImage(const std::wstring pszImageFilename)
{
	MessageBox(g_hFrameWindow, L"Invalid HDV file.\nPlease choose another.", L"Warning", MB_ICONASTERISK | MB_OK);
}

//===========================================================================

BOOL HD_Insert(const int iDrive, const std::wstring & pszImageFilename);

//===========================================================================

static void HD_SaveLastDiskImage(const int iDrive)
{
	_ASSERT(iDrive == HARDDISK_1);

	if (!g_bSaveDiskImage)
		return;

	wchar_t szPathName[MAX_PATH];
	wcscpy(szPathName, HD_GetFullPathName(iDrive).c_str());
	if (wcsrchr(szPathName, TEXT('\\')))
	{
		g_nonVolatile.hdvPath = szPathName;
		g_nonVolatile.SaveToDisk();
	}
}

//===========================================================================

// (Nearly) everything below is global

static BYTE __stdcall HD_IO_EMUL(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

static const DWORD HDDRVR_SIZE = APPLE_SLOT_SIZE;

bool HD_CardIsEnabled(void)
{
	return g_bHD_RomLoaded && g_bHD_Enabled;
}

// Called by:
// . LoadConfiguration() - Done at each restart
// . RestoreCurrentConfig() - Done when Config dialog is cancelled
void HD_SetEnabled(const bool bEnabled)
{
	if(g_bHD_Enabled == bEnabled)
		return;

	g_bHD_Enabled = bEnabled;

	if (bEnabled)
		GetCardMgr().Insert(SLOT7, CT_GenericHDD);
	else
		GetCardMgr().Remove(SLOT7);
}

//-------------------------------------

const std::string& HD_GetFullName(const int iDrive)
{
	return g_HardDisk[iDrive].fullname;
}

const std::wstring& HD_GetFullPathName(const int iDrive)
{
	return ImageGetPathname(g_HardDisk[iDrive].imagehandle);
}

//-------------------------------------

void HD_Reset(void)
{
	g_HardDisk[HARDDISK_1].hd_error = 0;
}

//-------------------------------------

void HD_Load_Rom(const LPBYTE pCxRomPeripheral, const UINT uSlot)
{
	if(!g_bHD_Enabled)
		return;

	HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_HDDRVR_FW), L"FIRMWARE");
	if(hResInfo == NULL)
		return;

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if(dwResSize != HDDRVR_SIZE)
		return;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if(hResData == NULL)
		return;

	BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
	if(pData == NULL)
		return;

	g_uSlot = uSlot;
	memcpy(pCxRomPeripheral + uSlot*256, pData, HDDRVR_SIZE);
	g_bHD_RomLoaded = true;

	RegisterIoHandler(g_uSlot, HD_IO_EMUL, HD_IO_EMUL, NULL, NULL, NULL, NULL);
}

void HD_Destroy(void)
{
	g_bSaveDiskImage = false;
	HD_CleanupDrive(HARDDISK_1);

	g_bSaveDiskImage = true;
}

// Pre: pszImageFilename is qualified with path
BOOL HD_Insert(const int iDrive, const std::wstring & pszImageFilename)
{
	if (pszImageFilename.empty())
		return FALSE;

	if (g_HardDisk[iDrive].hd_imageloaded)
		HD_Unplug(iDrive);

	const bool bCreateIfNecessary = false;		// NB. Don't allow creation of HDV files
	const bool bExpectFloppy = false;
	const bool bIsHarddisk = true;
	ImageError_e Error = ImageOpen(pszImageFilename,
		&g_HardDisk[iDrive].imagehandle,
		&g_HardDisk[iDrive].bWriteProtected,
		bCreateIfNecessary,
		bExpectFloppy);

	g_HardDisk[iDrive].hd_imageloaded = (Error == eIMAGE_ERROR_NONE);

	if (Error == eIMAGE_ERROR_NONE)
	{
		HD_SaveLastDiskImage(iDrive);
		// If we've just loaded the boot drive, tell Remote Control
		if (iDrive == 0)
		{
			g_RemoteControlMgr.setLoadedHDInfo(g_HardDisk[iDrive].imagehandle);
		}
	}

	return g_HardDisk[iDrive].hd_imageloaded;
}

static bool HD_SelectImage(const int drive, LPCWSTR pszFilename)
{
	bool bRes = false;
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
				{ L"HDV Files", L"*.hdv" },
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
						if (dir.is_regular_file() && (dir.path().extension().compare("hdv")))
						{
							std::wstring filename = dir.path().wstring();
							if (HD_Insert(drive, filename))
							{
								bRes = true;
							}
							else
							{
								NotifyInvalidImage(filename);
							}
						}
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}

	return bRes;
}

bool HD_Select(const int iDrive)
{
	return HD_SelectImage(iDrive, TEXT(""));
}

void HD_Unplug(const int iDrive)
{
	if (g_HardDisk[iDrive].hd_imageloaded)
	{
		HD_CleanupDrive(iDrive);
		// RIK BEGIN
		if (iDrive == 0)
		{
			g_RemoteControlMgr.setLoadedHDInfo(NULL);
		}
		// RIK END
	}
}

bool HD_IsDriveUnplugged(const int iDrive)
{
	return g_HardDisk[iDrive].hd_imageloaded == false;
}

//-----------------------------------------------------------------------------

#define DEVICE_OK				0x00
#define DEVICE_UNKNOWN_ERROR	0x28
#define DEVICE_IO_ERROR			0x27

static BYTE __stdcall HD_IO_EMUL(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	BYTE r = DEVICE_OK;
	addr &= 0xFF;

	if (!HD_CardIsEnabled())
		return r;

	HDD* pHDD = &g_HardDisk[g_nHD_UnitNum >> 7];	// bit7 = drive select
	
	if (bWrite == 0) // read
	{
		switch (addr)
		{
			case 0xF0:
				if (pHDD->hd_imageloaded)
				{
					// based on loaded data block request, load block into memory
					// returns status
					switch (g_nHD_Command)
					{
						default:
						case 0x00: //status
							if (ImageGetImageSize(pHDD->imagehandle) == 0)
							{
								pHDD->hd_error = 1;
								r = DEVICE_IO_ERROR;
							}
							break;
						case 0x01: //read
							if ((pHDD->hd_diskblock * HD_BLOCK_SIZE) < ImageGetImageSize(pHDD->imagehandle))
							{
								bool bRes = ImageReadBlock(pHDD->imagehandle, pHDD->hd_diskblock, pHDD->hd_buf);
								if (bRes)
								{
									pHDD->hd_error = 0;
									r = 0;
									pHDD->hd_buf_ptr = 0;
								}
								else
								{
									pHDD->hd_error = 1;
									r = DEVICE_IO_ERROR;
								}
							}
							else
							{
								pHDD->hd_error = 1;
								r = DEVICE_IO_ERROR;
							}
							break;
						case 0x02: //write
							{
								bool bRes = true;
								const bool bAppendBlocks = (pHDD->hd_diskblock * HD_BLOCK_SIZE) >= ImageGetImageSize(pHDD->imagehandle);

								if (bAppendBlocks)
								{
									ZeroMemory(pHDD->hd_buf, HD_BLOCK_SIZE);

									// Inefficient (especially for gzip/zip files!)
									UINT uBlock = ImageGetImageSize(pHDD->imagehandle) / HD_BLOCK_SIZE;
									while (uBlock < pHDD->hd_diskblock)
									{
										bRes = ImageWriteBlock(pHDD->imagehandle, uBlock++, pHDD->hd_buf);
										_ASSERT(bRes);
										if (!bRes)
											break;
									}
								}

								MoveMemory(pHDD->hd_buf, mem+pHDD->hd_memblock, HD_BLOCK_SIZE);

								if (bRes)
									bRes = ImageWriteBlock(pHDD->imagehandle, pHDD->hd_diskblock, pHDD->hd_buf);

								if (bRes)
								{
									pHDD->hd_error = 0;
									r = 0;
								}
								else
								{
									pHDD->hd_error = 1;
									r = DEVICE_IO_ERROR;
								}
							}
							break;
						case 0x03: //format
							break;
					}
				}
				else
				{
					pHDD->hd_error = 1;
					r = DEVICE_UNKNOWN_ERROR;
				}
			break;
		case 0xF1: // hd_error
			if (pHDD->hd_error)
			{
				_ASSERT(pHDD->hd_error & 1);
				pHDD->hd_error |= 1;	// Firmware requires that b0=1 for an error
			}

			r = pHDD->hd_error;
			break;
		case 0xF2:
			r = g_nHD_Command;
			break;
		case 0xF3:
			r = g_nHD_UnitNum;
			break;
		case 0xF4:
			r = (BYTE)(pHDD->hd_memblock & 0x00FF);
			break;
		case 0xF5:
			r = (BYTE)(pHDD->hd_memblock & 0xFF00 >> 8);
			break;
		case 0xF6:
			r = (BYTE)(pHDD->hd_diskblock & 0x00FF);
			break;
		case 0xF7:
			r = (BYTE)(pHDD->hd_diskblock & 0xFF00 >> 8);
			break;
		case 0xF8:
			r = pHDD->hd_buf[pHDD->hd_buf_ptr];
			if (pHDD->hd_buf_ptr < sizeof(pHDD->hd_buf)-1)
				pHDD->hd_buf_ptr++;
			break;
		default:
			return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
		}
	}
	else // write to registers
	{
		switch (addr)
		{
		case 0xF2:
			g_nHD_Command = d;
			break;
		case 0xF3:
			// b7    = drive#
			// b6..4 = slot#
			// b3..0 = ?
			g_nHD_UnitNum = d;
			break;
		case 0xF4:
			pHDD->hd_memblock = (pHDD->hd_memblock & 0xFF00) | d;
			break;
		case 0xF5:
			pHDD->hd_memblock = (pHDD->hd_memblock & 0x00FF) | (d << 8);
			break;
		case 0xF6:
			pHDD->hd_diskblock = (pHDD->hd_diskblock & 0xFF00) | d;
			break;
		case 0xF7:
			pHDD->hd_diskblock = (pHDD->hd_diskblock & 0x00FF) | (d << 8);
			break;
		default:
			return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
		}
	}

	return r;
}
