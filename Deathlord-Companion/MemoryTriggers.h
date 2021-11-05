#pragma once
#include <map>
#include "StepTimer.h"
#include "DeathlordHacks.h"

enum class DelayedTriggersFunction
{
	PARSE_TILES,
	VISIBLE_TILES,
	FINISH_TRANSITION,
	COUNT
};

class MemoryTriggers	// Singleton
{
public:
	float intervalPollMemory = 0.1f;		// default interval (seconds) between polling memory	
	float intervalDelayedTriggers = 0.1f;	// default interval (seconds) between firing triggers



	void Process();						// Processes triggers as necessary 
	void PollMapSetCurrentValues();		// Forces an update to the current state of memory
	void PollKeyMemoryLocations();		// Manually call memory polling
	void DelayedTriggersProcess(bool force=false);		// Manually call firing ready triggers
	void DelayedTriggerInsert(DelayedTriggersFunction funcId, UINT64 delayInMilliSeconds);
	void ClearAllTriggers() { delayedTriggerMap.clear(); };

	// Poll Delayed Trigger Functions
	bool DelayedTrigger_ParseTiles();
	bool DelayedTrigger_FinishMapTransitionState();
	bool DelayedTrigger_AnalyzeVisibleTiles();

	// public singleton code
	static MemoryTriggers* GetInstance(const DX::StepTimer* timer) {
		if (NULL == s_instance)
			s_instance = new MemoryTriggers(timer);
		return s_instance;
	}
	static MemoryTriggers* GetInstance() {
		return s_instance;
	}
	~MemoryTriggers() {
	}
private:
	const DX::StepTimer* p_timer;
	static MemoryTriggers* s_instance;

	// Memory polling map
	using FuncPtrPoll = void (MemoryTriggers::*)(UINT8);
	using FuncPtrTrigger = bool (MemoryTriggers::*)();
	std::map<UINT32, FuncPtrPoll> memPollMap;                 // memlocation -> function
	std::map<DelayedTriggersFunction, FuncPtrTrigger> delayedTriggerFuncIDs;
	std::map<DelayedTriggersFunction, float> delayedTriggerMap;    // functionID -> ElapsedSeconds_when_trigger
	std::map<UINT32, UINT8>memPollPreviousValues;

	// Memory polling
	// PollChanged_StartedTransition is in the public methods because it might want to
	// be checked directly by some other process.
	void PollChanged_InGameMap(UINT8 oldVal);
	void PollChanged_MapID(UINT8 oldVal);
	void PollChanged_MapType(UINT8 oldVal);
	void PollChanged_Floor(UINT8 oldVal);
	void PollChanged_XPos(UINT8 oldVal);
	void PollChanged_YPos(UINT8 oldVal);
	void PollChanged_OverlandMapX(UINT8 oldVal);
	void PollChanged_OverlandMapY(UINT8 oldVal);


	float timeSinceMemoryPolled = 0;
	float timeSinceTriggersFired = 0;

	MemoryTriggers(const DX::StepTimer* ptimer)
	{
		p_timer = ptimer;

		memPollMap[MAP_TYPE] = &MemoryTriggers::PollChanged_MapType;
		memPollMap[MAP_FLOOR] = &MemoryTriggers::PollChanged_Floor;
		memPollMap[MAP_ID] = &MemoryTriggers::PollChanged_MapID;
		memPollMap[MAP_OVERLAND_X] = &MemoryTriggers::PollChanged_OverlandMapX;
		memPollMap[MAP_OVERLAND_Y] = &MemoryTriggers::PollChanged_OverlandMapY;
		// Unused
		//memPollMap[MAP_XPOS] = &MemoryTriggers::PollChanged_XPos;
		//memPollMap[MAP_YPOS] = &MemoryTriggers::PollChanged_YPos;

		delayedTriggerFuncIDs[DelayedTriggersFunction::PARSE_TILES] = &MemoryTriggers::DelayedTrigger_ParseTiles;
		delayedTriggerFuncIDs[DelayedTriggersFunction::VISIBLE_TILES] = &MemoryTriggers::DelayedTrigger_AnalyzeVisibleTiles;
		delayedTriggerFuncIDs[DelayedTriggersFunction::FINISH_TRANSITION] = &MemoryTriggers::DelayedTrigger_FinishMapTransitionState;
	}
};

