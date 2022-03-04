#include "pch.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <shobjidl_core.h> 
#include "NonVolatile.h"
#include "nlohmann/json.hpp"
#include "HAUtils.h"
#include <shlobj.h>
#include <objbase.h>

using namespace std;
namespace fs = std::filesystem;

static nlohmann::json nv_json = R"(
  {
    "hdvPath":			  "",
    "diskBootPath":		  "Images\\Deathlord - Disk 1, Side A Boot disk.woz",
    "diskScenAPath":	  "Images\\Deathlord Scenario A.nib",
    "diskScenBPath":	  "Images\\Deathlord Scenario B.nib",
    "applewinScale":	  1.0,
	"scanlines":		  true,
	"removeFog":		  false,
	"showFootsteps":	  false,
	"showHidden":		  false,
	"showSpells":		  false,
	"video":			  0,
    "volumeSpeaker":	  2,
	"useGameLink":        false,
	"logCombat":		  false
  }
)"_json;

static nlohmann::json nvmarkers_json = R"(
	{
		"fogOfWarMarkers":	  {},
		"sectorsSeen":		  []
	}
)"_json;

static std::wstring configfilename = L"DeathlordRelorded.conf";
static std::wstring markersfilename = L"dcmarkers.data";	// contains markers data (8 bits per tile) for all maps

bool FileExists(LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

int NonVolatile::SaveToDisk()
{
	// Determine saved games folder
	std::wstring savedGamesPath;
	PWSTR path = NULL;
	HRESULT r = SHGetKnownFolderPath(FOLDERID_SavedGames, KF_FLAG_CREATE, NULL, &path);
	if (path != NULL)
	{
		savedGamesPath.assign(path);
		savedGamesPath.append(L"\\Deathlord Relorded\\");
		CoTaskMemFree(path);

		auto _dbfn = fs::path(diskBootPath).filename().wstring();
		if (!FileExists((savedGamesPath + _dbfn).c_str()))
		{
			CopyFile(diskBootPath.c_str(), (savedGamesPath + _dbfn).c_str(), true);
			diskBootPath = savedGamesPath + _dbfn;
		}
		auto _safn = fs::path(diskScenAPath).filename().wstring();
		if (!FileExists((savedGamesPath + _safn).c_str()))
		{
			CopyFile(diskScenAPath.c_str(), (savedGamesPath + _safn).c_str(), true);
			diskScenAPath = savedGamesPath + _safn;
		}
		auto _sbfn = fs::path(diskScenBPath).filename().wstring();
		if (!FileExists((savedGamesPath + _sbfn).c_str()))
		{
			CopyFile(diskScenBPath.c_str(), (savedGamesPath + _sbfn).c_str(), true);
			diskScenBPath = savedGamesPath + _sbfn;
		}
	}

	std::string sDiskBootPath;
	std::string sDiskScenAPath;
	std::string sDiskScenBPath;
	HA::ConvertWStrToStr(&diskBootPath, &sDiskBootPath);
	HA::ConvertWStrToStr(&diskScenAPath, &sDiskScenAPath);
	HA::ConvertWStrToStr(&diskScenBPath, &sDiskScenBPath);

	nv_json["diskBootPath"]			= sDiskBootPath;
	nv_json["diskScenAPath"]		= sDiskScenAPath;
	nv_json["diskScenBPath"]		= sDiskScenBPath;
	nv_json["applewinScale"]		= applewinScale;
	nv_json["scanlines"]			= scanlines;
	nv_json["removeFog"]			= removeFog;
	nv_json["showFootsteps"]		= showFootsteps;
	nv_json["showHidden"]			= showHidden;
	nv_json["showSpells"]			= showSpells;
	nv_json["video"]				= video;
	nv_json["volumeSpeaker"]		= volumeSpeaker;
	nv_json["useGameLink"]			= useGameLink;
	nv_json["logCombat"]			= logCombat;
	std::ofstream out(savedGamesPath + configfilename);
	out << std::setw(4) << nv_json << std::endl;
	out.close();

	// save markers and other map data independently
	nvmarkers_json["fogOfWarMarkers"] = fogOfWarMarkers;
	nvmarkers_json["sectorsSeen"] = sectorsSeen;
	out.open(savedGamesPath + markersfilename);
	out << std::setw(0) << nvmarkers_json << std::endl;
	out.close();

	return 0;
}

int NonVolatile::LoadFromDisk()
{
	bool shouldSaveAfterLoad = false;	// If the save game folder doesn't exist, we'll save afterwards

	// Determine saved games folder
	std::wstring savedGamesPath;
	PWSTR path = NULL;

	HRESULT r = SHGetKnownFolderPath(FOLDERID_SavedGames, KF_FLAG_CREATE, NULL, &path);
	if (path != NULL)
	{
		savedGamesPath.assign(path);
		savedGamesPath.append(L"\\Deathlord Relorded\\");
		CoTaskMemFree(path);

		// Try to create the save game dir. If it already exists, use it
		BOOL bRes = CreateDirectory(savedGamesPath.c_str(), NULL);
		if (bRes != 0)
		{
			// The saved games dir didn't exist, load from default game dir
			savedGamesPath = L"";
			shouldSaveAfterLoad = true;
		}
	}

	nlohmann::json j;
	std::ifstream in(savedGamesPath + configfilename);
	if (!in.fail())
	{
		in >> j;
		in.close();
		nv_json.merge_patch(j);
	}
	// load markers
	j.clear();
	in.open(savedGamesPath + markersfilename);
	if (!in.fail())
	{
		in >> j;
		in.close();
		nvmarkers_json.merge_patch(j);
	}

	std::string _diskBootPath = nv_json["diskBootPath"].get<std::string>();
	HA::ConvertStrToWStr(&_diskBootPath, &diskBootPath);
	std::string _diskScenAPath = nv_json["diskScenAPath"].get<std::string>();
	HA::ConvertStrToWStr(&_diskScenAPath, &diskScenAPath);
	std::string _diskScenBPath = nv_json["diskScenBPath"].get<std::string>();
	HA::ConvertStrToWStr(&_diskScenBPath, &diskScenBPath);

	applewinScale = nv_json["applewinScale"].get<float>();
	scanlines = nv_json["scanlines"].get<bool>();
	removeFog = nv_json["removeFog"].get<bool>();
	showFootsteps = nv_json["showFootsteps"].get<bool>();
	showHidden = nv_json["showHidden"].get<bool>();
	showSpells = nv_json["showSpells"].get<bool>();
	video = nv_json["video"].get<int>();
	volumeSpeaker = nv_json["volumeSpeaker"].get<int>();
	useGameLink = nv_json["useGameLink"].get<bool>();
	logCombat = nv_json["logCombat"].get<bool>();

	// markers
	fogOfWarMarkers = nvmarkers_json["fogOfWarMarkers"].get<std::map<std::string, std::vector<UINT8>>>();
	// overland sectors seen
	sectorsSeen = nvmarkers_json["sectorsSeen"].get<std::vector<UINT8>>();

	if (shouldSaveAfterLoad)
		SaveToDisk();
	return 0;
}
