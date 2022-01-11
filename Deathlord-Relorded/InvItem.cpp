#include "pch.h"
#include "InvItem.h"

InvItem::InvItem()
{
	Initialize();
}

InvItem::InvItem(UINT8 _id, InventorySlots _slot, UINT16 _classMask, UINT16 _raceMask,
	std::wstring _name, INT8 _thaco, UINT8 _numAttacks,
	UINT8 _damageMin, UINT8 _damageMax, INT8 _ac, std::wstring _special)
{
	Initialize(_id, _slot, _classMask, _raceMask,
		_name, _thaco, _numAttacks,
		_damageMin, _damageMax,  _ac,  _special);
}


void InvItem::copyPropertiesFromItem(InvItem* item)
{
	if (!item)
		return;
	id = item->id;
	slot = item->slot;
	classMask = item->classMask;
	raceMask = item->raceMask;
	name = item->name;
	thaco = item->thaco;
	numAttacks = item->numAttacks;
	damageMin = item->damageMin;
	damageMax = item->damageMax;
	ac = item->ac;
	special = item->special;
}

bool InvItem::canEquip(DeathlordClasses dlClass, DeathlordRaces dlRace)
{
	int classOK = (1 << (UINT8)dlClass) & classMask;
	int raceOK = (1 << (UINT8)dlRace) & raceMask;
	return (classOK && raceOK);
}

void InvItem::Initialize()
{
	id = EMPTY_ITEM_ID;
	slot = InventorySlots::Melee;
	classMask = 0;
	raceMask = 0;
	name = std::wstring();
	thaco = 0;
	numAttacks = 0;
	damageMin = 0;
	damageMax = 0;
	ac = 0;
	special = std::wstring();
}

void InvItem::Initialize(UINT8 _id, InventorySlots _slot, UINT16 _classMask, UINT16 _raceMask,
	std::wstring _name, INT8 _thaco, UINT8 _numAttacks,
	UINT8 _damageMin, UINT8 _damageMax, INT8 _ac, std::wstring _special)
{
	id = _id;
	slot = _slot;
	classMask = _classMask;
	raceMask = _raceMask;
	name = _name;
	thaco = _thaco;
	numAttacks = _numAttacks;
	damageMin = _damageMin;
	damageMax = _damageMax;
	ac = _ac;
	special = _special;

	count = 0;
	charges = std::vector<UINT8>();
}
