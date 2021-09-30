#include "pch.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include "NonVolatile.h"
#include "nlohmann/json.hpp"
#include "HAUtils.h"

static nlohmann::json nv_json = R"(
  {
    "profilePath": "Profiles\\Deathlord.json",
    "hdvPath": "DEATHLORD.HDV",
	"speed":			  0,
	"scanlines":		  false,
	"video":			  0,
    "volumeSpeaker":	  4,
    "volumeMockingBoard": 4,
	"useGameLink":        false,
	"logCombat":		  false
  }
)"_json;

static std::string configfilename = "noxcompanion.conf";

int NonVolatile::SaveToDisk()
{
	std::string sprofPath;
	std::string sHdvPath;
	HA::ConvertWStrToStr(&profilePath, &sprofPath);
	HA::ConvertWStrToStr(&hdvPath, &sHdvPath);

	nv_json["profilePath"]			= sprofPath;
	nv_json["hdvPath"]				= sHdvPath;
	nv_json["speed"]				= speed;
	nv_json["scanlines"]			= scanlines;
	nv_json["video"]				= video;
	nv_json["volumeSpeaker"]		= volumeSpeaker;
	nv_json["volumeMockingBoard"]	= volumeMockingBoard;
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
	HA::ConvertStrToWStr(&_profilePath, &profilePath);
	std::string _hdvPath = nv_json["hdvPath"].get<std::string>();
	HA::ConvertStrToWStr(&_hdvPath, &hdvPath);

	speed = nv_json["speed"].get<int>();
	scanlines = nv_json["scanlines"].get<bool>();
	video = nv_json["video"].get<int>();
	volumeSpeaker = nv_json["volumeSpeaker"].get<int>();
	volumeMockingBoard = nv_json["volumeMockingBoard"].get<int>();
	useGameLink = nv_json["useGameLink"].get<bool>();
	logCombat = nv_json["logCombat"].get<bool>();

	return 0;
}
