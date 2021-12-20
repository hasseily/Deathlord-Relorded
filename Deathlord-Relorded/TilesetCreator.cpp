#include "pch.h"
#include "TilesetCreator.h"
#include "Emulator/Applewin.h"
#include "Emulator/Memory.h"
#include <fstream>
#include <filesystem>
#include "RemoteControl/RemoteControlManager.h"
#include <map>
#include "Game.h"

// below because "The declaration of a static data member in its class definition is not a definition"
TilesetCreator* TilesetCreator::s_instance;

/// <summary>
/// Creates a map of the rects on the spritesheet of all tiles based on the tile ID
/// </summary>

void TilesetCreator::FillTileSpritePositions()
{
	for (long j = 0; j < PNGTILESPERCOL; j++)
	{
		for (long i = 0; i < PNGTILESPERROW; i++)
		{
			tileSpritePositions[j * PNGTILESPERROW + i] = {
				i* PNGTW,
				j* PNGTH,
				i* PNGTW + PNGTW,
				j* PNGTH + PNGTH
			};
		}
	}
}

/// <summary>
/// Goes through all the tiles in HGR2 and parses them, adding them to the PNG patchwork
/// </summary>
/// <return>A pointer to the buffer with RGBA data</return>
LPBYTE TilesetCreator::parseTilesInHGR2()
{
    /*
        The plan is to:
            - pause the emulator
            - save the current HGR1 memory for restoring it at the end.
            - take each tile from HGR2 stored linearly and put it inside HGR1 correctly
            - run the video renderer
            - extract from the framebuffer the rendered tile
            - store it correctly in the tilemap
            - restore HGR1
            - unpause the emulator
    */

	UINT memPtrHGR1Base = 0x2000;
	UINT memPtrHGR2Base = 0x4000;
    UINT sizeOfHGR = 0x1fff;
    UINT tilesParsed = 0;

    g_nAppMode = AppMode_e::MODE_PAUSED;

    LPBYTE snapshotHGR1 = new BYTE[sizeOfHGR];
    memcpy_s(snapshotHGR1, sizeOfHGR, MemGetMainPtr(memPtrHGR1Base), sizeOfHGR);
    memset(MemGetMainPtr(memPtrHGR1Base), 0, sizeOfHGR);    // blacken the whole HGR1
	VideoRedrawScreen();

	UINT8 tileByteSize = 2 * 16;   // 2 bytes in each of 16 lines
	UINT8 tilesSentToHGR1 = 0;

	UINT32 fbWidth = GetFrameBufferWidth();
	UINT32 fbHeight = GetFrameBufferHeight();
	UINT32 fbBorderLeft = GetFrameBufferBorderWidth();
	UINT32 fbBorderTop = GetFrameBufferBorderHeight();

    for (UINT32 iT = 0; iT < (tileByteSize * 0x100); iT += tileByteSize)  // Tile by tile, stop at 128 tiles (the rest is garbage)
    {
        for (size_t jT = 0; jT < tileByteSize; jT += 2)     // jump line by line, 2 bytes at a time
        {
            // Copy the 2 bytes of each line onto HGR1
            UINT8 lineNumber = jT / 2;
            memcpy_s(MemGetMainPtr(memPtrHGR1Base + EIGHTLINEADDRESSES[lineNumber / 8] + SINGLELINEOFFSETS[lineNumber % 8] + tilesSentToHGR1*4), 2, MemGetMainPtr(memPtrHGR2Base + iT + jT), 2);
        }
		tilesSentToHGR1++;
		if ((tilesSentToHGR1 < 10) && (iT < (tileByteSize * 0xff)))	// Use one line to hold 10 tiles at a time. We're batch processing 10 tiles at once, with black tiles in between.
			continue;

		// Now get AppleWin to do all the video magic and generate the modern framebuffer data
		VideoRedrawScreen();

		// And extract the 10 tiles from the framebuffer into the tileset framebuffer
		for (size_t nTile = 0; nTile < tilesSentToHGR1; nTile++)
		{
			UINT32 iFBOriginByte = (fbBorderTop * fbWidth * PIXELDEPTH) + (fbBorderLeft * PIXELDEPTH) + nTile * 2 * FBTW * PIXELDEPTH;
			// Calculate the destination 0,0 byte to draw the tile onto
			UINT8 tileNumber = (iT - (tilesSentToHGR1 - nTile - 1) * tileByteSize) / tileByteSize;
			UINT8 iPNGRow = tileNumber / 0x10;
			UINT8 iPNGCol = tileNumber % 0x10;
			UINT32 iPNGOriginByte = (iPNGRow * PNGTH * PNGBUFFERWIDTHB) + (iPNGCol * PNGTW * PIXELDEPTH);

			// Map every pixel of the source tile onto the destination tile
			// But only parse every other line, and skip every other pixel
			UINT8 b0, b1, b2, b3;   // The individual pixel bytes, low to high
			UINT32 iFBCurrentByte;
			UINT32 iPNGCurrentByte;

			for (UINT32 j = 0; j < FBTH; j++)
			{
				//if (j % 2)
				//	continue;
				for (UINT32 i = 0; i < FBTW; i++)
				{
					//	if (i % 2)
					//		continue;
					iFBCurrentByte = iFBOriginByte + (j * fbWidth * PIXELDEPTH) + (i * PIXELDEPTH);
					b0 = g_pFramebufferbits[iFBCurrentByte];
					b1 = g_pFramebufferbits[iFBCurrentByte + 1];
					b2 = g_pFramebufferbits[iFBCurrentByte + 2];
					b3 = g_pFramebufferbits[iFBCurrentByte + 3];

					// Swap the RGB bytes, and force alpha to be opaque because when rgb==ffffff (white), a==00.
					iPNGCurrentByte = iPNGOriginByte + (j * PNGBUFFERWIDTHB) + (i * PIXELDEPTH);
					pTilesetBuffer[iPNGCurrentByte] = b2;
					pTilesetBuffer[iPNGCurrentByte + 1] = b1;
					pTilesetBuffer[iPNGCurrentByte + 2] = b0;
					pTilesetBuffer[iPNGCurrentByte + 3] = 0xFF;
				}
			}
			++tilesParsed;
		}
		tilesSentToHGR1 = 0;
		memset(MemGetMainPtr(memPtrHGR1Base), 0, sizeOfHGR);    // blacken the whole HGR1
		VideoRedrawScreen();
    }

    // bring everything back the way it was
    memcpy_s(MemGetMainPtr(memPtrHGR1Base), sizeOfHGR, snapshotHGR1, sizeOfHGR);
    delete[] snapshotHGR1;
    snapshotHGR1 = nullptr;
	VideoRedrawScreen();

	g_nAppMode = AppMode_e::MODE_RUNNING;
	return pTilesetBuffer;
}

