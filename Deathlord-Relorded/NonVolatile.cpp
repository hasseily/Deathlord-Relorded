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
#include <shlwapi.h>
#include <objbase.h>

// Link with Shlwapi.lib for PathRemoveFileSpec
#pragma comment(lib, "Shlwapi.lib")

using namespace std;
namespace fs = std::filesystem;

static nlohmann::json nv_json = R"(
  {
    "hdvPath":			  "Images\\Deathlord PRODOS.hdv",
    "applewinScale":	  1.0,
    "opacityF11":		  85,
	"scanlines":		  true,
	"removeFog":		  false,
	"showFootsteps":	  false,
	"showHidden":		  false,
	"showSpells":		  false,
	"video":			  0,
    "volumeSpeaker":	  2,
	"useGameLink":        false,
	"logCombat":		  false,
	"noEffects":		  false,
	"englishNames":		  false
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

		auto _dbfn = fs::path(hdvPath).filename().wstring();
		if (!FileExists((savedGamesPath + _dbfn).c_str()))
		{
			CopyFile(hdvPath.c_str(), (savedGamesPath + _dbfn).c_str(), true);
			hdvPath = savedGamesPath + _dbfn;
		}
	}

	std::string sHdvPath;
	HA::ConvertWStrToStr(&hdvPath, &sHdvPath);

	nv_json["hdvPath"]				= sHdvPath;
	nv_json["applewinScale"]		= applewinScale;
	nv_json["opacityF11"]			= opacityF11;
	nv_json["scanlines"]			= scanlines;
	nv_json["removeFog"]			= removeFog;
	nv_json["showFootsteps"]		= showFootsteps;
	nv_json["showHidden"]			= showHidden;
	nv_json["showSpells"]			= showSpells;
	nv_json["video"]				= video;
	nv_json["volumeSpeaker"]		= volumeSpeaker;
	nv_json["useGameLink"]			= useGameLink;
	nv_json["logCombat"]			= logCombat;
	nv_json["noEffects"]			= noEffects;
	nv_json["englishNames"]			= englishNames;
	nv_json["relordedChanges"] = {
		{"XP Reallocation", relordedChanges.xp_reallocation},
		{"Exit Pit By Moving", relordedChanges.exit_pit_by_moving},
		{"Freeze Time When Idle", relordedChanges.freeze_time_when_idle},
		{"Ranged Attack For Rear Line", relordedChanges.ranged_attack_for_rear_line},
		{"Search Always Succeeds", relordedChanges.search_always_succeeds},
		{"No Level Drain", relordedChanges.no_level_drain},
		{"Magic Water Increases Stats", relordedChanges.magic_water_increases_stats},
		{"No Stats Maximum", relordedChanges.no_stats_limit},
		{"No HP Loss From Starvation", relordedChanges.no_hp_loss_from_starvation},
		{"Extra Race And Class Bonuses", relordedChanges.extra_race_and_class_bonuses},
		{"No Autosave After Death", relordedChanges.no_autosave_after_death},
		{"Expanded Weapon Use", relordedChanges.expanded_weapon_use},
		{"Keep Extra XP On Levelup", relordedChanges.keep_extra_xp_on_levelup},
		{"Distribute Food", relordedChanges.distribute_food},
		{"Fix Gold Pooling", relordedChanges.fix_gold_pooling},
		{"Distribute Gold", relordedChanges.distribute_gold}
	};
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

	std::string _hdvPath = nv_json["hdvPath"].get<std::string>();
	HA::ConvertStrToWStr(&_hdvPath, &hdvPath);

	applewinScale = nv_json["applewinScale"].get<float>();
	opacityF11 = nv_json["opacityF11"].get<float>();
	scanlines = nv_json["scanlines"].get<bool>();
	removeFog = nv_json["removeFog"].get<bool>();
	showFootsteps = nv_json["showFootsteps"].get<bool>();
	showHidden = nv_json["showHidden"].get<bool>();
	showSpells = nv_json["showSpells"].get<bool>();
	video = nv_json["video"].get<int>();
	volumeSpeaker = nv_json["volumeSpeaker"].get<int>();
	useGameLink = nv_json["useGameLink"].get<bool>();
	logCombat = nv_json["logCombat"].get<bool>();
	noEffects = nv_json["noEffects"].get<bool>();
	englishNames = nv_json["englishNames"].get<bool>();

	auto rjson = nv_json["relordedChanges"];
	if (rjson.is_object()) {
		try {
			relordedChanges.xp_reallocation = rjson.value("XP Reallocation", true);
			relordedChanges.exit_pit_by_moving = rjson.value("Exit Pit By Moving", true);
			relordedChanges.freeze_time_when_idle = rjson.value("Freeze Time When Idle", true);
			relordedChanges.ranged_attack_for_rear_line = rjson.value("Ranged Attack For Rear Line", true);
			relordedChanges.search_always_succeeds = rjson.value("Search Always Succeeds", true);
			relordedChanges.no_level_drain = rjson.value("No Level Drain", true);
			relordedChanges.magic_water_increases_stats = rjson.value("Magic Water Increases Stats", true);
			relordedChanges.no_stats_limit = rjson.value("No Stats Maximum", true);
			relordedChanges.no_hp_loss_from_starvation = rjson.value("No HP Loss From Starvation", true);
			relordedChanges.extra_race_and_class_bonuses = rjson.value("Extra Race And Class Bonuses", true);
			relordedChanges.no_autosave_after_death = rjson.value("No Autosave After Death", true);
			relordedChanges.expanded_weapon_use = rjson.value("Expanded Weapon Use", true);
			relordedChanges.keep_extra_xp_on_levelup = rjson.value("Keep Extra XP On Levelup", true);
			relordedChanges.distribute_food = rjson.value("Distribute Food", true);
			relordedChanges.fix_gold_pooling = rjson.value("Fix Gold Pooling", true);
			relordedChanges.distribute_gold = rjson.value("Distribute Gold", true);
		}
		catch (const std::exception& e) {
			std::cerr << "Error loading RelordedChanges: " << e.what() << std::endl;
		}
	}

	// markers
	fogOfWarMarkers = nvmarkers_json["fogOfWarMarkers"].get<std::map<std::string, std::vector<UINT8>>>();
	// overland sectors seen
	sectorsSeen = nvmarkers_json["sectorsSeen"].get<std::vector<UINT8>>();

	if (shouldSaveAfterLoad)
		SaveToDisk();
	return 0;
}
