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
	std::wstring profilePath;
	std::wstring diskBootPath;
	std::wstring diskScenAPath;
	std::wstring diskScenBPath;
	int speed = 1;
	bool scanlines = false;
	bool showMap = true;
	bool showFog = true;
	bool showFootsteps = true;
	bool showHidden = true;
	bool showSpells = true;
	bool showInv = false;
	int video = 1;
	int volumeSpeaker = 1;
	bool useGameLink = false;
	bool logCombat = false;
	AutoMapQuadrant mapQuadrant = AutoMapQuadrant::FollowPlayer;	// Not saved, always reverts to All upon restart

	// contains markers data (8 bits per tile) for all maps
	// saved in an independent file
	std::map<std::string, std::vector<UINT8>>fogOfWarMarkers;

	// I/O
	int SaveToDisk();
	int LoadFromDisk();
};

