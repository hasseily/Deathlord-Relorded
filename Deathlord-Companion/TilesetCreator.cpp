#include "pch.h"
#include "TilesetCreator.h"
#include "Emulator/Applewin.h"
#include "Memory.h"
#include <fstream>
#include <filesystem>
#include "RemoteControl/RemoteControlManager.h"


void TilesetCreator::start()
{
    if (isActive == true)
        return;
    isActive = true;
    readTileFile();
    MessageBox(g_hFrameWindow, L"Starting Tileset Creator!", TEXT("Deathlord Tileset"), MB_OK);
}

void TilesetCreator::stop()
{
    if (isActive == false)
        return;
    saveTileFile();
    isActive = false;
}

void TilesetCreator::reset()
{
    if (isActive == false)
        return;
    if (MessageBox(g_hFrameWindow,
        TEXT("Are you sure you want to reset the tileset creator? The tileset file will be emptied!\n\n")
        TEXT("Deathlord Tileset Reset Warning"),
        TEXT("Benchmarks"),
        MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDCANCEL)
        return;
    iInserted = 0;
    ZeroMemory(pTilesetBuffer, PNGBUFFERSIZE);
    for (UINT8 i = 0; i < UINT8_MAX; i++)
    {
        aKnownTiles[i] = 0;
    }
    saveTileFile();
    MessageBox(g_hFrameWindow, L"Tileset Creator has been reset", TEXT("Deathlord Tileset"), MB_OK);
}

void TilesetCreator::readTileFile()
{
    if (isActive == false)
        return;
    std::filesystem::path tilesetPath = std::filesystem::current_path();
    tilesetPath += "\\Maps\\Deathlord Tileset - Auto.data";
    std::fstream fsFile(tilesetPath, std::ios::in | std::ios::binary);
    if (!fsFile.is_open())
    {
        return;
    }
    fsFile.read(pTilesetBuffer, PNGBUFFERSIZE);
    fsFile.close();
}

void TilesetCreator::saveTileFile()
{
    if (isActive == false)
        return;
	std::filesystem::path tilesetPath = std::filesystem::current_path();
	tilesetPath += "\\Maps\\Deathlord Tileset - Auto.data";
    std::fstream fsFile(tilesetPath, std::ios::out | std::ios::binary);
    fsFile.write(pTilesetBuffer, PNGBUFFERSIZE);
    fsFile.close();
    std::wstring msg(L"Saved tile file\nRGBA file is at: Deathlord Tileset - Auto.data\nNumber of tiles loaded: ");
    msg.append(std::to_wstring(iInserted));
    MessageBox(g_hFrameWindow, msg.c_str(), TEXT("Deathlord Tileset"), MB_OK);
}

/// <summary>
/// Goes through all the tiles in the framebuffer and parses them, adding them to the PNG patchwork as necessary
/// This method only calculates a tile's unique ID and checks that it hasn't been processed yet.
/// </summary>
/// <returns>Number of tiles inserted</returns>
UINT TilesetCreator::parseTilesInFrameBuffer()
{
    if (isActive == false)
        return 0;
    wchar_t tmpb[200] = {};
    UINT8 iPlayerX = *MemGetMainPtr(RMAPX);
    UINT8 iPlayerY = *MemGetMainPtr(RMAPY);
    // Calculate the X,Y where the visible tiles start, using the player position as center of the visible box
    UINT8 iFirstTileX = iPlayerX - (FBTILESPERCOL / 2);
	UINT8 iFirstTileY = iPlayerY - (FBTILESPERROW / 2);
    INT8 iTileX;
    INT8 iTileY;
    UINT32 iTileIdLocation; // Tile Id location in main memory
    UINT32 iTileId;         // Tile Id value
    for (UINT8 j = 0; j < FBTILESPERROW; j++)
    {
        for (UINT8 i = 0; i < FBTILESPERCOL; i++)
        {
            iTileX = iPlayerX + i - (FBTILESPERCOL / 2);
            iTileY = iPlayerY + j - (FBTILESPERROW / 2);
            // If the tile goes beyond the max region map boundaries, discard
            if ((iTileX) < 0 || (iTileX >= REGIONMAPWIDTH))
                continue;
			if ((iTileY) < 0 || (iTileY >= REGIONMAPWIDTH))
				continue;
            iTileIdLocation = REGIONMAPSTART + ((iFirstTileY + j) * REGIONMAPWIDTH) + (iFirstTileX + i);
            iTileId = *MemGetMainPtr(iTileIdLocation);
            if (aKnownTiles[iTileId] > 0)
            {
//                sprintf(tmpb, "Tile is known - ID: %2X at location %8X\n", iTileId, iTileIdLocation - REGIONMAPSTART);
//                OutputDebugString(tmpb);
                continue;
            }
            if (insertTileInTilesetBuffer(iTileId, i, j))
            {
                iInserted++;
                wsprintf(tmpb, L"Inserted from %2d,%2d (location %#6x) tile ID: %2x\n", j, i, iTileIdLocation, iTileId);
                OutputDebugString(tmpb);
            }
            else
            {
				wsprintf(tmpb, L"Did NOT insert from %2d,%2d (location %#6x) tile ID: %2x\n", j, i, iTileIdLocation, iTileId);
				OutputDebugString(tmpb);
            }
        }
    }
    return iInserted;
}

/// <summary>
/// Takes a tile from the framebuffer and inserts it into a patchwork buffer in the position of its tile ID.
/// The tileset buffer is that of a patchwork image of FBTILESPERCOL x FBTILESPERROW tiles, each TW x TH pixels big
/// </summary>
/// <param name="iTileId">The tile id of the tile, which will determine its position in the patchwork</param>
/// <param name="iTileNumber">The tile number within the in-game view of the world. Tile 0 is the top left tile. There are FBTILESPERCOL x FBTILESPERROW tiles.</param>
/// <returns>True if tile is inserted, false if not (probably because it wasn't in line of sight</returns>

bool TilesetCreator::insertTileInTilesetBuffer(UINT32 iTileId, UINT8 iTileX, UINT8 iTileY)
{
    if (isActive == false)
        return false;
    // Calculate the source 0,0 byte to get the tile from
    UINT32 iFBOriginByte = ((TOPMARGIN + iTileY * FBTH) * FRAMEBUFFERWIDTHB) + ((LEFTMARGIN + iTileX * FBTW) * sizeof(UINT32));

    // Calculate the destination 0,0 byte to draw the tile onto
    UINT8 iPNGRow = iTileId >> 4;
    UINT8 iPNGCol = iTileId & 0b1111;
    // Same as above, except that there are no margins
    UINT32 iPNGOriginByte = (iPNGRow * PNGTH * PNGBUFFERWIDTHB) + (iPNGCol * PNGTW * sizeof(UINT32));

    // Map every pixel of the source tile onto the destination tile
    // But only parse every other line, and skip every other pixel
    UINT8 b0, b1, b2, b3;   // The individual pixel bytes, low to high
    UINT iFBCurrentByte;
    UINT iPNGCurrentByte;

    // First check if the tile has any non-black pixels
    // We do this twice because even if we did create buffer just for this tile
    // we'd have to loop through it to copy it correctly into the tileset buffer.
    bool bTileIsBlack = true;
    for (UINT32 j = 0; j < FBTH; j++)
    {
        if (j % 2)
            continue;
        for (UINT32 i = 0; i < FBTW; i++)
        {
            if (i % 2)
                continue;
            iFBCurrentByte = iFBOriginByte + (j * FRAMEBUFFERWIDTHB) + (i * sizeof(UINT32));
            b0 = g_pFramebufferbits[iFBCurrentByte];
            b1 = g_pFramebufferbits[iFBCurrentByte + 1];
            b2 = g_pFramebufferbits[iFBCurrentByte + 2];
            b3 = g_pFramebufferbits[iFBCurrentByte + 3];
            if (b0 != 0 || b1 != 0 || b2 != 0)
                bTileIsBlack = false;
        }
    }
    if (bTileIsBlack)
        return false;

    // Now do the real thing
	for (UINT32 j = 0; j < FBTH; j ++)
	{
        if (j % 2)
            continue;
		for (UINT32 i = 0; i < FBTW; i ++)
		{
            if (i % 2)
                continue;
            iFBCurrentByte = iFBOriginByte + (j * FRAMEBUFFERWIDTHB) + (i * sizeof(UINT32));
            b0 = g_pFramebufferbits[iFBCurrentByte];
            b1 = g_pFramebufferbits[iFBCurrentByte + 1];
            b2 = g_pFramebufferbits[iFBCurrentByte + 2];
            b3 = g_pFramebufferbits[iFBCurrentByte + 3];

            // Swap the RGB bytes, and force alpha to be opaque because when rgb==ffffff (white), a==00.
            iPNGCurrentByte = iPNGOriginByte + ((j / 2) * PNGBUFFERWIDTHB) + ((i / 2) * sizeof(UINT32));
            pTilesetBuffer[iPNGCurrentByte] = b2;
            pTilesetBuffer[iPNGCurrentByte + 1] = b1;
            pTilesetBuffer[iPNGCurrentByte + 2] = b0;
            pTilesetBuffer[iPNGCurrentByte + 3] = (char)0xFF;
        }
	}

	aKnownTiles[iTileId] = 1;
	return true;
}
