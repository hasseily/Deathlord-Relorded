#include "pch.h"
#include "DeathlordHacks.h"
#include "Emulator/Memory.h"
#include "HAUtils.h"
#include <string>

constexpr int MAP_START_MEM = 0xC00;		// Start of memory area of the in-game map
constexpr UINT8 MAP_WIDTH = 64;
constexpr int MAP_LENGTH = MAP_WIDTH * MAP_WIDTH;			// Size of map (in bytes)


DeathlordHacks::DeathlordHacks()
{

}

void DeathlordHacks::saveMapDataToDisk()
{
	std::string sMapData = "";
	UINT8 *memPtr = MemGetBankPtr(0);
	memPtr += MAP_START_MEM;
	char tileId[3] = "00";
	for (size_t i = 0; i < MAP_LENGTH; i++)
	{
		sprintf(tileId, "%02X", memPtr[i]);
		if (i != 0)
		{
			if ((i % MAP_WIDTH) == 0)
				sMapData.append("\n");
			else
				sMapData.append(" ");
		}
		sMapData.append(tileId, 2);
	}
	
	std::wstring wbuf;
	HA::ConvertStrToWStr(&sMapData, &wbuf);
	OutputDebugString(wbuf.c_str());
	

}
