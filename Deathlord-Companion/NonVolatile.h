#pragma once
class NonVolatile
{
public:
	std::wstring profilePath;
	std::wstring diskBootPath;
	std::wstring diskScenAPath;
	std::wstring diskScenBPath;
	int speed = 1;
	bool scanlines = false;
	int video = 1;
	int volumeSpeaker = 1;
	bool useGameLink = false;
	bool logCombat = false;

	// I/O
	int SaveToDisk();
	int LoadFromDisk();
};

