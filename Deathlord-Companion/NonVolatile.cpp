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
	"video":			  0,
    "volumeSpeaker":	  2,
	"useGameLink":        true,
	"logCombat":		  false
  }
)"_json;

static std::string configfilename = "deathlordcompanion.conf";

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
	nv_json["video"]				= video;
	nv_json["volumeSpeaker"]		= volumeSpeaker;
	nv_json["useGameLink"]			= useGameLink;
	nv_json["logCombat"]			= logCombat;
	std::ofstream out(configfilename);
	out << std::setw(4) << nv_json << std::endl;
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
	video = nv_json["video"].get<int>();
	volumeSpeaker = nv_json["volumeSpeaker"].get<int>();
	useGameLink = nv_json["useGameLink"].get<bool>();
	logCombat = nv_json["logCombat"].get<bool>();

	return 0;
}
