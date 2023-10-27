#pragma once
#include <vector>
#include <string>
#include "DeathlordHacks.h"

enum class InventorySlots
{
	Melee = 0,
	Ranged,
	Chest,
	Shield,
	Misc,
	Jewelry,
	Tool,
	Scroll,
	TOTAL
};

constexpr UINT8 EMPTY_ITEM_ID = 0xFF;

class InvItem
{
public:
	// Properties
	UINT8 id;
	InventorySlots slot;
	UINT16 classMask;
	UINT16 raceMask;
	std::wstring name;
	std::wstring nameEnglish;
	INT8 thaco;
	UINT8 numAttacks;
	UINT8 damageMin;
	UINT8 damageMax;
	INT8 ac;
	std::wstring special;

	InvItem();
	InvItem(UINT8 _id, InventorySlots _slot, UINT16 _classMask, UINT16 _raceMask,
		std::wstring _name, std::wstring _nameEnglish, INT8 _thaco, UINT8 _numAttacks,
		UINT8 _damageMin, UINT8 _damageMax, INT8 _ac, std::wstring _special);
	void copyPropertiesFromItem(InvItem* item);
	bool canEquip(DeathlordClasses dlClass, DeathlordRaces dlRace);

private:
	UINT8 count;
	std::vector<UINT8> charges;

	void Initialize();
	void Initialize(UINT8 _id, InventorySlots _slot, UINT16 _classMask, UINT16 _raceMask,
		std::wstring _name, std::wstring _nameEnglish, INT8 _thaco, UINT8 _numAttacks,
		UINT8 _damageMin, UINT8 _damageMax, INT8 _ac, std::wstring _special);
};


struct InvInstance
{
	int extraIdentifier;	// a unique number within a batch to distinguish the same items from each other
	InvItem* item;
	UINT8 charges;
	UINT8 owner;	// The owner. Anything above DEATHLORD_PARTY_SIZE is the stash
	UINT8 row;		// For use by the overlay if it needs it
	bool equipped;	// If the owner has it equipped (i.e. can use it)
};