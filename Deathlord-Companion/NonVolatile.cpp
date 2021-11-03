#include "pch.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <shobjidl_core.h> 
#include "NonVolatile.h"
#include "nlohmann/json.hpp"
#include "HAUtils.h"

using namespace std;
namespace fs = std::filesystem;

static nlohmann::json nv_json = R"(
  {
    "profilePath": "Profiles\\Deathlord_Default.json",
    "hdvPath":			  "",
    "diskBootPath":		  "Images\\Deathlord - Disk 1, Side A Boot disk.woz",
    "diskScenAPath":	  "Images\\Deathlord Scenario A.nib",
    "diskScenBPath":	  "Images\\Deathlord Scenario B.nib",
	"speed":			  1,
	"scanlines":		  false,
	"showMap":			  true,
	"showFog":			  true,
	"showFootsteps":	  true,
	"showSpells":		  true,
	"video":			  0,
    "volumeSpeaker":	  2,
	"useGameLink":        true,
	"logCombat":		  false
  }
)"_json;

static nlohmann::json nvmarkers_json = R"(
	{
		"fogOfWarMarkers":	  {}
	}
)"_json;

static std::string configfilename = "deathlordcompanion.conf";
static std::string markersfilename = "dcmarkers.data";	// contains markers data (8 bits per tile) for all maps

int NonVolatile::SaveToDisk()
{
	std::string sprofPath;
	std::string sDiskBootPath;
	std::string sDiskScenAPath;
	std::string sDiskScenBPath;
	HA::ConvertWStrToStr(&profilePath, &sprofPath);
	HA::ConvertWStrToStr(&diskBootPath, &sDiskBootPath);
	HA::ConvertWStrToStr(&diskScenAPath, &sDiskScenAPath);
	HA::ConvertWStrToStr(&diskScenBPath, &sDiskScenBPath);

	nv_json["profilePath"]			= sprofPath;
	nv_json["diskBootPath"]			= sDiskBootPath;
	nv_json["diskScenAPath"]		= sDiskScenAPath;
	nv_json["diskScenBPath"]		= sDiskScenBPath;
	nv_json["speed"]				= speed;
	nv_json["scanlines"]			= scanlines;
	nv_json["showMap"]				= showMap;
	nv_json["showFog"]				= showFog;
	nv_json["showFootsteps"]		= showFootsteps;
	nv_json["showSpells"]			= showSpells;
	nv_json["video"]				= video;
	nv_json["volumeSpeaker"]		= volumeSpeaker;
	nv_json["useGameLink"]			= useGameLink;
	nv_json["logCombat"]			= logCombat;
	std::ofstream out(configfilename);
	out << std::setw(4) << nv_json << std::endl;
	out.close();

	// save markers independently
	nvmarkers_json["fogOfWarMarkers"] = fogOfWarMarkers;
	out.open(markersfilename);
	out << std::setw(0) << nvmarkers_json << std::endl;
	out.close();

	return 0;
}

int NonVolatile::LoadFromDisk()
{
	nlohmann::json j;
	std::ifstream in(configfilename);
	if (!in.fail())
	{
		in >> j;
		in.close();
		nv_json.merge_patch(j);
	}
	// load markers
	j.clear();
	in.open(markersfilename);
	if (!in.fail())
	{
		in >> j;
		in.close();
		nvmarkers_json.merge_patch(j);
	}

	std::string _profilePath = nv_json["profilePath"].get<std::string>();
	if (_profilePath.size() == 0) {
		fs::path defaultPath = fs::current_path();
		defaultPath += "\\Profiles\\Deathlord_Default.json";
		_profilePath.assign(defaultPath.string());
	}
	HA::ConvertStrToWStr(&_profilePath, &profilePath);
	std::string _diskBootPath = nv_json["diskBootPath"].get<std::string>();
	HA::ConvertStrToWStr(&_diskBootPath, &diskBootPath);
	std::string _diskScenAPath = nv_json["diskScenAPath"].get<std::string>();
	HA::ConvertStrToWStr(&_diskScenAPath, &diskScenAPath);
	std::string _diskScenBPath = nv_json["diskScenBPath"].get<std::string>();
	HA::ConvertStrToWStr(&_diskScenBPath, &diskScenBPath);

	speed = nv_json["speed"].get<int>();
	scanlines = nv_json["scanlines"].get<bool>();
	showMap = nv_json["showMap"].get<bool>();
	showFog = nv_json["showFog"].get<bool>();
	showFootsteps = nv_json["showFootsteps"].get<bool>();
	showSpells = nv_json["showSpells"].get<bool>();
	video = nv_json["video"].get<int>();
	volumeSpeaker = nv_json["volumeSpeaker"].get<int>();
	useGameLink = nv_json["useGameLink"].get<bool>();
	logCombat = nv_json["logCombat"].get<bool>();

	// markers
	fogOfWarMarkers = nvmarkers_json["fogOfWarMarkers"].get<std::map<std::string, std::vector<UINT8>>>();

	return 0;
}
