#include "pch.h"
#include "MemoryTriggers.h"
#include "TilesetCreator.h"
#include "DeathlordHacks.h"

// below because "The declaration of a static data member in its class definition is not a definition"
MemoryTriggers* MemoryTriggers::s_instance;

void MemoryTriggers::Process()
{
	// poll memory and fire triggers at specific intervals
	auto elapsedTime = float(p_timer->GetElapsedSeconds());
    timeSinceMemoryPolled += elapsedTime;
	timeSinceTriggersFired += elapsedTime;
	if (timeSinceMemoryPolled > intervalPollMemory)
	{
		PollKeyMemoryLocations();
        timeSinceMemoryPolled = 0.f;
	}
	if (timeSinceTriggersFired > intervalDelayedTriggers)
	{
		DelayedTriggersProcess();
        timeSinceTriggersFired = 0.f;
	}
}


#pragma region Polling
//************************************
// Method:    PollKeyMemoryLocations
// FullName:  MemoryTriggers::PollKeyMemoryLocations
// Access:    public 
// Returns:   void
// Qualifier:

// Description: 
// Called after every regular keypress or at specific intervals.
// Polls for key memory location changes and fires relevant methods
//************************************
void MemoryTriggers::PollKeyMemoryLocations()
{
    LPBYTE pMem = MemGetMainPtr(0);
    for (auto it = memPollMap.begin(); it != memPollMap.end(); ++it)
    {
        if (pMem[it->first] != memPollPreviousValues[it->first])    // memory value changed since last poll
        {
            memPollPreviousValues[it->first] = pMem[it->first];
            (this->*(it->second))(it->first);
        }
    }
}

void MemoryTriggers::PollMapSetCurrentValues()
{
    LPBYTE pMem = MemGetMainPtr(0);
    for (auto it = memPollMap.begin(); it != memPollMap.end(); ++it)
    {
        memPollPreviousValues[it->first] = pMem[it->first];
    }
}

void MemoryTriggers::PollChanged_InGameMap(UINT memLoc)
{
    if (MemGetMainPtr(memLoc)[0] == 0xFCE0)     // User just got in game map
        DelayedTriggerInsert(DelayedTriggersFunction::PARSE_TILES, 5000);
    OutputDebugString((std::to_wstring(MemGetMainPtr(memLoc)[0]) + L" In game map!\n").c_str());
}

void MemoryTriggers::PollChanged_MapID(UINT memLoc)
{
    DelayedTriggerInsert(DelayedTriggersFunction::PARSE_TILES, 4000);
    //OutputDebugString((std::to_wstring(MemGetMainPtr(memLoc)[0]) + L" MapID changed!\n").c_str());
}

void MemoryTriggers::PollChanged_MapType(UINT memLoc)
{
    //OutputDebugString((std::to_wstring(MemGetMainPtr(memLoc)[0]) + L" MapType changed!\n").c_str());
}

void MemoryTriggers::PollChanged_Floor(UINT memLoc)
{
    DelayedTriggerInsert(DelayedTriggersFunction::PARSE_TILES, 4000);
    //OutputDebugString((std::to_wstring(MemGetMainPtr(memLoc)[0]) + L" Floor changed!\n").c_str());
}

void MemoryTriggers::PollChanged_XPos(UINT memLoc)
{
    //OutputDebugString((std::to_wstring(MemGetMainPtr(memLoc)[0]) + L" XPos changed!\n").c_str());
}

void MemoryTriggers::PollChanged_YPos(UINT memLoc)
{
    //OutputDebugString((std::to_wstring(MemGetMainPtr(memLoc)[0]) + L" YPos changed!\n").c_str());
}

void MemoryTriggers::PollChanged_OverlandMapX(UINT memLoc)
{
    DelayedTriggerInsert(DelayedTriggersFunction::PARSE_TILES, 4000);
    //OutputDebugString((std::to_wstring(MemGetMainPtr(memLoc)[0]) + L" OverlandMapX changed!\n").c_str());
}

void MemoryTriggers::PollChanged_OverlandMapY(UINT memLoc)
{
    DelayedTriggerInsert(DelayedTriggersFunction::PARSE_TILES, 4000);
    //OutputDebugString((std::to_wstring(MemGetMainPtr(memLoc)[0]) + L" OverlandMapY changed!\n").c_str());
}
#pragma endregion

#pragma region Triggers
void MemoryTriggers::DelayedTriggerInsert(DelayedTriggersFunction funcId, UINT64 delayInMilliSeconds)
{
    // Insert the function to trigger with the necessary delay, if it doesn't yet exist
    // We don't trigger twice a function
    float nextTriggerS = p_timer->GetTotalSeconds() + delayInMilliSeconds / 1000;
    delayedTriggerMap.try_emplace(funcId, nextTriggerS);
}

void MemoryTriggers::DelayedTriggersProcess()
{
    if (delayedTriggerMap.empty())
        return;
    for (auto it = delayedTriggerMap.begin(); it != delayedTriggerMap.end();)
    {
        float currentS = p_timer->GetTotalSeconds();
        if (it->second < currentS)  // time has lapsed, let's trigger!
        {
            // get the actual function pointer and call the function
            (this->*(delayedTriggerFuncIDs.at(it->first)))(UINT_MAX);
            // erase the entry from the map, we've called it
            delayedTriggerMap.erase(it);
            if (delayedTriggerMap.empty())
                break;
        }
        else
        {
            it++;
        }
    }
}

// Below are the functions that are stored in the delayed trigger map
// Every update there's method that scans the map for functions whose timestamp
// expired and it triggers them
void MemoryTriggers::DelayedTrigger_ParseTiles(UINT memloc)
{
    std::shared_ptr<TilesetCreator>m_tileset = GetTilesetCreator();
    m_tileset->parseTilesInHGR2();
}
#pragma endregion
