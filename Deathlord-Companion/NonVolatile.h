#pragma once
class NonVolatile
{
public:
	std::wstring profilePath;
	std::wstring hdvPath;
	int speed = 1;
	bool scanlines = false;
	int video = 1;
	int volumeSpeaker = 1;
	int volumeMockingBoard = 1;
	bool useGameLink = false;
	bool logCombat = false;

	// I/O
	int SaveToDisk();
	int LoadFromDisk();
};

