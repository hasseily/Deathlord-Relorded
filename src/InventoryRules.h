#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace dlrl
{

class InventoryRules
{
public:
    bool Load(const std::filesystem::path& csvPath);
    bool CanEquip(std::uint8_t item, std::uint8_t characterClass,
                  std::uint8_t race) const;
    bool IsLoaded() const { return loaded_; }
    const std::string& Name(std::uint8_t item, bool english = true) const;
    int Slot(std::uint8_t item) const;
    int Thaco(std::uint8_t item) const;
    int Attacks(std::uint8_t item) const;
    int DamageMinimum(std::uint8_t item) const;
    int DamageMaximum(std::uint8_t item) const;
    int ArmorClass(std::uint8_t item) const;
    const std::string& Special(std::uint8_t item) const;

    static bool HookCanEquip(std::uint8_t item, std::uint8_t characterClass,
                             std::uint8_t race, void* userData);

private:
    struct Rule
    {
        std::uint16_t classMask = 0;
        std::uint16_t raceMask = 0;
        int slot = -1;
        std::string name;
        std::string englishName;
        int thaco = 0;
        int attacks = 0;
        int damageMinimum = 0;
        int damageMaximum = 0;
        int armorClass = 0;
        std::string special;
        bool valid = false;
    };

    std::array<Rule, 256> rules_{};
    bool loaded_ = false;
};

} // namespace dlrl
