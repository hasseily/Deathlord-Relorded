#pragma once
#include "InvItem.h"

// The inventory manager creates a stash that will have up to x items per slot
// It should allow the player to move items between party members or the stash
// It should also allow the player to discard any item in the stash only
// It should also allow the player to change readied weaponry for party members

constexpr UINT8 STASH_MAX_ITEMS_PER_SLOT = 2;
constexpr UINT8 DEATHLORD_PARTY_SIZE = 6;
constexpr UINT8 DEATHLORD_INVENTORY_SLOTS = 8;
constexpr UINT8 EMPTY_CHARGES_COUNT = 0xFF;		// Charges == 0x00 means infinite charges

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
	UINT8 StashSlotCount(InventorySlots slot);
	UINT8 MaxItemsPerSlot();
	std::vector<InvInstance> AllInventoryInSlot(InventorySlots slot);
	void DeleteItem(InventorySlots slot, UINT8 stashPosition);
	bool InvManager::PutInStash(UINT8 memberPosition, InventorySlots memberSlot);
	void SwapStashWithPartyMember(UINT8 stashPosition, UINT8 memberPosition, InventorySlots memberSlot);
	void ExchangeBetweeenPartyMembers(UINT8 m1Position, UINT8 m2Position, InventorySlots mSlot);
	InvItem* ItemWithId(UINT8 itemId);

	// properties

private:
	static InvManager* s_instance;
	bool bIsDisplayed;

	InvManager()
	{
		Initialize();
	}

	void Initialize();
	std::vector<std::pair<UINT8, UINT8>> stash;	// id, charges
	std::map<INT8, InvItem> itemList;
};

