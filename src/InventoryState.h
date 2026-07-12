#pragma once

#include <array>
#include <cstdint>

namespace dlrl
{

class InventoryState
{
public:
    struct StoredItem
    {
        std::uint8_t item = 0xFF;
        std::uint8_t charges = 0xFF;
    };

    const StoredItem& Stashed(int slot, int index) const;
    int StashCount(int slot) const;
    bool MoveToParty(int slot, int sourceOwner, int targetMember);
    bool MovePartyToStash(int slot, int sourceMember);
    bool DeleteStashed(int slot, int stashIndex);

private:
    std::array<std::array<StoredItem, 2>, 8> stash_{};
};

} // namespace dlrl
