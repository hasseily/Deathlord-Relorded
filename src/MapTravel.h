#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dlrl
{
struct MapAddress { std::uint8_t track = 0, sector = 0; };

struct MapDestination
{
    std::string name;
    std::string region;
    int type = 0; // Deathlord: indoor=0, overland=1, dungeon=2.
    int worldX = 0, worldY = 0;
    int exitX = 0, exitY = 0;
    MapAddress address;
    MapAddress parentTown;
    int townExitX = 0, townExitY = 0;
    bool insideTown = false;
    int floors = 1;
    std::array<MapAddress, 4> floorGroups{};
};

struct TeleportRequest
{
    MapDestination destination;
    int floor = 1;
    int x = 0, y = 0; // Coordinates within the selected floor, zero based.
};

// Read-only view of the active scenario. The emulator owns all writes.
class MapTravel
{
public:
    bool Load(const std::filesystem::path& hdv, std::string& error);
    const std::vector<MapDestination>& Destinations() const { return destinations_; }
    bool Preview(const MapDestination& destination, int floor,
                 std::array<std::uint8_t, 4096>& tiles, std::string& error) const;
    static bool Validate(const TeleportRequest& request, std::string& error);
    static int Size(const MapDestination& destination) { return destination.type == 2 ? 32 : 64; }

private:
    std::vector<std::uint8_t> scenario_;
    std::vector<MapDestination> destinations_;
    std::vector<std::uint8_t> ReadPages(int side, MapAddress& address, int count) const;
    void AddDungeon(MapDestination destination);
};
} // namespace dlrl
