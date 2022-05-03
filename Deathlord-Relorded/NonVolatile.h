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

class NonVolatile
{
public:
	std::wstring diskBootPath;
	std::wstring diskScenAPath;
	std::wstring diskScenBPath;
	int speed = 1;
	float applewinScale = 1.0f;
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
	AutoMapQuadrant mapQuadrant = AutoMapQuadrant::FollowPlayer;	// Not saved, always reverts to All upon restart

	// Independent file containing map data:
	// contains markers data (8 bits per tile) for all maps
	std::map<std::string, std::vector<UINT8>>fogOfWarMarkers;
	// contains all sectors seen on the overland (XY coordinates, from 0x00 to 0xFF. X is the high nibble)
	std::vector<UINT8>sectorsSeen;

	// I/O
	int SaveToDisk();
	int LoadFromDisk();
};

