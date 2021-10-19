#pragma once
#include <windows.h>
#include <array>

constexpr auto PIXELDEPTH	= 4;	// ARGB

constexpr auto REGIONMAPSTART		= 0xC00;					// Start of Region Map in main memory. Ends at 0x1C00
constexpr auto REGIONMAPEND			= REGIONMAPSTART + 0x1000;	// End of Region Map in main memory. Uses 64k
constexpr auto REGIONMAPWIDTH		= 0x40;						// Width of the Region Map (it's also the height since it's square)
constexpr auto RMAPX				= 0xFC06;					// Location of RMAP X value in main memory. It determines where the player is within the Region Map
constexpr auto RMAPY				= 0xFC07;					// Location of RMAP X value in main memory. It determines where the player is within the Region Map
//constexpr auto VISIBLEORIGINOFFSET	= 0;

// For the framebuffer
constexpr UINT8 FBTW = 28;	// FB tile width in pixels
constexpr UINT8 FBTH = 32;	// FB tile height in pixels
constexpr UINT8 FBTILESPERROW = 9;
constexpr UINT8 FBTILESPERCOL = 9;
constexpr auto FRAMEBUFFERWIDTHB = 600 * sizeof(UINT32);
constexpr auto FRAMEBUFFERHEIGHT = 420;
constexpr auto FRAMEBUFFERSIZE = FRAMEBUFFERWIDTHB * FRAMEBUFFERHEIGHT;
constexpr UINT8 LEFTMARGIN = 20;
constexpr UINT8 TOPMARGIN = 34;

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
	bool isActive = false;
	UINT32 iInserted = 0;
	TilesetCreator()
	{
		pTilesetBuffer = new char[PNGBUFFERSIZE];
		memset(pTilesetBuffer, 0, PNGBUFFERSIZE);
	}
	~TilesetCreator()
	{
		delete[] pTilesetBuffer;
	}
	void start();
	void stop();
	void reset();
	UINT parseTilesInFrameBuffer();
	bool insertTileInTilesetBuffer(UINT32 iTileId, UINT8 iTileX, UINT8 iTileY);
	UINT parseTilesInHGR2();
	void readTileFile();
	void saveTileFile();
private:
	char* pTilesetBuffer;
	UINT8 aKnownTiles[255] = {0};
};


