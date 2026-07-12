#include "InventoryRules.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace dlrl
{

bool InventoryRules::Load(const std::filesystem::path& csvPath)
{
    rules_ = {};
    loaded_ = false;
    std::ifstream input(csvPath);
    if (!input) return false;

    std::string line;
    bool header = true;
    int recordCount = 0;
    while (std::getline(input, line))
    {
        if (header)
        {
            header = false;
            continue;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> fields;
        std::stringstream stream(line);
        std::string field;
        while (std::getline(stream, field, ',')) fields.push_back(field);
        if (fields.size() < 21) continue;

        try
        {
            const auto item = static_cast<std::uint8_t>(std::stoul(fields[0], nullptr, 16));
            Rule rule{};
            rule.slot = std::stoi(fields[1]);
            for (std::size_t index = 0; index < 16; ++index)
            {
                if (std::stoi(fields[index + 2]) != 0)
                    rule.classMask |= static_cast<std::uint16_t>(1u << index);
            }
            rule.raceMask = static_cast<std::uint16_t>(std::stoul(fields[18], nullptr, 0));
            rule.name = fields[19];
            rule.englishName = fields[20];
            if (fields.size() > 21) rule.thaco = std::stoi(fields[21]);
            if (fields.size() > 22) rule.attacks = std::stoi(fields[22]);
            if (fields.size() > 23) rule.damageMinimum = std::stoi(fields[23]);
            if (fields.size() > 24) rule.damageMaximum = std::stoi(fields[24]);
            if (fields.size() > 25) rule.armorClass = std::stoi(fields[25]);
            if (fields.size() > 26) rule.special = fields[26];
            rule.valid = true;
            rules_[item] = rule;
            ++recordCount;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
    loaded_ = recordCount > 0;
    return loaded_;
}

const std::string& InventoryRules::Name(std::uint8_t item, bool english) const
{
    static const std::string unknown = "Unknown item";
    const Rule& rule = rules_[item];
    if (!rule.valid) return unknown;
    if (english && !rule.englishName.empty()) return rule.englishName;
    return rule.name.empty() ? unknown : rule.name;
}

int InventoryRules::Slot(std::uint8_t item) const
{
    return rules_[item].valid ? rules_[item].slot : -1;
}

int InventoryRules::Thaco(std::uint8_t item) const { return rules_[item].thaco; }
int InventoryRules::Attacks(std::uint8_t item) const { return rules_[item].attacks; }
int InventoryRules::DamageMinimum(std::uint8_t item) const
{
    return rules_[item].damageMinimum;
}
int InventoryRules::DamageMaximum(std::uint8_t item) const
{
    return rules_[item].damageMaximum;
}
int InventoryRules::ArmorClass(std::uint8_t item) const
{
    return rules_[item].armorClass;
}
const std::string& InventoryRules::Special(std::uint8_t item) const
{
    return rules_[item].special;
}

bool InventoryRules::CanEquip(std::uint8_t item, std::uint8_t characterClass,
                              std::uint8_t race) const
{
    if (characterClass >= 16 || race >= 16) return false;
    const Rule& rule = rules_[item];
    return rule.valid
        && (rule.classMask & (1u << characterClass)) != 0
        && (rule.raceMask & (1u << race)) != 0;
}

bool InventoryRules::HookCanEquip(std::uint8_t item, std::uint8_t characterClass,
                                  std::uint8_t race, void* userData)
{
    return static_cast<const InventoryRules*>(userData)->CanEquip(
        item, characterClass, race);
}

} // namespace dlrl
