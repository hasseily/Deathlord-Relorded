#include "PartyTransfer.h"

#include "Emulator/StdAfx.h"

#include "DlrlData.h"
#include "Emulator/Memory.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace dlrl
{
namespace
{

using namespace deathlord;

constexpr const char* FormatName = "deathlord-relorded-party";
constexpr int FormatVersion = 1;
constexpr std::uint16_t PartyDataBegin = 0xFD00;

std::string DecodeBytes(const std::uint8_t* source, std::size_t length)
{
    std::string value;
    for (std::size_t index = 0; index < length; ++index)
    {
        const std::uint8_t encoded = source[index];
        char decoded = static_cast<char>((encoded & 0x7F) ^ 0x65);
        if (decoded < 32 || decoded > 126) decoded = ' ';
        value.push_back(decoded);
        if ((encoded & 0x80) != 0) break;
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

template<std::size_t Size>
bool ReadByteArray(const nlohmann::json& value, std::array<std::uint8_t, Size>& output,
                   const char* label, std::string& error)
{
    if (!value.is_array() || value.size() != Size)
    {
        error = std::string(label) + " has the wrong byte count";
        return false;
    }
    for (std::size_t index = 0; index < Size; ++index)
    {
        if (!value[index].is_number_integer())
        {
            error = std::string(label) + " contains a non-byte value";
            return false;
        }
        const int byte = value[index].get<int>();
        if (byte < 0 || byte > 255)
        {
            error = std::string(label) + " contains a value outside 0-255";
            return false;
        }
        output[index] = static_cast<std::uint8_t>(byte);
    }
    return true;
}

template<std::size_t Size>
std::vector<std::uint8_t> Bytes(const std::array<std::uint8_t, Size>& bytes)
{
    return {bytes.begin(), bytes.end()};
}

} // namespace

bool PartyTransfer::HasActiveParty(bool runtimeInGame)
{
    if (!runtimeInGame || !MemGetMainPtr(0)) return false;
    const std::uint8_t size = MemGetMainPtr(PartySizeAddress)[0];
    return size >= 1 && size <= PartySize;
}

PartySnapshot PartyTransfer::Capture(const std::string& exportedAt)
{
    PartySnapshot snapshot;
    snapshot.exportedAt = exportedAt.empty() ? TimestampNow() : exportedAt;
    snapshot.partySize = MemGetMainPtr(PartySizeAddress)[0];
    snapshot.leader = MemGetMainPtr(PartyLeader)[0];
    snapshot.currentCharacter = MemGetMainPtr(PartyCurrentCharacter)[0];
    std::copy_n(MemGetMainPtr(PartyPartyName), snapshot.partyNameBytes.size(),
                snapshot.partyNameBytes.begin());
    std::copy_n(MemGetMainPtr(PartyDataBegin), snapshot.partyData.size(),
                snapshot.partyData.begin());
    snapshot.partyName = DecodeBytes(snapshot.partyNameBytes.data(),
                                     snapshot.partyNameBytes.size());
    return snapshot;
}

void PartyTransfer::Apply(const PartySnapshot& snapshot)
{
    std::copy(snapshot.partyNameBytes.begin(), snapshot.partyNameBytes.end(),
              MemGetMainPtr(PartyPartyName));
    std::copy(snapshot.partyData.begin(), snapshot.partyData.end(),
              MemGetMainPtr(PartyDataBegin));
    MemGetMainPtr(PartySizeAddress)[0] = snapshot.partySize;
    MemGetMainPtr(PartyLeader)[0] = snapshot.leader;
    MemGetMainPtr(PartyCurrentCharacter)[0] = snapshot.currentCharacter;
    const std::uint8_t current = std::min<std::uint8_t>(
        snapshot.currentCharacter, static_cast<std::uint8_t>(PartySize - 1));
    MemGetMainPtr(PartyCurrentClass)[0] = MemGetMainPtr(PartyClass)[current];
}

bool PartyTransfer::Save(const PartySnapshot& snapshot, const std::filesystem::path& path,
                         std::string& error)
{
    try
    {
        const nlohmann::json json = {
            {"format", FormatName},
            {"version", FormatVersion},
            {"exported_at", snapshot.exportedAt},
            {"party_name", snapshot.partyName},
            {"party_size", snapshot.partySize},
            {"leader", snapshot.leader},
            {"current_character", snapshot.currentCharacter},
            {"party_name_bytes", Bytes(snapshot.partyNameBytes)},
            {"party_data_fd00_ffff", Bytes(snapshot.partyData)},
        };
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Unable to create " + path.string();
            return false;
        }
        output << std::setw(2) << json << '\n';
        if (!output)
        {
            error = "Unable to finish writing " + path.string();
            return false;
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

bool PartyTransfer::Load(const std::filesystem::path& path, PartySnapshot& snapshot,
                         std::string& error)
{
    try
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "Unable to open " + path.string();
            return false;
        }
        nlohmann::json json;
        input >> json;
        if (json.value("format", std::string{}) != FormatName
            || json.value("version", 0) != FormatVersion)
        {
            error = "This is not a supported Deathlord Relorded party file";
            return false;
        }

        PartySnapshot loaded;
        loaded.exportedAt = json.value("exported_at", std::string{});
        loaded.partySize = static_cast<std::uint8_t>(json.value("party_size", 0));
        loaded.leader = static_cast<std::uint8_t>(json.value("leader", 255));
        loaded.currentCharacter = static_cast<std::uint8_t>(
            json.value("current_character", 255));
        if (loaded.exportedAt.empty() || loaded.partySize < 1 || loaded.partySize > PartySize
            || loaded.leader >= loaded.partySize
            || loaded.currentCharacter >= loaded.partySize)
        {
            error = "The party metadata is invalid";
            return false;
        }
        if (!ReadByteArray(json.at("party_name_bytes"), loaded.partyNameBytes,
                           "party_name_bytes", error)
            || !ReadByteArray(json.at("party_data_fd00_ffff"), loaded.partyData,
                              "party_data_fd00_ffff", error))
            return false;

        loaded.partyName = DecodeBytes(loaded.partyNameBytes.data(),
                                       loaded.partyNameBytes.size());
        if (loaded.partyName.empty())
        {
            error = "The party file has no party name";
            return false;
        }
        snapshot = std::move(loaded);
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

std::string PartyTransfer::DecodeString(std::uint16_t address, std::size_t length)
{
    return DecodeBytes(MemGetMainPtr(address), length);
}

void PartyTransfer::EncodeString(std::uint16_t address, std::size_t length,
                                 const std::string& value, bool centered)
{
    if (length == 0) return;
    std::string printable;
    printable.reserve(std::min(length, value.size()));
    for (char character : value)
    {
        if (printable.size() == length) break;
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte >= 32 && byte <= 126) printable.push_back(character);
    }

    std::uint8_t* destination = MemGetMainPtr(address);
    const std::uint8_t encodedSpace = static_cast<std::uint8_t>(' ' ^ 0x65);
    std::fill_n(destination, length, encodedSpace);
    const std::size_t offset = centered && printable.size() < length
                             ? (length - printable.size()) / 2 : 0;
    if (printable.empty())
    {
        destination[offset] |= 0x80;
        return;
    }
    for (std::size_t index = 0; index < printable.size(); ++index)
        destination[offset + index] = static_cast<std::uint8_t>(printable[index] ^ 0x65);
    destination[offset + printable.size() - 1] |= 0x80;
}

std::string PartyTransfer::TimestampNow()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

} // namespace dlrl
