#include "MapTravel.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace dlrl
{
namespace
{
struct WorldEntry
{
    const char* region;
    int x, y, track, sector;
    std::array<const char*, 8> names;
};

// Entrance records in the 2.0.1 scenario: five indoor and three dungeon slots.
constexpr WorldEntry World[] = {
    {"Hell Island",11,1,0,15,{"Skull Keep"}},
    {"Chigaku",7,2,4,15,{"Crystalmist","Fort Wintergreen",nullptr,nullptr,nullptr,"Tower of Shumi","Troll Hole"}},
    {"Lost Isles",1,3,8,14,{nullptr,nullptr,nullptr,nullptr,nullptr,"Caves of the Four Elements"}},
    {"Black Isles",14,3,9,13,{"Red Shogun's Castle",nullptr,nullptr,nullptr,nullptr,"Doors Dungeon"}},
    {"Kodan",4,4,11,8,{"Tokugawa","Emperor's Palace","Kawa"}},
    {"Kodan",5,4,16,6,{"Tokushima","Yokahama Ruins",nullptr,nullptr,nullptr,"Caves east of Kawa"}},
    {"Kodan",4,5,19,0,{"Wakiza Ruins"}},
    {"Osozaki",9,5,22,0,{"Deepingdale","Wakai Ruins (Vorn)",nullptr,nullptr,nullptr,"Telegrond"}},
    {"Asagata",1,6,28,15,{"Towne Royal","Croyo",nullptr,nullptr,nullptr,"Fire Giants' Lair"}},
    {"Desert Island",12,7,0,15,{}},
    {"Akmihr",7,8,0,13,{"Desert Flower","Oasis","Akhamun-Ra's Pyramid","Sultan's Palace",nullptr,"Kobito Mines"}},
    {"Isle of the Dead",15,9,5,12,{"Pyramid of the Old Ones"}},
    {"Narawn",2,10,8,2,{"Kashiwa","Malkanth","Fort Demonguard","Lost Lagoon"}},
    {"Giluin",11,10,12,0,{"Kobar",nullptr,nullptr,nullptr,nullptr,"Linear Dungeon"}},
    {"Giluin",11,11,16,4,{"Shupan","Temple of Oceanus"}},
    {"Forest Island",6,12,19,9,{}},
    {"Sirion",3,13,19,7,{"Greenbanks Ruins",nullptr,nullptr,nullptr,nullptr,"Chessboard Dungeon"}},
    {"Sirion",4,13,22,10,{"Clearview",nullptr,nullptr,nullptr,nullptr,"Staircase Dungeon"}},
    {"Tsumani",14,14,24,3,{"Morningfrost","Snow Raven",nullptr,nullptr,nullptr,"Chutes and Ladders"}},
    {"Nyuku",2,0,27,0,{"Twin Rivers","North Spindrift","South Spindrift",nullptr,nullptr,"Sunken Temple"}},
};

int Side(const MapDestination& map) { return map.worldY >= 1 && map.worldY < 7 ? 0 : 1; }
std::uint16_t Word(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return bytes.at(offset) | (bytes.at(offset + 1) << 8);
}
}

std::vector<std::uint8_t> MapTravel::ReadPages(int side, MapAddress& address, int count) const
{
    std::vector<std::uint8_t> result;
    for (int i = 0; i < count; ++i)
    {
        if (side < 0 || side > 1 || address.track >= 35 || address.sector >= 16)
            throw std::runtime_error("Invalid scenario map address.");
        const std::size_t offset = ((side * 35 + address.track) * 16 + address.sector) * 256;
        if (offset + 256 > scenario_.size()) throw std::runtime_error("Truncated scenario data.");
        result.insert(result.end(), scenario_.begin() + offset, scenario_.begin() + offset + 256);
        if (address.sector == 0 || (address.track == 0 && address.sector == 1))
        {
            ++address.track;
            address.sector = address.track == 19 ? 11 : 15;
        }
        else --address.sector;
    }
    return result;
}

void MapTravel::AddDungeon(MapDestination destination)
{
    destination.type = 2;
    auto cursor = destination.address;
    auto headerCursor = cursor;
    const auto header = ReadPages(Side(destination), headerCursor, 1);
    destination.floors = header[0xEC];
    if (destination.floors < 1 || destination.floors > 16)
        throw std::runtime_error("Invalid dungeon floor count.");
    for (int group = 0; group < (destination.floors + 3) / 4; ++group)
    {
        destination.floorGroups[group] = cursor;
        headerCursor = cursor;
        const auto data = ReadPages(Side(destination), headerCursor, 1);
        ReadPages(Side(destination), cursor, 3 + data[0xEE]);
    }
    destinations_.push_back(std::move(destination));
}

bool MapTravel::Load(const std::filesystem::path& hdv, std::string& error)
{
    scenario_.clear();
    destinations_.clear();
    try
    {
        std::ifstream input(hdv, std::ios::binary);
        if (!input) throw std::runtime_error("Unable to read the active game disk.");
        const std::vector<std::uint8_t> disk{std::istreambuf_iterator<char>(input), {}};
        const auto pointer = [&](int block, int index) {
            const auto p = static_cast<std::size_t>(block) * 512 + index;
            return disk.at(p) | (disk.at(p + 256) << 8);
        };
        std::set<int> visited;
        for (int directory = 2; directory && scenario_.empty(); directory = Word(disk, directory * 512 + 2))
        {
            if (!visited.insert(directory).second) throw std::runtime_error("Invalid ProDOS directory.");
            for (int entry = 0; entry < 13; ++entry)
            {
                const std::size_t p = directory * 512 + 4 + entry * 39;
                const int kind = disk.at(p) >> 4;
                const int length = disk.at(p) & 15;
                if (p + 39 > disk.size()) throw std::runtime_error("Truncated ProDOS directory.");
                if (kind != 3 || std::string(disk.begin()+p+1, disk.begin()+p+1+length) != "DEATHLORD.Z") continue;
                const int key = Word(disk, p + 17);
                const int size = Word(disk, p + 21) | (disk.at(p + 23) << 16);
                if (size != 286720) throw std::runtime_error("This disk does not contain the supported Deathlord scenario.");
                for (int block = 0; block < size / 512; ++block)
                {
                    const int index = pointer(key, block / 256);
                    const int data = pointer(index, block % 256);
                    if (!index || !data || static_cast<std::size_t>(data + 1) * 512 > disk.size())
                        throw std::runtime_error("Invalid ProDOS scenario blocks.");
                    scenario_.insert(scenario_.end(), disk.begin() + data * 512, disk.begin() + (data + 1) * 512);
                }
            }
        }
        if (scenario_.empty()) throw std::runtime_error("DEATHLORD.Z was not found on this disk.");
        for (const auto& world : World)
        {
            MapDestination overland;
            overland.region = world.region;
            overland.name = std::string(world.region) + " - overworld (" + std::to_string(world.x) + ", " + std::to_string(world.y) + ")";
            overland.type = 1;
            overland.worldX = world.x;
            overland.worldY = world.y;
            overland.address = {static_cast<std::uint8_t>(world.track), static_cast<std::uint8_t>(world.sector)};
            destinations_.push_back(overland);
            auto cursor = overland.address;
            const auto header = ReadPages(Side(overland), cursor, 1);
            for (int entry = 0; entry < 8; ++entry)
            {
                if (!world.names[entry]) continue;
                if (header[0xA0 + entry] == 255) throw std::runtime_error("The scenario entrance table is incompatible.");
                MapDestination map = overland;
                map.name = world.names[entry];
                map.type = entry < 5 ? 0 : 2;
                map.exitX = header[0xA0 + entry];
                map.exitY = header[0xA8 + entry];
                map.address = {header[0xB0 + entry], header[0xB8 + entry]};
                if (map.type == 2) { AddDungeon(map); continue; }
                destinations_.push_back(map);
                cursor = map.address;
                const auto town = ReadPages(Side(map), cursor, 1);
                if (town[0xEC] == 255) continue;
                MapDestination dungeon = map;
                dungeon.insideTown = true;
                dungeon.parentTown = map.address;
                dungeon.address = {town[0xEC], town[0xED]};
                if (map.name == "Emperor's Palace") dungeon.name = "Kawahara's Dungeon";
                else if (map.name == "Kawa") dungeon.name = "Yakuza Guild";
                else if (map.name == "Wakiza Ruins") dungeon.name = "Pirate's Den";
                else if (map.name == "Skull Keep") dungeon.name = "Hell";
                else dungeon.name = map.name + " - dungeon";
                std::array<std::uint8_t,4096> tiles{};
                std::string previewError;
                if (!Preview(map,1,tiles,previewError)) throw std::runtime_error(previewError);
                const auto stairs = std::find_if(tiles.begin(),tiles.end(),[](auto t){ return t % 80 == 3 || t % 80 == 4; });
                if (stairs == tiles.end()) throw std::runtime_error("Dungeon entrance stairs were not found.");
                const auto position = stairs - tiles.begin();
                dungeon.townExitX = position % 64;
                dungeon.townExitY = position / 64 + 1;
                AddDungeon(dungeon);
            }
        }
        MapDestination ocean;
        ocean.name = "World - any sector";
        ocean.region = "World";
        ocean.type = 1;
        ocean.worldX = ocean.worldY = 0;
        destinations_.push_back(ocean);
        std::sort(destinations_.begin(),destinations_.end(),[](const auto& a,const auto& b){ return a.name < b.name; });
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        scenario_.clear();
        destinations_.clear();
        error = exception.what();
        return false;
    }
}

bool MapTravel::Preview(const MapDestination& map, int floor,
                       std::array<std::uint8_t,4096>& tiles, std::string& error) const
{
    tiles.fill(0);
    if (!Validate({map,floor,0,0},error)) return false;
    try
    {
        if (floor < 1 || floor > map.floors) throw std::runtime_error("Invalid floor.");
        auto address = map.type == 2 ? map.floorGroups[(floor - 1) / 4] : map.address;
        if (map.type == 1)
        {
            const auto world = std::find_if(std::begin(World),std::end(World),[&](const auto& w){ return w.x==map.worldX && w.y==map.worldY; });
            if (world == std::end(World)) { tiles.fill(0x2B); error.clear(); return true; }
            address = {static_cast<std::uint8_t>(world->track),static_cast<std::uint8_t>(world->sector)};
        }
        const int pages = map.type == 0 ? 4 : map.type == 1 ? 1 : 3;
        const auto header = ReadPages(Side(map), address, pages);
        const auto compressed = ReadPages(Side(map), address, header[0xEE]);
        std::array<std::uint8_t,4096> raw{};
        std::size_t in = 0, out = 0;
        while (out < raw.size())
        {
            auto tile = compressed.at(in++);
            int count = 1;
            if (tile == 255)
            {
                tile = compressed.at(in++);
                count = compressed.at(in++);
                if (!count) count = 256;
            }
            if (out + count > raw.size()) throw std::runtime_error("Invalid map tile data.");
            std::fill_n(raw.begin() + out, count, tile);
            out += count;
        }
        tiles = raw;
        if (map.type == 2)
        {
            const int offsetX = ((floor - 1) & 1) * 32;
            const int offsetY = (((floor - 1) & 3) / 2) * 32;
            for (int y=0;y<32;++y)
                for (int x=0;x<32;++x) tiles[y*32+x] = raw[(y+offsetY)*64+x+offsetX];
        }
        error.clear();
        return true;
    }
    catch (const std::exception& exception) { error=exception.what(); return false; }
}

bool MapTravel::Validate(const TeleportRequest& request, std::string& error)
{
    const auto& map = request.destination;
    const int size = Size(map);
    if (map.type < 0 || map.type > 2 || map.floors < 1 || map.floors > 16
        || request.floor < 1 || request.floor > map.floors
        || request.x < 0 || request.y < 0 || request.x >= size || request.y >= size
        || map.worldX < 0 || map.worldX > 15 || map.worldY < 0 || map.worldY > 15)
    { error = "Choose a valid map, floor, and position."; return false; }
    if (map.type != 1 && (map.address.track >= 35 || map.address.sector >= 16))
    { error = "Invalid destination map address."; return false; }
    if ((map.type != 2 && map.floors != 1)
        || map.exitX < 0 || map.exitX > 63 || map.exitY < 0 || map.exitY > 63)
    { error = "Invalid destination return position."; return false; }
    if (map.type == 2)
        for (int group=0;group<(map.floors+3)/4;++group)
            if (map.floorGroups[group].track >= 35 || map.floorGroups[group].sector >= 16)
            { error = "Invalid dungeon floor address."; return false; }
    if (map.insideTown && (map.type != 2 || map.parentTown.track >= 35
        || map.parentTown.sector >= 16 || map.townExitX < 0 || map.townExitX > 63
        || map.townExitY < 1 || map.townExitY > 63))
    { error = "Invalid parent town entrance."; return false; }
    error.clear();
    return true;
}
} // namespace dlrl
