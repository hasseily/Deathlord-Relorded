#include "pch.h"
#include "InvItem.h"

InvItem::InvItem()
{
	Initialize();
}

InvItem::InvItem(UINT8 _id, UINT8 _slot, UINT16 _equipMask, UINT16 _classMask, UINT16 _raceMask,
	std::string _name, INT8 _thaco, UINT8 _numAttacks,
	UINT8 _damageMin, UINT8 _damageMax, INT8 _ac, std::string _special)
{
	Initialize(_id, _slot, _equipMask, _classMask, _raceMask,
		_name, _thaco, _numAttacks,
		_damageMin, _damageMax,  _ac,  _special);
}


void InvItem::copyPropertiesFromItem(InvItem* item)
{
	if (!item)
		return;
	id = item->id;
	slot = item->slot;
	equipMask = item->equipMask;
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

void InvItem::Initialize()
{
	id = 0;
	slot = 0;
	equipMask = 0;
	classMask = 0;
	raceMask = 0;
	name = std::string();
	thaco = 0;
	numAttacks = 0;
	damageMin = 0;
	damageMax = 0;
	ac = 0;
	special = std::string();
}

void InvItem::Initialize(UINT8 _id, UINT8 _slot, UINT16 _equipMask, UINT16 _classMask, UINT16 _raceMask,
	std::string _name, INT8 _thaco, UINT8 _numAttacks,
	UINT8 _damageMin, UINT8 _damageMax, INT8 _ac, std::string _special)
{
	id = _id;
	slot = _slot;
	equipMask = _equipMask;
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
