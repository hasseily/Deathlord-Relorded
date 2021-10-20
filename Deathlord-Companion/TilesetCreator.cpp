#include "pch.h"
#include "TilesetCreator.h"
#include "Emulator/Applewin.h"
#include "Emulator/Memory.h"
#include <fstream>
#include <filesystem>
#include "RemoteControl/RemoteControlManager.h"
#include <map>


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

	// LPBYTE _hgr1 = MemGetMainPtr(memPtrHGR1Base);
	// LPBYTE _hgr2 = MemGetMainPtr(memPtrHGR2Base);

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
			UINT32 iFBOriginByte = (TOPMARGIN * FRAMEBUFFERWIDTHB) + (LEFTMARGIN * PIXELDEPTH) + nTile * 2 * FBTW * PIXELDEPTH;
// Calculate the destination 0,0 byte to draw the tile onto
			UINT8 tileNumber = (iT - (tilesSentToHGR1 - nTile - 1) * tileByteSize) / tileByteSize;
			UINT8 iPNGRow = tileNumber / 0x10;
			UINT8 iPNGCol = tileNumber % 0x10;
			UINT iPNGOriginByte = (iPNGRow * PNGTH * PNGBUFFERWIDTHB) + (iPNGCol * PNGTW * PIXELDEPTH);

			// Map every pixel of the source tile onto the destination tile
			// But only parse every other line, and skip every other pixel
			UINT8 b0, b1, b2, b3;   // The individual pixel bytes, low to high
			UINT iFBCurrentByte;
			UINT iPNGCurrentByte;

			for (UINT32 j = 0; j < FBTH; j++)
			{
				//if (j % 2)
				//	continue;
				for (UINT32 i = 0; i < FBTW; i++)
				{
					//	if (i % 2)
					//		continue;
					iFBCurrentByte = iFBOriginByte + (j * FRAMEBUFFERWIDTHB) + (i * PIXELDEPTH);
					b0 = g_pFramebufferbits[iFBCurrentByte];
					b1 = g_pFramebufferbits[iFBCurrentByte + 1];
					b2 = g_pFramebufferbits[iFBCurrentByte + 2];
					b3 = g_pFramebufferbits[iFBCurrentByte + 3];

					// Swap the RGB bytes, and force alpha to be opaque because when rgb==ffffff (white), a==00.
					iPNGCurrentByte = iPNGOriginByte + (j * PNGBUFFERWIDTHB) + (i * PIXELDEPTH);
					pTilesetBuffer[iPNGCurrentByte] = b2;
					pTilesetBuffer[iPNGCurrentByte + 1] = b1;
					pTilesetBuffer[iPNGCurrentByte + 2] = b0;
					pTilesetBuffer[iPNGCurrentByte + 3] = (char)0xFF;
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

	g_nAppMode = AppMode_e::MODE_RUNNING;
	return pTilesetBuffer;
}