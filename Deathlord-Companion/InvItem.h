#pragma once
#include <vector>
#include <string>

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

class InvItem
{
public:
	// Properties
	UINT8 id;
	InventorySlots slot;
	UINT16 classMask;
	UINT16 raceMask;
	std::string name;
	INT8 thaco;
	UINT8 numAttacks;
	UINT8 damageMin;
	UINT8 damageMax;
	INT8 ac;
	std::string special;

	InvItem();
	InvItem(UINT8 _id, InventorySlots _slot, UINT16 _classMask, UINT16 _raceMask,
		std::string _name, INT8 _thaco, UINT8 _numAttacks,
		UINT8 _damageMin, UINT8 _damageMax, INT8 _ac, std::string _special);
	void copyPropertiesFromItem(InvItem* item);

private:
	UINT8 count;
	std::vector<UINT8> charges;

	void Initialize();
	void Initialize(UINT8 _id, InventorySlots _slot, UINT16 _classMask, UINT16 _raceMask,
		std::string _name, INT8 _thaco, UINT8 _numAttacks, 
		UINT8 _damageMin, UINT8 _damageMax, INT8 _ac, std::string _special);
};

