#pragma once

enum class AutoMapQuandrant
{
	All,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight
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
	int video = 1;
	int volumeSpeaker = 1;
	bool useGameLink = false;
	bool logCombat = false;
	AutoMapQuandrant mapQuadrant = AutoMapQuandrant::All;	// Not saved, always reverts to All upon restart

	// contains markers data (8 bits per tile) for all maps
	// saved in an independent file
	std::map<std::string, std::vector<UINT8>>fogOfWarMarkers;

	// I/O
	int SaveToDisk();
	int LoadFromDisk();
};

