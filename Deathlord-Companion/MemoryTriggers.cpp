#include "pch.h"
#include "MemoryTriggers.h"
#include "TilesetCreator.h"
#include "DeathlordHacks.h"
#include "AutoMap.h"
#include "Game.h"

// below because "The declaration of a static data member in its class definition is not a definition"
MemoryTriggers* MemoryTriggers::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();

void MemoryTriggers::Process()
{
    if (!g_isInGameMap)
        return;
    // Process the the delayed triggers during the player wait loop timer. The player is already
    // in a map, since this Process() method only runs on the timer in game that wants to 
    // pass a turn. So the player is already in a "safe" state and the tile parsing can happen
	DelayedTriggersProcess();
    return;

    // DISABLED INTERVAL PROCESSING
    // Now it's done during the game's turn pass timer routine (See Fetch() in CPU.cpp)
    
    /*
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
    */
}


#pragma region Polling
//************************************
// Method:    PollKeyMemoryLocations
// FullName:  MemoryTriggers::PollKeyMemoryLocations
// Access:    public 
// Returns:   void
// Qualifier:

// Description: 
// Called on the game's main Update() loop
// Polls for key memory location changes and fires relevant methods
//************************************
void MemoryTriggers::PollKeyMemoryLocations()
{
    LPBYTE pMem = MemGetMainPtr(0);
    for (auto it = memPollMap.begin(); it != memPollMap.end(); ++it)
    {
        if (pMem[it->first] != memPollPreviousValues[it->first])    // memory value changed since last poll
        {
			(this->*(it->second))(memPollPreviousValues[it->first]);
            memPollPreviousValues[it->first] = pMem[it->first];
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

void MemoryTriggers::PollChanged_InGameMap(UINT8 oldVal)
{
    if (MemGetMainPtr(MAP_IS_IN_GAME_MAP)[0] == 0xE5)     // User just got in game map
    {
		//OutputDebugString((std::to_wstring(MemGetMainPtr(MAP_IS_IN_GAME_MAP)[0]) + L" In game map!\n").c_str());
		DelayedTriggerInsert(DelayedTriggersFunction::PARSE_TILES, 0);
    }
}

void MemoryTriggers::PollChanged_MapID(UINT8 oldVal)    // unused
{
	//OutputDebugString((std::to_wstring(MemGetMainPtr(MAP_ID)[0]) + L" MapID changed!\n").c_str());
}

void MemoryTriggers::PollChanged_MapType(UINT8 oldVal)  // unused
{
    //OutputDebugString((std::to_wstring(MemGetMainPtr(MAP_IS_OVERLAND)[0]) + L" MapType changed!\n").c_str());
}

void MemoryTriggers::PollChanged_Floor(UINT8 oldVal)
{
	//OutputDebugString((std::to_wstring(MemGetMainPtr(MAP_LEVEL)[0]) + L" Floor changed!\n").c_str());
    // TODO: In dungeons and towers, zoom to the current level (each map is 4 levels)
}

void MemoryTriggers::PollChanged_XPos(UINT8 oldVal) // Unused
{
    //OutputDebugString((std::to_wstring(MemGetMainPtr(MAP_XPOS)[0]) + L" XPos changed from " + std::to_wstring(oldVal) + L"!\n").c_str());
}

void MemoryTriggers::PollChanged_YPos(UINT8 oldVal) // Unused
{
    //OutputDebugString((std::to_wstring(MemGetMainPtr(MAP_YPOS)[0]) + L" YPos changed from " + std::to_wstring(oldVal) + L"!\n").c_str());
}

void MemoryTriggers::PollChanged_OverlandMapX(UINT8 oldVal)
{
	//OutputDebugString((std::to_wstring(MemGetMainPtr(MAP_OVERLAND_X)[0]) + L" OverlandMapX changed!\n").c_str());
	AutoMap* aM = AutoMap::GetInstance();
	aM->SetShowTransition(true);
    DelayedTriggerInsert(DelayedTriggersFunction::PARSE_TILES, 0);
}

void MemoryTriggers::PollChanged_OverlandMapY(UINT8 oldVal)
{
	//OutputDebugString((std::to_wstring(MemGetMainPtr(MAP_OVERLAND_Y)[0]) + L" OverlandMapY changed!\n").c_str());
	AutoMap* aM = AutoMap::GetInstance();
	aM->SetShowTransition(true);
    DelayedTriggerInsert(DelayedTriggersFunction::PARSE_TILES, 0);
}
#pragma endregion

#pragma region Triggers
void MemoryTriggers::DelayedTriggerInsert(DelayedTriggersFunction funcId, UINT64 delayInMilliSeconds)
{
    // Insert the function to trigger with the necessary delay, if it doesn't yet exist
    // If it already exists, change the timer to the requested one if it's shorter
    // We don't trigger twice a function
    float nextTriggerS = p_timer->GetTotalSeconds() + delayInMilliSeconds / 1000;
    auto search = delayedTriggerMap.find(funcId);
    if (search != delayedTriggerMap.end())
    {
        if (search->second > nextTriggerS)
			delayedTriggerMap[funcId] = nextTriggerS;
    }
    else
    {
		delayedTriggerMap.try_emplace(funcId, nextTriggerS);
    }
}

void MemoryTriggers::DelayedTriggersProcess(bool force)
{
    if (delayedTriggerMap.empty())
        return;
    std::vector<DelayedTriggersFunction> keysToDelete;
    bool shouldDelete = false;
    for (auto it = delayedTriggerMap.begin(); it != delayedTriggerMap.end();)
    {
        float currentS = p_timer->GetTotalSeconds();
        if ((it->second <= currentS) || (force == true))  // time has lapsed, let's trigger!
        {
            // get the actual function pointer and call the function
            shouldDelete = (this->*(delayedTriggerFuncIDs.at(it->first)))();
            // erase the entry from the map, if it agrees to :)
            if (shouldDelete)
                keysToDelete.push_back(it->first);
        }
		it++;
    }
    for each (auto keyToDel in keysToDelete)
    {
        delayedTriggerMap.erase(keyToDel);
    }
}

// Below are the functions that are stored in the delayed trigger map
// Every update there's a method that scans the map for functions whose timestamp
// expired and it triggers them
// Such functions should generally return true to be removed from the processing list
bool MemoryTriggers::DelayedTrigger_ParseTiles()
{
	auto tileset = TilesetCreator::GetInstance();
    tileset->parseTilesInHGR2();
    if (g_isInGameMap)
    {
		auto aM = AutoMap::GetInstance();
        if (aM != NULL)
            aM->CreateNewTileSpriteMap();
        aM->ForceRedrawMapArea();
        // Delay the transition finish so that the XY is updated to the correct map
        // In some cases it updates before the MapId has had a chance to update itself
        DelayedTriggerInsert(DelayedTriggersFunction::FINISH_TRANSITION, 50);
    }
    return true;
}

// This is actually set up by the Automap to get it to analyze visible tiles
// when it is safe to do so
bool MemoryTriggers::DelayedTrigger_AnalyzeVisibleTiles()
{
    if (g_isInGameMap)
    {
		auto aM = AutoMap::GetInstance();
        if (aM != NULL)
            aM->AnalyzeVisibleTiles();
    }
    return true;
}

bool MemoryTriggers::DelayedTrigger_FinishMapTransitionState()
{
	AutoMap* aM = AutoMap::GetInstance();
    aM->SetShowTransition(false);
    return true;
}

#pragma endregion
