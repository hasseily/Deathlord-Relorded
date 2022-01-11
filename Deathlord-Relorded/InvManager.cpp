#include "pch.h"
#include "InvManager.h"
#include <filesystem>
#include <sstream>
#include <iostream>
#include <vector>
#include "DeathlordHacks.h"

using namespace std;
namespace fs = std::filesystem;

// below because "The declaration of a static data member in its class definition is not a definition"
InvManager* InvManager::s_instance;

constexpr UINT8 ITEM_CHARGES_OFFSET = 0x08;		// Charges == 0xFF by default


// The comparator to always keep inventory instances static across movement between party members
// Otherwise when we swap equipment the list reorders
static bool compareInvInstance(InvInstance i1, InvInstance i2)
{
	if (i1.item->id == i2.item->id)
		return (i1.charges < i2.charges);
	return (i1.item->id < i2.item->id);
}

enum class InventoryHeaders {
	id = 0,
	slot, 
	bitFig,
	bitPal,
	bitRan,
	bitBar,
	bitBer,
	bitSam,
	bitDar,
	bitThi,
	bitAss,
	bitNin,
	bitMon,
	bitPri,
	bitDru,
	bitWiz,
	bitIll,
	bitPea,
	raceMask,
	name,
	thaco,
	numAttacks,
	damageMin,
	damageMax,
	ac,
	special,
	COUNT
};

