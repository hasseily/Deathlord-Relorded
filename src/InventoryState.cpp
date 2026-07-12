#include "InventoryState.h"

#include "DlrlData.h"
#include "Emulator/StdAfx.h"
#include "Emulator/Memory.h"

#include <algorithm>

namespace dlrl
{
namespace
{
constexpr int PartySize = 6;

BYTE* PartyItem(int member, int slot)
{
    return MemGetMainPtr(static_cast<WORD>(
        deathlord::PartyInventory + member * 0x20 + slot));
}

bool ValidSlot(int slot) { return slot >= 0 && slot < 8; }
bool ValidMember(int member) { return member >= 0 && member < PartySize; }
} // namespace

const InventoryState::StoredItem& InventoryState::Stashed(int slot, int index) const
{
    static const StoredItem empty{};
    if (!ValidSlot(slot) || index < 0 || index >= 2) return empty;
    return stash_[slot][index];
}

int InventoryState::StashCount(int slot) const
{
    if (!ValidSlot(slot)) return 0;
    return static_cast<int>(std::count_if(stash_[slot].begin(), stash_[slot].end(),
        [](const StoredItem& item) { return item.item != 0xFF; }));
}

bool InventoryState::MoveToParty(int slot, int sourceOwner, int targetMember)
{
    if (!ValidSlot(slot) || !ValidMember(targetMember) || sourceOwner == targetMember)
        return false;
    BYTE* target = PartyItem(targetMember, slot);
    if (ValidMember(sourceOwner))
    {
        BYTE* source = PartyItem(sourceOwner, slot);
        std::swap(source[0], target[0]);
        std::swap(source[8], target[8]);
        return true;
    }
    const int stashIndex = sourceOwner - PartySize;
    if (stashIndex < 0 || stashIndex >= 2 || stash_[slot][stashIndex].item == 0xFF)
        return false;
    std::swap(stash_[slot][stashIndex].item, target[0]);
    std::swap(stash_[slot][stashIndex].charges, target[8]);
    return true;
}

bool InventoryState::MovePartyToStash(int slot, int sourceMember)
{
    if (!ValidSlot(slot) || !ValidMember(sourceMember)) return false;
    auto empty = std::find_if(stash_[slot].begin(), stash_[slot].end(),
        [](const StoredItem& item) { return item.item == 0xFF; });
    if (empty == stash_[slot].end()) return false;
    BYTE* source = PartyItem(sourceMember, slot);
    if (source[0] == 0xFF) return false;
    empty->item = source[0];
    empty->charges = source[8];
    source[0] = source[8] = 0xFF;
    return true;
}

bool InventoryState::DeleteStashed(int slot, int stashIndex)
{
    if (!ValidSlot(slot) || stashIndex < 0 || stashIndex >= 2
        || stash_[slot][stashIndex].item == 0xFF)
        return false;
    stash_[slot][stashIndex] = {};
    return true;
}

} // namespace dlrl
