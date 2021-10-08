#pragma once
#include <windows.h>

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
constexpr UINT8 PNGTW = 14;
constexpr UINT8 PNGTH = 16;
constexpr UINT8 PNGTILESPERROW = 16;
constexpr UINT8 PNGTILESPERCOL	= 16;
constexpr auto PNGBUFFERWIDTHB = PNGTW * PNGTILESPERROW * sizeof(UINT32);
constexpr auto PNGBUFFERHEIGHT = PNGTH * PNGTILESPERCOL;
constexpr auto PNGBUFFERSIZE = PNGBUFFERWIDTHB * PNGBUFFERHEIGHT;


class TilesetCreator
{
public:
	bool isActive = false;
	UINT32 iInserted = 0;
	TilesetCreator()
	{
		pTilesetBuffer = new char[PNGBUFFERSIZE];
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
	void readTileFile();
	void saveTileFile();
private:
	char* pTilesetBuffer;
	UINT8 aKnownTiles[255] = {0};
};


