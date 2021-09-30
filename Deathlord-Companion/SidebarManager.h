#pragma once
#include <vector>
#include "Sidebar.h"

/// <summary>
/// Sidebar Manager creates sidebars and determines their width and height, and number of items
/// Sidebar Manager determines where each sidebar is located, and handles their origin point
/// </summary>

constexpr int SIDEBAR_LR_WIDTH = 200;		// Sidebar Left/Right width
constexpr int SIDEBAR_TB_HEIGHT = 100;		// Sidebar TOB/BOTTOM height
constexpr UINT8 SIDEBARS_MAX_COUNT = 10;	// Max number of sidebars possible

class SidebarManager
{

public:
	SidebarManager();
	SidebarError CreateSidebar(SidebarTypes type, UINT8 numBlocks, UINT16 size, __out UINT8* id);
	SidebarError ClearSidebar(UINT8 id);
	void ClearAllSidebars();
	void DeleteAllSidebars();

	static void GetBaseSize(__out int& width, __out int& height) noexcept;
	static float GetAspectRatio() noexcept;
	// Sets the required size of the client frame to include all the sidebars, given the default APPLEWIN WIDTH and HEIGHT
	// Everything later scales based on the user's window resizing in Game.cpp
	void SetBaseSize(const int width, const int height) noexcept;

	// Properties
	std::vector<std::wstring> fontsAvailable;	// TODO: Redundant with enum FontDescriptors???
	std::vector<Sidebar> sidebars;
};

static std::wstring s2ws(const std::string& s);


