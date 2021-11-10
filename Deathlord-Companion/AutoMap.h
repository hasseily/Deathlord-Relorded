#pragma once

#include "DeviceResources.h"
#include "Emulator/AppleWin.h"
#include <map>

using namespace DirectX;
using namespace DirectX::SimpleMath;

constexpr UINT32 GAMEMAP_START_MEM = 0xC00;					// Start of memory area of the in-game map
constexpr UINT32 GAMEMAP_START_CURRENT_TILELIST = 0x300;	// Start of the current tilelist of the in-game map
constexpr UINT32 GAMEMAP_START_NEW_TILELIST = 0x351;		// Start of the new tilelist of the in-game map which will be swapped with current on an update

constexpr UINT8 MAP_WIDTH = 64;
constexpr UINT8 MAP_HEIGHT = 64;
constexpr int MAP_LENGTH = MAP_WIDTH * MAP_HEIGHT;			// Size of map (in bytes)

// This is the description of what every bit in the FogOfWar vector does
// Each tile on the map has a FogOfWar descriptive byte with this info
enum class FogOfWarMarkers
{
	UnFogOfWar = 0,	// Set the tile has been seen by the player
	Footstep,		// Set if the player has walked on the tile
	Bit3,
	Bit4,
	Bit5,
	Bit6,
	Bit7,
	Bit8
};


/// Summary:
/// This class handles the management of the automapper.
/// It doesn't deal with the tileset(s), just the maps themselves.
/// It assumes the tileset is correct for the current level
/// Upon demand, the automapper should generate the whole level's image
/// from the current tiles in memory and the map of IDs in memory.
/// That image can then be used as the source for the GPU texture to display
/// 
/// The automapper should also not only provide the level's image,
/// but the level size and level origin in case the map merges multiple levels together
/// like in a dungeon or tower (4 levels of 16x16 in a single 64x63 map)
/// 
/// Depending on a FogOfWar flag, the automapper should only draw on the map the tiles that:
/// - have been seen before
/// - are not black on the main Apple2Video texture (i.e. seen now)
/// 
/// Hence, the automapper should remember for each level all the seen tiles and store/load as necessary
/// 
/// Finally, the automapper should provide the player's X/Y position on that map

class AutoMap	// Singleton
{
public:
	// Vector2 drawOrigin = Vector2();
	void DrawAutoMap(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, RECT* mapRect);
	void UpdateAvatarPositionOnAutoMap(UINT x, UINT y);
	void ClearMapArea();
	void ForceRedrawMapArea();
	void AnalyzeVisibleTiles();
	void CreateNewTileSpriteMap();
	void SaveCurrentMapInfo();
	void InitializeCurrentMapInfo();
	std::string GetCurrentMapUniqueName();
	LPBYTE GetCurrentGameMap() { return MemGetMainPtr(GAMEMAP_START_MEM); };

	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload);
	void OnDeviceLost();
	bool LoadTextureFromMemory(const unsigned char* image_data, Microsoft::WRL::ComPtr <ID3D12Resource>* out_tex_resource,
		DXGI_FORMAT tex_format, int width, int height);


	void SetShowTransition(bool showTransition);
	bool isInTransition() { return bShowTransition; };

	// public singleton code
	static AutoMap* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
								std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new AutoMap(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static AutoMap* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~AutoMap()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	static AutoMap* s_instance;
	bool bShowTransition;
	XMUINT2 m_avatarPosition;

	AutoMap(std::unique_ptr<DX::DeviceResources>& deviceResources, 
		std::unique_ptr<DirectX::DescriptorHeap>&  resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}

	void Initialize();
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_autoMapTextureBG;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_autoMapTexture;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_autoMapAvatar;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_autoMapSpriteSheet;
	D3D12_SUBRESOURCE_DATA m_textureDataMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUploadHeapMap;

	RECT m_currentMapRect;	// Rect of the currently drawn map, as requested by the game engine

	std::vector <std::vector<UINT8>> m_bbufCurrentMapTiles;	// All tileid for the whole map ordered by mapPos
															// to ensure we don't redraw what's already there
															// The number of vectors will be equal to the backbuffer count

	std::vector<std::vector<UINT8>> m_bbufFogOfWarTiles;	// Info about the user having seen map tiles, walked on them...
															// The number of vectors will be equal to the backbuffer count

	std::string m_currentMapUniqueName;
};

