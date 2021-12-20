#pragma once
#include <filesystem>
#include "SidebarManager.h"
#include "nlohmann/json.hpp"
#include <map>
#include "StepTimer.h"

constexpr UINT8 SIDEBAR_MAX_VARS_IN_BLOCK = 255;

/// <summary>
/// SidebarContent is responsible for managing the json profiles
/// and generating the dynamic text from the Apple 2 memory
/// </summary>

class SidebarContent
{
public:
	SidebarContent::SidebarContent();
	void Initialize();
	void LoadProfileUsingDialog(SidebarManager* sbM);
	bool setActiveProfile(SidebarManager* sbM, std::string* name);
	std::string OpenProfile(std::filesystem::directory_entry entry);
	std::string OpenProfile(std::wstring entry);
	void ClearActiveProfile(SidebarManager* sbM);
	UINT64 UpdateAllSidebarText(SidebarManager* sbM, bool forceUpdate, UINT64 maxMicroSecondsAllowed = 1000);
	bool UpdateBlock(SidebarManager* sbM, UINT8 sidebarId, UINT8 blockId, nlohmann::json* pdata);
	int nextSidebarToRender;
	int nextBlockToRender;
private:
	void LoadProfilesFromDisk();
	nlohmann::json ParseProfile(std::filesystem::path filepath);
	std::string SerializeVariable(nlohmann::json* pvar);
	std::string FormatBlockText(nlohmann::json* pdata);

	std::map<std::string, nlohmann::json> m_allProfiles;
	nlohmann::json m_activeProfile;
};