void InvManager::Initialize()
{
	stash.clear();
	itemList.clear();

	// Import cvs file with all the item data
	fs::path currentDir = fs::current_path();
	currentDir += "\\Assets\\InventoryList.csv";
	wifstream ifs(currentDir);
	
	vector<wstring> headerFields;
	vector<wstring> fields;
	wchar_t _buf[200];
	int lineIdx = 0;
	for (std::wstring line; std::getline(ifs, line); )
	{
		fields.clear();
		wstringstream ss(line);
		while (ss.getline(_buf, 30, ',')) {
			if (lineIdx == 0)
				headerFields.push_back(wstring(_buf));
			else
				fields.push_back(wstring(_buf));
		}
		if (lineIdx == 0)	// header, do nothing
		{
			lineIdx++;
			continue;
		}
		bool hasError = false;
		/*
		if (fields.size() < ((int)InventoryHeaders::COUNT - 1))		// the last "special" field can be empty
		{
			hasError = true;
			lineIdx++;
			continue;
		}
		*/
		InvItem item;
		for (size_t i = 0; i < fields.size(); i++)
		{
			switch ((InventoryHeaders)i)
			{
			case InventoryHeaders::id:
				try { item.id = stoi(fields.at(i), 0, 16); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::slot:
				try { item.slot = (InventorySlots)stoi(fields.at(i)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::bitFig:
			case InventoryHeaders::bitPal:
			case InventoryHeaders::bitRan:
			case InventoryHeaders::bitBar:
			case InventoryHeaders::bitBer:
			case InventoryHeaders::bitSam:
			case InventoryHeaders::bitDar:
			case InventoryHeaders::bitThi:
			case InventoryHeaders::bitAss:
			case InventoryHeaders::bitNin:
			case InventoryHeaders::bitMon:
			case InventoryHeaders::bitPri:
			case InventoryHeaders::bitDru:
			case InventoryHeaders::bitWiz:
			case InventoryHeaders::bitIll:
			case InventoryHeaders::bitPea:
				try { item.classMask = item.classMask | (stoi(fields.at(i)) << (i - 2)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::raceMask:
				try { item.raceMask = stoi(fields.at(i)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::name:
				try { item.name = fields.at(i); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::thaco:
				try { item.thaco = stoi(fields.at(i)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::numAttacks:
				try { item.numAttacks = stoi(fields.at(i)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::damageMin:
				try { item.damageMin = stoi(fields.at(i)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::damageMax:
				try { item.damageMax = stoi(fields.at(i)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::ac:
				try { item.ac = stoi(fields.at(i)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::special:
				try { item.special = fields.at(i); }
				catch (const std::exception&) { hasError = true; }
				break;
			default:
				break;
			}
		}
		if (hasError)
		{
			wchar_t _buf[100];
			swprintf_s(_buf, 100, L"Error loading line %.04d: %s\n", lineIdx, line.c_str());
			MessageBox(HWND_TOP,
				_buf,
				L"Inventory Data Parser Error",
				MB_ICONEXCLAMATION | MB_SETFOREGROUND);
		}
		else {
			itemList[item.id] = item;
		}
		lineIdx++;
	}
	// Finally, fill the stash with empty items with 0 charges
	for (size_t i = 0; i < (STASH_MAX_ITEMS_PER_SLOT * DEATHLORD_INVENTORY_SLOTS); i++)
	{
		std::pair<UINT8, UINT8> emptyItem = { EMPTY_ITEM_ID, EMPTY_CHARGES_COUNT };
		stash.push_back(emptyItem);
	}
	// stash.at(0).first = 0x29;	// test
	// stash.at(1).first = 0x33;	// test
}


#pragma region Methods
#pragma warning(push)
#pragma warning(disable : 26451)

UINT8 InvManager::MaxItemsPerSlot()
{
	return STASH_MAX_ITEMS_PER_SLOT;
}

UINT8 InvManager::StashSlotCount(InventorySlots slot)
{
	// Count the number of non-empty items in the proper slot
	UINT8 inUse = 0;
	for (size_t i = 0; i < STASH_MAX_ITEMS_PER_SLOT; i++)
	{
		std::pair<UINT8, UINT8> theItem = stash.at(STASH_MAX_ITEMS_PER_SLOT * (UINT8)slot + i);
		if (theItem.first != EMPTY_ITEM_ID)
			inUse++;
	}
	return inUse;
}

void InvManager::DeleteItem(InventorySlots slot, UINT8 stashPosition)
{
	if (stashPosition >= STASH_MAX_ITEMS_PER_SLOT)
		return;
	int stashPos = STASH_MAX_ITEMS_PER_SLOT * (UINT8)slot + stashPosition;
	stash.at(stashPos).first = EMPTY_ITEM_ID;
	stash.at(stashPos).second = EMPTY_CHARGES_COUNT;
}

// Put in the stash in any available empty spot. Returns false if stash is full
bool InvManager::PutInStash(UINT8 memberPosition, InventorySlots memberSlot)
{
	for (size_t i = 0; i < STASH_MAX_ITEMS_PER_SLOT; i++)
	{
		if (stash.at((STASH_MAX_ITEMS_PER_SLOT * (UINT8)memberSlot) + i).first == EMPTY_ITEM_ID)
		{
			SwapStashWithPartyMember(i, memberPosition, memberSlot);
			return true;
		}
	}
	return false;
}

// stash position is the position within the possible stash items for this inventory slot
void InvManager::SwapStashWithPartyMember(UINT8 stashPosition, UINT8 memberPosition, InventorySlots memberSlot)
{
	if (stashPosition >= STASH_MAX_ITEMS_PER_SLOT)
		return;
	if (memberPosition >= DEATHLORD_PARTY_SIZE)
		return;
	if ((UINT8)memberSlot >= DEATHLORD_INVENTORY_SLOTS)
		return;

	std::pair<UINT8,UINT8>* theItem = &stash.at(STASH_MAX_ITEMS_PER_SLOT* (UINT8)memberSlot + stashPosition);
	UINT8 otherItemId;
	UINT8 otherItemCharges;
	// Members' inventory takes up 0x20 in memory
	UINT16 memberMemSlot = PARTY_INVENTORY_START + memberPosition * 0x20 + (UINT8)memberSlot;

	// Swap items
	otherItemId = MemGetMainPtr(memberMemSlot)[0];
	otherItemCharges = MemGetMainPtr(memberMemSlot)[ITEM_CHARGES_OFFSET];
	MemGetMainPtr(memberMemSlot)[0] = theItem->first;
	MemGetMainPtr(memberMemSlot)[ITEM_CHARGES_OFFSET] = theItem->second;
	theItem->first = otherItemId;
	theItem->second = otherItemCharges;
}

void InvManager::ExchangeBetweeenPartyMembers(UINT8 m1Position, UINT8 m2Position, InventorySlots mSlot)
{
	if (m1Position >= DEATHLORD_PARTY_SIZE)
		return;
	if (m2Position >= DEATHLORD_PARTY_SIZE)
		return;
	if ((UINT8)mSlot >= DEATHLORD_INVENTORY_SLOTS)
		return;
	UINT16 m1MemSlot = PARTY_INVENTORY_START + m1Position * 0x20 + (UINT8)mSlot;
	UINT16 m2MemSlot = PARTY_INVENTORY_START + m2Position * 0x20 + (UINT8)mSlot;

	UINT8 tmpItemId = MemGetMainPtr(m1MemSlot)[0];
	UINT8 tmpItemCharges = MemGetMainPtr(m1MemSlot)[ITEM_CHARGES_OFFSET];
	MemGetMainPtr(m1MemSlot)[0] = MemGetMainPtr(m2MemSlot)[0];
	MemGetMainPtr(m1MemSlot)[ITEM_CHARGES_OFFSET] = MemGetMainPtr(m2MemSlot)[ITEM_CHARGES_OFFSET];
	MemGetMainPtr(m2MemSlot)[0] = tmpItemId;
	MemGetMainPtr(m2MemSlot)[ITEM_CHARGES_OFFSET] = tmpItemCharges;
}

InvItem* InvManager::ItemWithId(UINT8 itemId)
{
	if (itemList.find(itemId) != itemList.end())
		return &itemList[itemId];
	return nullptr;
}

std::vector<InvInstance> InvManager::AllInventoryInSlot(InventorySlots slot)
{
	std::vector<InvInstance> _currentInventory;
	int extraId = 0;	// extraidentifier to properly sort items with the same id
	// Get the party inventory for this slot
	for (UINT8 i = 0; i < (DEATHLORD_PARTY_SIZE); i++)
	{
		UINT16 idMemSlot = PARTY_INVENTORY_START + i * 0x20 + (UINT8)slot;
		UINT8 itemId = MemGetMainPtr(idMemSlot)[0];
		if (itemId == EMPTY_ITEM_ID)
			continue;
		InvInstance _inst;
		_inst.extraIdentifier = extraId;
		_inst.item = &itemList[MemGetMainPtr(idMemSlot)[0]];
		_inst.charges = MemGetMainPtr(idMemSlot)[ITEM_CHARGES_OFFSET];
		_inst.owner = i;
		_inst.equipped = _inst.item->canEquip(
			(DeathlordClasses)MemGetMainPtr(PARTY_CLASS_START)[i], 
			(DeathlordRaces)MemGetMainPtr(PARTY_RACE_START)[i]
		);
		_currentInventory.push_back(_inst);
		++extraId;
	}
	// Get the stash inventory
	for (UINT8 i = 0; i < STASH_MAX_ITEMS_PER_SLOT; i++)
	{
		std::pair<UINT8, UINT8>* theItem = &stash.at(STASH_MAX_ITEMS_PER_SLOT * (UINT8)slot + i);
		if (theItem->first == EMPTY_ITEM_ID)
			continue;
		InvInstance _inst;
		_inst.extraIdentifier = extraId;
		_inst.item = &itemList[theItem->first];
		_inst.charges = theItem->second;
		_inst.owner = DEATHLORD_PARTY_SIZE + i;
		_inst.equipped = false;
		_currentInventory.push_back(_inst);
		++extraId;
	}
	// Finally sort and set the row.
	sort(_currentInventory.begin(), _currentInventory.end(), compareInvInstance);
	for (size_t i = 0; i < _currentInventory.size(); i++)
	{
		_currentInventory.at(i).row = i;
	}
	return _currentInventory;
}

// This is used to just get the inventory for a single party member
// PartyLayout.cpp uses it
std::vector<InvInstance> InvManager::AllInventoryForMember(UINT8 member)
{
	std::vector<InvInstance> _currentInventory;
	if (member >= DEATHLORD_PARTY_SIZE)
		return _currentInventory;
	for (UINT8 i = 0; i < (DEATHLORD_INVENTORY_SLOTS); i++)
	{
		UINT16 idMemSlot = PARTY_INVENTORY_START + member * 0x20 + i;
		UINT8 itemId = MemGetMainPtr(idMemSlot)[0];
		InvInstance _inst;
		_inst.extraIdentifier = 0;
		_inst.item = &itemList[MemGetMainPtr(idMemSlot)[0]];
		_inst.charges = MemGetMainPtr(idMemSlot)[ITEM_CHARGES_OFFSET];
		_inst.owner = member;
		_inst.equipped = _inst.item->canEquip(
			(DeathlordClasses)MemGetMainPtr(PARTY_CLASS_START)[member],
			(DeathlordRaces)MemGetMainPtr(PARTY_RACE_START)[member]
		);
		_currentInventory.push_back(_inst);
	}
	// Finally set the row
	// It's equal to the slot #
	for (size_t i = 0; i < _currentInventory.size(); i++)
	{
		_currentInventory.at(i).row = i;
	}
	return _currentInventory;
}

#pragma warning(pop)
#pragma endregion