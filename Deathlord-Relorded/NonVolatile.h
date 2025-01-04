#pragma once

enum class AutoMapQuadrant
{
	All = 0,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	FollowPlayer = 99
};

struct RelordedChanges {
	bool xp_reallocation;
	bool exit_pit_by_moving;
	bool freeze_time_when_idle;
	bool ranged_attack_for_rear_line;
	bool search_always_succeeds;
	bool no_level_drain;
	bool magic_water_increases_stats;
	bool no_stats_limit;
	bool no_hp_loss_from_starvation;
	bool extra_race_and_class_bonuses;
	bool no_autosave_after_death;
	bool expanded_weapon_use;
	bool keep_extra_xp_on_levelup;
	bool distribute_food;
	bool fix_gold_pooling;
	bool distribute_gold;
};

class NonVolatile
{
public:
	std::wstring hdvPath;
	int speed = 1;
	float applewinScale = 1.0f;
	int opacityF11 = 85;
	bool scanlines = false;
	bool removeFog = true;
	bool showFootsteps = true;
	bool showHidden = true;
	bool showSpells = true;
	bool showInv = false;
	int video = 1;
	int volumeSpeaker = 1;
	bool useGameLink = false;
	bool logCombat = false;
	bool noEffects = false;
	bool englishNames = false;
	AutoMapQuadrant mapQuadrant = AutoMapQuadrant::FollowPlayer;	// Not saved, always reverts to All upon restart

	// Independent file containing map data:
	// contains markers data (8 bits per tile) for all maps
	std::map<std::string, std::vector<UINT8>>fogOfWarMarkers;
	// contains all sectors seen on the overland (XY coordinates, from 0x00 to 0xFF. X is the high nibble)
	std::vector<UINT8>sectorsSeen;

	// I/O
	int SaveToDisk();
	int LoadFromDisk();

	RelordedChanges relordedChanges = {
		true, // xpReallocation
		true, // exitPitByMoving
		true, // freezeTimeWhenIdle
		true, // rangedAttackForRearLine
		true, // searchAlwaysSucceeds
		true, // noLevelDrain
		true, // magicWaterIncreasesStats
		true, // noStatsLimit
		true, // noHpLossFromStarvation
		true, // extraRaceAndClassBonuses
		true, // noAutosaveAfterDeath
		true, // expandedWeaponUse
		true, // keepExtraXpOnLevelup
		true, // distributeFood
		true, // fixGoldPooling
		true  // distributeGold
	};
};

