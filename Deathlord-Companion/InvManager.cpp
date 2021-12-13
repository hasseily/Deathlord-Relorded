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

constexpr UINT8 DEATHLORD_PARTY_SIZE = 6;
constexpr UINT8 DEATHLORD_INVENTORY_SLOTS = 8;
constexpr UINT8 EMPTY_ITEM_ID = 0xFF;
constexpr UINT8 ITEM_CHARGES_OFFSET = 0x10;		// TODO: VERIFY!!! AND VERIFY CHARGES = 0 BY DEFAULT

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
	classMask,
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
	ifstream ifs(currentDir);
	
	vector<string> headerFields;
	vector<string> fields;
	string s;
	int lineIdx = 0;
	for (std::string line; std::getline(ifs, line); )
	{
		stringstream ss(line);
		while (getline(ss, s, ',')) {
			if (lineIdx == 0)
				headerFields.push_back(s);
			else
				fields.push_back(s);
		}
		if (lineIdx == 0)	// header, do nothing
			continue;
		bool hasError = false;
		if (fields.size() != (int)InventoryHeaders::COUNT)
		{
			hasError = true;
			continue;
		}
		InvItem item;
		for (size_t i = 0; i < fields.size(); i++)
		{
			switch ((InventoryHeaders)i)
			{
			case InventoryHeaders::id:
				try { item.id = stoi(fields.at(i)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::slot:
				try { item.slot = stoi(fields.at(i)); }
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
				try { item.equipMask& (stoi(fields.at(i)) << (i - 2)); }
				catch (const std::exception&) { hasError = true; }
				break;
			case InventoryHeaders::classMask:
				try { item.classMask = stoi(fields.at(i)); }
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
			char _buf[100];
			sprintf_s(_buf, 100, "Error loading line %.04d: %s\n", lineIdx, line.c_str());
		}
		else {
			itemList[item.id] = item;
		}
		lineIdx++;
	}
	// Finally, fill the stash with empty items with 0 charges
	for (size_t i = 0; i < (STASH_MAX_ITEMS_PER_SLOT * DEATHLORD_INVENTORY_SLOTS); i++)
	{
		std::vector<UINT8, UINT8> emptyItem = { EMPTY_ITEM_ID, 0 };
		stash.push_back(emptyItem);
	}
}

#pragma region Methods

void InvManager::deleteItem(UINT8 slot, UINT8 stashPosition)
{
	std::vector<UINT8, UINT8> theItem = stash.at(slot + stashPosition);
	theItem[0] = EMPTY_ITEM_ID;
	theItem[1] = 0;
}

// stash position is the position within the possible stash items for this inventory slot
void InvManager::swapStashWithPartyMember(UINT8 stashPosition, UINT8 memberPosition, UINT8 memberSlot)
{
	if (stashPosition >= stash.size())
		return;
	if (memberPosition >= DEATHLORD_PARTY_SIZE)
		return;
	if (memberSlot >= DEATHLORD_INVENTORY_SLOTS)
		return;

	std::vector<UINT8,UINT8> theItem = stash.at(memberSlot + stashPosition);
	UINT8 otherItemId;
	UINT8 otherItemCharges;
	// Members' inventory takes up 0x20 in memory
	UINT16 memberMemSlot = PARTY_INVENTORY_START + memberPosition * 0x20 + memberSlot;

	// Swap items
	otherItemId = MemGetMainPtr(memberMemSlot)[0];
	otherItemCharges = MemGetMainPtr(memberMemSlot)[ITEM_CHARGES_OFFSET];
	MemGetMainPtr(memberMemSlot)[0] = theItem[0];
	MemGetMainPtr(memberMemSlot)[ITEM_CHARGES_OFFSET] = theItem[1];
	theItem[0] = otherItemId;
	theItem[1] = otherItemCharges;
}

void InvManager::exchangeBetweeenPartyMembers(UINT8 m1Position, UINT8 m1Slot, UINT8 m2Position, UINT8 m2Slot)
{
	if (m1Position >= DEATHLORD_PARTY_SIZE)
		return;
	if (m1Slot >= DEATHLORD_INVENTORY_SLOTS)
		return;
	if (m2Position >= DEATHLORD_PARTY_SIZE)
		return;
	if (m2Slot >= DEATHLORD_INVENTORY_SLOTS)
		return;
	UINT16 m1MemSlot = PARTY_INVENTORY_START + m1Position * 0x20 + m1Slot;
	UINT16 m2MemSlot = PARTY_INVENTORY_START + m2Position * 0x20 + m2Slot;

	UINT8 tmpItemId = MemGetMainPtr(m1MemSlot)[0];
	UINT8 tmpItemCharges = MemGetMainPtr(m1MemSlot)[ITEM_CHARGES_OFFSET];
	MemGetMainPtr(m1MemSlot)[0] = MemGetMainPtr(m2MemSlot)[0];
	MemGetMainPtr(m1MemSlot)[ITEM_CHARGES_OFFSET] = MemGetMainPtr(m2MemSlot)[ITEM_CHARGES_OFFSET];
	MemGetMainPtr(m2MemSlot)[0] = tmpItemId;
	MemGetMainPtr(m2MemSlot)[ITEM_CHARGES_OFFSET] = tmpItemCharges;
}

#pragma endregion