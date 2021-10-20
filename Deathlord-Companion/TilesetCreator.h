#pragma once
#include <windows.h>
#include "Emulator/Memory.h"
#include <array>

constexpr int GAMEMAP_START_MEM = 0xC00;		// Start of memory area of the in-game map

constexpr auto PIXELDEPTH	= 4;	// RGBA

// For the framebuffer
constexpr UINT8 FBTW = 28;	// FB tile width in pixels
constexpr UINT8 FBTH = 32;	// FB tile height in pixels
constexpr auto FRAMEBUFFERWIDTHB = 600 * PIXELDEPTH;
constexpr auto FRAMEBUFFERHEIGHT = 420;
constexpr auto FRAMEBUFFERSIZE = FRAMEBUFFERWIDTHB * FRAMEBUFFERHEIGHT;
constexpr UINT8 LEFTMARGIN = 20;
constexpr UINT8 TOPMARGIN = 17;

// For the patchwork PNG
constexpr UINT8 PNGTW = 28;
constexpr UINT8 PNGTH = 32;
constexpr UINT8 PNGTILESPERROW = 16;
constexpr UINT8 PNGTILESPERCOL	= 16;
constexpr auto PNGBUFFERWIDTHB = PNGTW * PNGTILESPERROW * sizeof(UINT32);
constexpr auto PNGBUFFERHEIGHT = PNGTH * PNGTILESPERCOL;
constexpr auto PNGBUFFERSIZE = PNGBUFFERWIDTHB * PNGBUFFERHEIGHT;

// For parsing the tileset in HGR2
constexpr std::array<const uint16_t, 192 / 8> EIGHTLINEADDRESSES = {
	0x0000, 0x0080, 0x0100, 0x0180, 0x0200, 0x0280, 0x0300, 0x0380,
	0x0028, 0x00a8, 0x0128, 0x01a8, 0x0228, 0x02a8, 0x0328, 0x03a8,
	0x0050, 0x00d0, 0x0150, 0x01d0, 0x0250, 0x02d0, 0x0350, 0x03d0
};
constexpr std::array<const uint16_t, 8> SINGLELINEOFFSETS = {
	0x0, 0x400, 0x800, 0xc00, 0x1000, 0x1400, 0x1800, 0x1c00
};

class TilesetCreator
{
public:
	UINT32 iInserted = 0;
	TilesetCreator()
	{
		pTilesetBuffer = new BYTE[PNGBUFFERSIZE];
		memset(pTilesetBuffer, 0, PNGBUFFERSIZE);
	}
	~TilesetCreator()
	{
		delete[] pTilesetBuffer;
	}
	LPBYTE parseTilesInHGR2();
	LPBYTE GetCurrentTilesetBuffer() { return pTilesetBuffer; };
	LPBYTE GetCurrentGameMap() { return MemGetMainPtr(GAMEMAP_START_MEM); };
private:
	LPBYTE pTilesetBuffer;
};