//************************************
// Method:    extractSpritesFromMemory
// FullName:  TilesetCreator::extractSpritesFromMemory
// Access:    public 
// Returns:   void
// Qualifier:
// Parameter: int startAddress
// Parameter: UINT8 spriteCount
// Parameter: std::string fileName

// Description: 
// Given an address in memory, it extracts spriteCount 14x16 HGR sprites and saves them to fileName.
// The sprites should be 2 bytes per row, with 16 rows each, totalling 32 bytes per sprite
// The drawing style is RGB, without artifacts. The algorithms is extracted from AppleWin's UpdateHiResRGBCell()
//************************************
void TilesetCreator::extractSpritesFromMemory (int startAddress, UINT8 spriteCount,
	UINT8 spritesPerRow, std::wstring fileName)
{
	int memStart = startAddress;
	int tilesTotal = spriteCount;
	std::wstring saveFileName(L"Deathlord Sprites.data");
	if (!fileName.empty())
		saveFileName = fileName;

	constexpr UINT8 tileHeight = 16;	// number of rows per tile
	constexpr UINT8 bytesPerRow = 2;
	constexpr UINT8 dotsPerByte = 7;	// 7 dots in each byte, 2 dots per pixel
	constexpr UINT16 dotsPerRow = bytesPerRow * dotsPerByte;

		// Group 1: 0b01: green (odd), 0b10: violet (even).     Group 2: 0b01: orange (odd), 0b10: blue (even)
#define  SETRGBCOLOR(r,g,b) {r,g,b,0xff}
	static RGBQUAD pPalette[] =
	{
	SETRGBCOLOR(/*HGR_BLACK, */ 0x00,0x00,0x00),
	SETRGBCOLOR(/*HGR_WHITE, */ 0xFF,0xFF,0xFF),
	SETRGBCOLOR(/*BLUE,      */ 0x0D,0xA1,0xFF),
	SETRGBCOLOR(/*ORANGE,    */ 0xF2,0x5E,0x00),
	SETRGBCOLOR(/*GREEN,     */ 0x38,0xCB,0x00),
	SETRGBCOLOR(/*MAGENTA,   */ 0xC7,0x34,0xFF),
	};

	UINT32* rgbaTiles = new UINT32[tilesTotal * tileHeight * dotsPerRow];	// RGBA pixels, 7 per 2 bytes of mem
	memset(rgbaTiles, 0, tilesTotal * tileHeight * dotsPerRow * sizeof(UINT32));
	int rgbaStart = 0;

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
						rgbaTiles[rgbaStart] = colors[i];
						rgbaStart++;
					}
					else
					{
						// B&W pixel
						rgbaTiles[rgbaStart] = bw[(dwordval & chck2 ? 1 : 0)];
						rgbaStart++;
					}
					// Next pixel
					dwordval = dwordval >> 1;
				}
			}
			memStart += 2;
		}
	}

	// Now we have a file a single column of sprites wide
	// Ideally the file is 8 sprites wide, it's much easier to both visualize
	// and calculate the sprite position (since each sprite has 2 bytes)
	
	UINT32 currentMatrixRow = 0;
	UINT32 currentMatrixCol = 0;
	UINT32 lastDotIndex = 0;

	// The tile matrix has to be a multiple of sprites per row, with the last sprites
	// being transparent.
	UINT32 matrixTileTotal = (1 + tilesTotal / spritesPerRow) * spritesPerRow;
	UINT32* tileMatrix = new UINT32[matrixTileTotal * tileHeight * dotsPerRow];
	memset(tileMatrix, 0, matrixTileTotal* tileHeight* dotsPerRow * sizeof(UINT32));

	UINT32* mPtr = tileMatrix;
	UINT32* tPtr = rgbaTiles;


	for (UINT32 _tile = 0; _tile < (tilesTotal); _tile++)
	{
		currentMatrixRow = _tile / spritesPerRow;
		currentMatrixCol = _tile % spritesPerRow;
		mPtr = tileMatrix
			+ currentMatrixRow * spritesPerRow * (dotsPerRow * tileHeight)	// skip all the filled rows
			+ currentMatrixCol * dotsPerRow;							// skip the first line up to our tile
		for (UINT32 _row = 0; _row < tileHeight; _row++)
		{
			memcpy_s(mPtr, dotsPerRow * sizeof(UINT32), tPtr, dotsPerRow * sizeof(UINT32));
			mPtr += spritesPerRow * dotsPerRow;
			tPtr += dotsPerRow;
			lastDotIndex += dotsPerRow;
		}
	}
	std::fstream fsFile(saveFileName.c_str(), std::ios::out | std::ios::binary);
	fsFile.write((char*)tileMatrix, tilesTotal * tileHeight * dotsPerRow * sizeof(UINT32));
	fsFile.close();
	mPtr = NULL;
	tPtr = NULL;
	delete[] tileMatrix;
	delete[] rgbaTiles;
	return;
}

