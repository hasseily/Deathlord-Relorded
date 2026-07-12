#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace dlrl
{

struct PartySnapshot
{
    static constexpr std::size_t PartyNameLength = 16;
    static constexpr std::size_t PartyDataLength = 0x300;

    std::string partyName;
    std::string exportedAt;
    std::uint8_t partySize = 0;
    std::uint8_t leader = 0;
    std::uint8_t currentCharacter = 0;
    std::array<std::uint8_t, PartyNameLength> partyNameBytes{};
    std::array<std::uint8_t, PartyDataLength> partyData{};
};

class PartyTransfer
{
public:
    static bool HasActiveParty(bool runtimeInGame);
    static PartySnapshot Capture(const std::string& exportedAt = {});
    static void Apply(const PartySnapshot& snapshot);
    static bool Save(const PartySnapshot& snapshot, const std::filesystem::path& path,
                     std::string& error);
    static bool Load(const std::filesystem::path& path, PartySnapshot& snapshot,
                     std::string& error);

    static std::string DecodeString(std::uint16_t address, std::size_t length);
    static void EncodeString(std::uint16_t address, std::size_t length,
                             const std::string& value, bool centered);
    static std::string TimestampNow();
};

} // namespace dlrl
