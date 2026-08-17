#pragma once

#include <cstdint>

namespace dlrl::deathlord
{

constexpr std::uint8_t PartySize = 6;

// Deathlord main-memory layout used by the Relorded hooks.
constexpr std::uint16_t MapIsInGame = 0xFCE0;
constexpr std::uint16_t MapIsOverland = 0xFC10;
constexpr std::uint16_t MapType = 0xFC04;
constexpr std::uint16_t MapX = 0xFC06;
constexpr std::uint16_t MapY = 0xFC07;
constexpr std::uint16_t MapOverlandX = 0xFC4B;
constexpr std::uint16_t MapOverlandY = 0xFC4C;
constexpr std::uint16_t GameMap = 0x0C00;
constexpr std::uint16_t DayHour = 0xFC00;
constexpr std::uint16_t DayMinute = 0xFC01;
constexpr std::uint16_t DayOfMonth = 0xFC02;
constexpr std::uint16_t PartyCurrentCharacter = 0xFC21;
constexpr std::uint16_t PartyCurrentClass = 0xFC22;
constexpr std::uint16_t PartyLeader = 0xFC19;
constexpr std::uint16_t PartyIconType = 0xFC1D;
constexpr std::uint16_t PartySizeAddress = 0xFC20;
constexpr std::uint16_t PartyPartyName = 0xFCF0;
constexpr std::uint16_t PartyName = 0xFD00;
constexpr std::uint16_t PartyInventory = 0xFE20;
constexpr std::uint16_t PartyClass = 0xFD60;
constexpr std::uint16_t PartyRace = 0xFD5A;
constexpr std::uint16_t PartyGender = 0xFDC0;
constexpr std::uint16_t PartyAlignment = 0xFDC6;
constexpr std::uint16_t PartyWeaponReady = 0xFDEA;
constexpr std::uint16_t PartyStatus = 0xFD36;
constexpr std::uint16_t PartyArmorClass = 0xFD3C;
constexpr std::uint16_t PartyHealthLow = 0xFD4E;
constexpr std::uint16_t PartyHealthHigh = 0xFD54;
constexpr std::uint16_t PartyHealthMaxLow = 0xFD42;
constexpr std::uint16_t PartyHealthMaxHigh = 0xFD48;
constexpr std::uint16_t PartyFood = 0xFDD2;
constexpr std::uint16_t PartyTorches = 0xFDE4;
constexpr std::uint16_t PartyGoldLow = 0xFDD8;
constexpr std::uint16_t PartyGoldHigh = 0xFDDE;
constexpr std::uint16_t PartyXpLow = 0xFDF6;
constexpr std::uint16_t PartyXpHigh = 0xFDFC;
constexpr std::uint16_t PartyLevel = 0xFD66;
constexpr std::uint16_t PartyLevelPlus = 0xFD6C;
constexpr std::uint16_t PartyPower = 0xFD96;
constexpr std::uint16_t PartyPowerMax = 0xFD9C;
constexpr std::uint16_t PartyStrength = 0xFD72;
constexpr std::uint16_t PartyConstitution = 0xFD78;
constexpr std::uint16_t PartySizeAttribute = 0xFD7E;
constexpr std::uint16_t PartyIntelligence = 0xFD84;
constexpr std::uint16_t PartyDexterity = 0xFD8A;
constexpr std::uint16_t PartyCharisma = 0xFD90;
constexpr std::uint16_t PartyMagicUserType = 0xFDA2;
constexpr std::uint16_t CurrentPlayerTile = 0x0328;
constexpr std::uint16_t MapMonsterSpriteIds = 0x08EF;
constexpr std::uint16_t MapVisibilityRadius = 0xFC05;
constexpr std::uint16_t MapId = 0xFC4E;
constexpr std::uint16_t MapFloor = 0xFC4F;

// Live monster tracking, used to recover the static tile a monster stands on.
// Overland and towns track 32 monsters; dungeons track 16 per floor times 4.
constexpr std::uint16_t OverlandMonsterX = 0x0800;
constexpr std::uint16_t OverlandMonsterY = 0x0820;
constexpr std::uint16_t OverlandMonsterTile = 0x0860;
constexpr std::uint8_t OverlandMonsterCount = 32;
constexpr std::uint16_t DungeonMonsterX = 0x0800;
constexpr std::uint16_t DungeonMonsterY = 0x0840;
constexpr std::uint16_t DungeonMonsterTile = 0x0AC0;
constexpr std::uint8_t DungeonMonsterCount = 64;

constexpr std::uint16_t CharacterCreateRng = 0x6500;
constexpr std::uint16_t CharacterCreateAttributes = 0x71F2;
constexpr std::uint16_t RaceAttributeMaximums = 0x70E0;

constexpr std::uint16_t BattleGetXp = 0xAFE8;
constexpr std::uint16_t BattleXpLow = 0xAFEE;
constexpr std::uint16_t BattleXpHigh = 0xAFEF;
constexpr std::uint16_t BattleGoldLow = 0x009A;
constexpr std::uint16_t BattleGoldHigh = 0x009B;
constexpr std::uint16_t BattleDamage = 0x0067;
constexpr std::uint16_t BattleCharacterHealLow = 0x006C;
constexpr std::uint16_t BattleCharacterHealHigh = 0x7845;
constexpr std::uint16_t BattleEnemyIndex = 0xA574;
constexpr std::uint16_t BattleEnemyCount = 0x0052;
constexpr std::uint16_t BattleEnemyDisabled = 0xAF8A;
constexpr std::uint16_t BattleEnemyHealth = 0xAFAA;
constexpr std::uint16_t MonsterCurrentHealthMultiplier = 0xAF72;
constexpr std::uint16_t MonsterCurrentName = 0xAF7E;

constexpr std::uint16_t PrintInverse = 0x00B4;
constexpr std::uint16_t PrintXOrigin = 0x00AD;
constexpr std::uint16_t PrintYOrigin = 0x00AA;
constexpr std::uint16_t PrintX = 0x00AE;
constexpr std::uint16_t PrintY = 0x00AF;
constexpr std::uint16_t PrintWidth = 0x00AB;
constexpr std::uint16_t PrintHeight = 0x00AC;

// Program counters in the 2.0.1 HDV executable. Floppy-management PCs are
// intentionally absent: DLRL 3.0 boots and runs exclusively from SmartPort.
constexpr std::uint16_t PcTitleKey = 0x1D47;
constexpr std::uint16_t PcMenuKey = 0x1C19;
constexpr std::uint16_t PcCharacterManagementKey = 0x7C0A;
constexpr std::uint16_t PcCharacterWaitKey = 0x7A63;
constexpr std::uint16_t PcCharacterEscape = 0x701B;
// Presentation-only checkpoint retained from v2: the HDV has finished its
// pre-game party/scenario validation and the Relorded credits splash can own
// the screen while the main map loads. No floppy behavior is attached.
constexpr std::uint16_t PcStartupSplash = 0x845C;

constexpr std::uint16_t PcMoveOverland = 0xEFF3;
constexpr std::uint16_t PcMoveDungeon = 0xB0EB;
constexpr std::uint16_t PcClearMap = 0x8A17;
constexpr std::uint16_t PcDecrementTimer = 0x621F;
constexpr std::uint16_t PcChangeOverlandMap = 0x82D9;
constexpr std::uint16_t PcChangeMapType = 0x8326;
constexpr std::uint16_t PcTransitOutOverland = 0xE8F4;
constexpr std::uint16_t PcTransitOutTown = 0xEF0A;
constexpr std::uint16_t PcTransitOutDungeon = 0xB175;
constexpr std::uint16_t PcChangeFloor = 0xB13C;
constexpr std::uint16_t PcEndDrawingTiles = 0x586A;
constexpr std::uint16_t PcMapKey = 0x5893;
constexpr std::uint16_t PcAllCharactersDead = 0x880C;

constexpr std::uint16_t PcRearAttackCheck = 0xA7EF;
constexpr std::uint16_t PcSearchCheck = 0x9428;
constexpr std::uint16_t PcEnemyDrain = 0xAAE5;
constexpr std::uint16_t PcMagicWater = 0xB707;
constexpr std::uint16_t PcStatCeiling = 0xB7A3;
constexpr std::uint16_t PcCharacterHpLoss = 0x54B8;
constexpr std::uint16_t PcCharacterTileDamage = 0x6038;
constexpr std::uint16_t PcSaveAfterOneDead = 0x5BC5;
constexpr std::uint16_t PcSaveAfterAllDead = 0x5C6E;
constexpr std::uint16_t PcNinjaMonkAcMask = 0xA952;
constexpr std::uint16_t PcReadyWeaponCheck = 0x6B98;
constexpr std::uint16_t PcIncrementLevel = 0xF5BE;
constexpr std::uint16_t PcResetXpOnLevel = 0xF5D4;
constexpr std::uint16_t PcLevelCheck = 0xF563;
constexpr std::uint16_t PcCharacterLevelUp = 0xA361;

constexpr std::uint16_t PcBuyFood = 0xF3F6;
constexpr std::uint16_t PcIncrementFood = 0xF433;
constexpr std::uint16_t PcPoolGold = 0x74D1;
constexpr std::uint16_t PcPoolGoldEnd = 0x7538;
constexpr std::uint16_t PcGiveBattleGold = 0x8EF5;

constexpr std::uint16_t PcScrollWindow = 0x5395;
constexpr std::uint16_t PcPrintCharacter = 0x532D;
constexpr std::uint16_t PcInverseLine = 0x53EB;
constexpr std::uint16_t PcClearPrintArea = 0x52BC;

constexpr std::uint16_t PcBattleAmbush = 0xEB43;
constexpr std::uint16_t PcBattleEnter = 0xEC30;
constexpr std::uint16_t PcBattleExit = 0xA37F;
constexpr std::uint16_t PcBattleEnemyHpSet = 0xA2D1;
constexpr std::uint16_t PcBattleEnemyId = 0xA244;
constexpr std::uint16_t PcBattleCharacterTurn = 0x5C0B;
constexpr std::uint16_t PcBattleCharacterAttack = 0xA85E;
constexpr std::uint16_t PcBattleCharacterHit = 0xA88E;
constexpr std::uint16_t PcBattleCharacterKilled = 0xA8C2;
constexpr std::uint16_t PcBattleCharacterBanished = 0x7B53;
constexpr std::uint16_t PcBattleCharacterHealed = 0x785B;
constexpr std::uint16_t PcBattleEnemyTurn = 0xA50C;
constexpr std::uint16_t PcBattleEnemyAttack = 0xAA42;
constexpr std::uint16_t PcBattleEnemyMissed = 0xAB43;
constexpr std::uint16_t PcBattleEnemyHit = 0xADC7;
constexpr std::uint16_t PcBattleBeginXp = 0xA30A;
constexpr std::uint16_t PcDead = 0x897E;
constexpr std::uint16_t PcGiveOrb = 0x9825;

constexpr std::uint8_t StackMapKeyLow = 0xCB;
constexpr std::uint8_t StackMapKeyHigh = 0x57;
constexpr std::uint8_t StackSpellLow = 0x1E;
constexpr std::uint8_t StackSpellHigh = 0x5F;

enum class CharacterClass : std::uint8_t
{
    Fighter, Paladin, Ranger, Barbarian, Berzerker, Samurai, DarkKnight,
    Thief, Assassin, Ninja, Monk, Priest, Druid, Magician, Illusionist,
    Peasant
};

enum class Race : std::uint8_t
{
    Human, Elf, HalfElf, Dwarf, Gnome, DarkElf, Orc, HalfOrc
};

enum class Attribute : std::uint8_t
{
    Strength, Constitution, Size, Intelligence, Dexterity, Charisma, Power
};

} // namespace dlrl::deathlord
