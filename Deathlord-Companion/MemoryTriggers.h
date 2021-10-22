#pragma once
#include <map>
#include "StepTimer.h"
#include "DeathlordHacks.h"

enum class DelayedTriggersFunction
{
	PARSE_TILES,
	COUNT
};

class MemoryTriggers	// Singleton
{
public:
	float intervalPollMemory = 0.5f;		// default interval (seconds) between polling memory	
	float intervalDelayedTriggers = 0.1f;	// default interval (seconds) between firing triggers



	void Process();						// The only method that is needed.
										// Processes as necessary, at intervals 
	void PollMapSetCurrentValues();		// Forces an update to the current state of memory
	void PollKeyMemoryLocations();		// Manually call memory polling
	void DelayedTriggersProcess();		// Manually call firing ready triggers




	// public singleton code
	static MemoryTriggers* GetInstance(const DX::StepTimer* timer) {
		if (NULL == s_instance)
			s_instance = new MemoryTriggers(timer);
		return s_instance;
	}
	~MemoryTriggers() {
	}
private:
	const DX::StepTimer* p_timer;
	static MemoryTriggers* s_instance;

	// Memory polling map
	using FuncPtr = void (MemoryTriggers::*)(UINT);
	std::map<UINT, FuncPtr> memPollMap;                 // memlocation -> function
	std::map<DelayedTriggersFunction, FuncPtr> delayedTriggerFuncIDs;
	std::map<DelayedTriggersFunction, float> delayedTriggerMap;    // functionID -> ElapsedSeconds_when_trigger
	std::map<UINT, UINT>memPollPreviousValues;

	// Memory polling
	void PollChanged_InGameMap(UINT memLoc);
	void PollChanged_MapID(UINT memLoc);
	void PollChanged_MapType(UINT memLoc);
	void PollChanged_Floor(UINT memLoc);
	void PollChanged_XPos(UINT memLoc);
	void PollChanged_YPos(UINT memLoc);
	void PollChanged_OverlandMapX(UINT memLoc);
	void PollChanged_OverlandMapY(UINT memLoc);
	void DelayedTriggerInsert(DelayedTriggersFunction funcId, UINT64 delayInMilliSeconds);
	// Poll Delayed Trigger Functions
	void DelayedTrigger_ParseTiles(UINT memloc);

	float timeSinceMemoryPolled = 0;
	float timeSinceTriggersFired = 0;

	MemoryTriggers(const DX::StepTimer* ptimer)
	{
		p_timer = ptimer;

		memPollMap[MAP_IS_IN_GAME_MAP] = &MemoryTriggers::PollChanged_InGameMap;
		memPollMap[MAP_IS_OVERLAND] = &MemoryTriggers::PollChanged_MapType;
		memPollMap[MAP_ID] = &MemoryTriggers::PollChanged_MapID;
		memPollMap[MAP_LEVEL] = &MemoryTriggers::PollChanged_Floor;
		memPollMap[MAP_OVERLAND_X] = &MemoryTriggers::PollChanged_OverlandMapX;
		memPollMap[MAP_OVERLAND_Y] = &MemoryTriggers::PollChanged_OverlandMapY;
		memPollMap[MAP_XPOS] = &MemoryTriggers::PollChanged_XPos;
		memPollMap[MAP_YPOS] = &MemoryTriggers::PollChanged_YPos;

		delayedTriggerFuncIDs[DelayedTriggersFunction::PARSE_TILES] = &MemoryTriggers::DelayedTrigger_ParseTiles;
	}
};