/// <summary>
/// This is not part of tileset creation, but its location here is as good as any.
/// This method looks at the visible screen of deathlord (the 9x9 tiles in the upper left)
/// And checks which tiles not fully black (i.e. visible).
/// It fills a passed in array of 9x9=81 UINT8s with 0-1 based on their visibility
/// It's up to the caller to both determine the map tiles that have been analyzed, and act upon them
/// </summary>
/// <param name="pVisibleTiles">A pointer to a 81-item array of UINT8s</param>
/// <returns>Nothing</returns>
void TilesetCreator::analyzeVisibleTiles(UINT8* pVisibleTiles)
{
	if (!g_isInGameMap)
		return;
	UINT32 fbWidth = GetFrameBufferWidth();
	UINT32 fbBorderLeft = GetFrameBufferBorderWidth();		// these are additional shifts to make the tiles align
	UINT32 fbBorderTop = GetFrameBufferBorderHeight() + 16;	// these are additional shifts to make the tiles align
	UINT32 iFBOriginByte;
	UINT32 iFBCurrentByte;
	bool isBlack;
	const UINT8 visTileSide = 9;	// 9 tiles per side visible

	for (UINT8 ir = 0; ir < visTileSide; ir++)		// rows
	{
		for (UINT8 jc = 0; jc < visTileSide; jc++)	// columns
		{
			isBlack = true;
			// find the top left byte of the tile
			iFBOriginByte = ((fbBorderTop + ir * FBTH) * fbWidth * PIXELDEPTH)
				+ (fbBorderLeft + jc * FBTW) * PIXELDEPTH;
			// Now read every byte triplet BGR and see if the whole thing is black (discard the Alpha byte)
			// except that we don't want to read the 2-pixel thick border which might have some bleeding.
			pVisibleTiles[ir * visTileSide + jc] = 0;	// default to not visible
			for (UINT32 br = 2; br < (FBTH-2); br++)
			{
				for (UINT32 bc = 2; bc < (FBTW-2); bc++)
				{
					iFBCurrentByte = iFBOriginByte + (br * fbWidth * PIXELDEPTH) + (bc * PIXELDEPTH);
					if ((g_pFramebufferbits[iFBCurrentByte] != 0)
						|| (g_pFramebufferbits[iFBCurrentByte + 1] != 0)
						|| (g_pFramebufferbits[iFBCurrentByte + 2] != 0))
					{
						pVisibleTiles[ir * visTileSide + jc] = 1; // b or g or r is not black
						isBlack = false;
						goto FINISHEDTILE;
					}
				}
			}
		FINISHEDTILE:
			{
				/*
				char _buf[500];
				sprintf_s(_buf, 500, "Tile at %d,%d is %d\n", ir, jc, isBlack);
				OutputDebugStringA(_buf);
				*/
				
			}

		}
	}
}