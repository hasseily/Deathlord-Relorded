#pragma once

#include "DeviceResources.h"
#include "Emulator/AppleWin.h"
#include "Descriptors.h"
#include <map>

using namespace DirectX;
using namespace DirectX::SimpleMath;

constexpr UINT8 DEFAULT_TILES_PER_ROW = 16;		// tileset tiles per row in general

constexpr UINT32 GAMEMAP_START_MEM = 0xC00;					// Start of memory area of the in-game map (with monsters, see below)
constexpr UINT32 GAMEMAP_START_CURRENT_TILELIST = 0x300;	// Start of the current tilelist of the in-game map
constexpr UINT32 GAMEMAP_START_NEW_TILELIST = 0x351;		// Start of the new tilelist of the in-game map which will be swapped with current on an update

// This is the list of 16 monsters that are in the current level
// Each id is the monster position in the monster spritesheet
// The 16 monsters are put in the level tilemap starting at 0x4800
// While the tilemap starts at 0x4000.
constexpr UINT32 GAMEMAP_START_MONSTERS_IN_LEVEL_IDX = 0x8EF;

// Everything the game uses to track monsters on the map
// The map at GAMEMAP_START_MEM has monster IDs as tiles
// Below is the info needed to swap back the original tile IDs as the monsters move
constexpr UINT8 GAMEMAP_ARRAY_MONSTER_SIZE = 32;				// Max # of tracked monsters in the level
constexpr UINT16 GAMEMAP_ARRAY_MONSTER_POSITION_X = 0x800;		// Empty slot when FF
constexpr UINT16 GAMEMAP_ARRAY_MONSTER_POSITION_Y = 0x820;		// Empty slot when FF
constexpr UINT16 GAMEMAP_ARRAY_MONSTER_ID = 0x840;
constexpr UINT16 GAMEMAP_ARRAY_MONSTER_TILE_ID = 0x860;


// Animation tiles
constexpr UINT16 TILESET_ANIMATIONS_ACID_Y_IDX= 0;
constexpr UINT16 TILESET_ANIMATIONS_FIRE_Y_IDX= 1;
constexpr UINT16 TILESET_ANIMATIONS_WATER_Y_IDX = 2;
constexpr UINT16 TILESET_ANIMATIONS_MAGIC_Y_IDX = 3;
constexpr UINT16 TILESET_ANIMATIONS_FORCE_Y_IDX = 4;

constexpr UINT8 MAP_WIDTH = 64;
constexpr UINT8 MAP_HEIGHT = 64;
constexpr int MAP_LENGTH = MAP_WIDTH * MAP_HEIGHT;			// Size of map (in bytes)

// This is the description of what every bit in the FogOfWar vector does
// Each tile on the map has a FogOfWar descriptive byte with this info
enum class FogOfWarMarkers
{
	UnFogOfWar = 0,	// Set the tile has been seen by the player
	Footstep,		// Set if the player has walked on the tile
	Hidden,			// Set if the player has identified the hidden layer (illusion, poison water, etc...)
	Bit4,
	Bit5,
	Bit6,
	Bit7,
	Bit8
};

enum class MapType	// Based on MAP_TYPE memory address at 0xFC04
{
	Town = 0,
	Overland = 1,
	Dungeon = 2,
	count
};

enum class PartyIconType	// What to draw on the party icon tile
{
	Regular = 0,
	Boat = 1,
	Pit = 2
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
/// 
/// PS:
/// There's ConditionallyDisplayHiddenLayerAroundPlayer() which will display the layer of pits, illusions...
/// based on the composition of the party, when the party is 1 tile away from it. For example, thieves and 
/// rangers will see pits and hidden doors

class AutoMap	// Singleton
{
public:
	// Vector2 drawOrigin = Vector2();
	void DrawAutoMap(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, DirectX::CommonStates* states, RECT* mapRect);

	void ClearMapArea();
	void ForceRedrawMapArea();
	void CalcTileVisibility(bool force = false);
	void ShouldCalcTileVisibility();	// Ask to recalc tile visibility on next update
	void ConditionallyDisplayHiddenLayerAroundPlayer(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, DirectX::CommonStates* states);
	void CreateNewTileSpriteMap();	// Obsolete. Disabled
	void SaveCurrentMapInfo();
	void InitializeCurrentMapInfo();
	std::string GetCurrentMapUniqueName();
	LPBYTE GetCurrentGameMap() { return MemGetMainPtr(GAMEMAP_START_MEM); };
	bool UpdateLOSRadius();
	UINT8 StaticTileIdAtMapPosition(UINT8 x, UINT8 y);	// (the tile that's hidden by the monster being on it)
	ID3D12Resource* GetMonsterSpriteSheet() { return m_monsterSpriteSheet.Get(); };

	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload);
	void OnDeviceLost();
	bool LoadTextureFromMemory(const unsigned char* image_data, Microsoft::WRL::ComPtr <ID3D12Resource>* out_tex_resource,
		DXGI_FORMAT tex_format, TextureDescriptors tex_desc, int width, int height);


	void SetShowTransition(bool showTransition);
	bool IsInTransition() { return bShowTransition; };

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
	bool UpdateAvatarPositionOnAutoMap(UINT x, UINT y);
	void CalculateLOS();
	void DrawLine(int x0, int y0, int x1, int y1);
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_autoMapTextureBG;		// Transition image
	Microsoft::WRL::ComPtr<ID3D12Resource> m_autoMapTexture;		// Obsolete: the original in-game tilemap
	Microsoft::WRL::ComPtr<ID3D12Resource> m_tilesOverland;			// The 64 overland tiles
	Microsoft::WRL::ComPtr<ID3D12Resource> m_tilesDungeon;			// The 64 town/dungeon tiles
	Microsoft::WRL::ComPtr<ID3D12Resource> m_tilesAnimated;			// The elements tiles that are animated
	Microsoft::WRL::ComPtr<ID3D12Resource> m_monsterSpriteSheet;	// The 64 town/dungeon tiles
	Microsoft::WRL::ComPtr<ID3D12Resource> m_autoMapSpriteSheet;	// Overlay sprites for hidden etc...
	D3D12_SUBRESOURCE_DATA m_textureDataMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUploadHeapMap;

	RECT m_currentMapRect;	// Rect of the currently drawn map, as requested by the game engine

	std::vector <std::vector<UINT8>> m_bbufCurrentMapTiles;	// All tileid for the whole map ordered by mapPos
															// to ensure we don't redraw what's already there
															// The number of vectors will be equal to the backbuffer count

	std::vector<UINT8> m_FogOfWarTiles;		// Info about the user having seen map tiles, walked on them...
	std::vector<float> m_LOSVisibilityTiles;	// visibility level of all the tiles on the map given the line of sight
	UINT8 m_LOSRadius;
	bool m_shouldCalcTileVisibility;

	std::string m_currentMapUniqueName;
};

