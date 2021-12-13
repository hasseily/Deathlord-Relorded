#pragma once
#include "InvItem.h"

// The inventory manager creates a stash that will have up to x items per slot
// It should allow the player to move items between party members or the stash
// It should also allow the player to discard any item in the stash only
// It should also allow the player to change readied weaponry for party members

constexpr UINT8 STASH_MAX_ITEMS_PER_SLOT = 2;

class InvManager	// Singleton
{
public:
	// public singleton code
	static InvManager* GetInstance()
	{
		if (nullptr == s_instance)
			s_instance = new InvManager();
		return s_instance;
	}
	~InvManager()
	{
	}

	// methods
	void deleteItem(UINT8 slot, UINT8 stashPosition);
	void swapStashWithPartyMember(UINT8 stashPosition, UINT8 memberPosition, UINT8 memberSlot);
	void exchangeBetweeenPartyMembers(UINT8 m1Position, UINT8 m1Slot, UINT8 m2Position, UINT8 m2Slot);
	const InvItem itemWithId(UINT8 itemId);

	// properties

private:
	static InvManager* s_instance;
	bool bIsDisplayed;

	InvManager()
	{
		Initialize();
	}

	void Initialize();
	std::vector<std::pair<UINT8,UINT8>> stash;	// id, charges
	std::map<INT8, InvItem> itemList;
};

