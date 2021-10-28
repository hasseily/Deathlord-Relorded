#pragma once
#include <map>
#include "StepTimer.h"
#include "DeathlordHacks.h"

enum class DelayedTriggersFunction
{
	PARSE_TILES,
	UPDATE_XY,
	COUNT
};

class MemoryTriggers	// Singleton
{
public:
	float intervalPollMemory = 0.1f;		// default interval (seconds) between polling memory	
	float intervalDelayedTriggers = 0.1f;	// default interval (seconds) between firing triggers



	void Process();						// The only method that is needed.
										// Processes as necessary, at intervals 
	void PollMapSetCurrentValues();		// Forces an update to the current state of memory
	void PollKeyMemoryLocations();		// Manually call memory polling
	void DelayedTriggersProcess();		// Manually call firing ready triggers

	void ClearAllTriggers() { delayedTriggerMap.clear(); };


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
	using FuncPtr = void (MemoryTriggers::*)(UINT8);
	std::map<UINT32, FuncPtr> memPollMap;                 // memlocation -> function
	std::map<DelayedTriggersFunction, FuncPtr> delayedTriggerFuncIDs;
	std::map<DelayedTriggersFunction, float> delayedTriggerMap;    // functionID -> ElapsedSeconds_when_trigger
	std::map<UINT32, UINT8>memPollPreviousValues;

	// Memory polling
	void PollChanged_StartedTransition(UINT8 oldVal);
	void PollChanged_InGameMap(UINT8 oldVal);
	void PollChanged_MapID(UINT8 oldVal);
	void PollChanged_MapType(UINT8 oldVal);
	void PollChanged_Floor(UINT8 oldVal);
	void PollChanged_XPos(UINT8 oldVal);
	void PollChanged_YPos(UINT8 oldVal);
	void PollChanged_OverlandMapX(UINT8 oldVal);
	void PollChanged_OverlandMapY(UINT8 oldVal);
	void DelayedTriggerInsert(DelayedTriggersFunction funcId, UINT64 delayInMilliSeconds);
	// Poll Delayed Trigger Functions
	void DelayedTrigger_ParseTiles(UINT8 memloc);
	void DelayedTrigger_UpdateAvatarPositionOnAutomap(UINT8 memloc);

	float timeSinceMemoryPolled = 0;
	float timeSinceTriggersFired = 0;

	MemoryTriggers(const DX::StepTimer* ptimer)
	{
		p_timer = ptimer;

		memPollMap[MAP_TRANSITION_BEGIN] = &MemoryTriggers::PollChanged_StartedTransition;
		//memPollMap[(UINT32)MAP_TRANSITION_BEGIN+1] = &MemoryTriggers::PollChanged_StartedTransition;
		//memPollMap[(UINT32)MAP_TRANSITION_BEGIN+2] = &MemoryTriggers::PollChanged_StartedTransition;
		//memPollMap[(UINT32)MAP_TRANSITION_BEGIN+3] = &MemoryTriggers::PollChanged_StartedTransition;
		//memPollMap[(UINT32)MAP_TRANSITION_BEGIN+4] = &MemoryTriggers::PollChanged_StartedTransition;
		//memPollMap[(UINT32)MAP_TRANSITION_BEGIN+5] = &MemoryTriggers::PollChanged_StartedTransition;
		//memPollMap[(UINT32)MAP_TRANSITION_BEGIN+6] = &MemoryTriggers::PollChanged_StartedTransition;
		//memPollMap[(UINT32)MAP_TRANSITION_END] = &MemoryTriggers::PollChanged_StartedTransition;
		memPollMap[MAP_IS_OVERLAND] = &MemoryTriggers::PollChanged_MapType;
		memPollMap[MAP_ID] = &MemoryTriggers::PollChanged_MapID;
		memPollMap[MAP_LEVEL] = &MemoryTriggers::PollChanged_Floor;
		memPollMap[MAP_OVERLAND_X] = &MemoryTriggers::PollChanged_OverlandMapX;
		memPollMap[MAP_OVERLAND_Y] = &MemoryTriggers::PollChanged_OverlandMapY;
		memPollMap[MAP_XPOS] = &MemoryTriggers::PollChanged_XPos;
		memPollMap[MAP_YPOS] = &MemoryTriggers::PollChanged_YPos;

		delayedTriggerFuncIDs[DelayedTriggersFunction::PARSE_TILES] = &MemoryTriggers::DelayedTrigger_ParseTiles;
		delayedTriggerFuncIDs[DelayedTriggersFunction::UPDATE_XY] = &MemoryTriggers::DelayedTrigger_UpdateAvatarPositionOnAutomap;
	}
};

