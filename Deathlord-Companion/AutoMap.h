#pragma once

#include "DeviceResources.h"
#include <map>

using namespace DirectX;
using namespace DirectX::SimpleMath;

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
	void UpdateAvatarPositionOnAutoMap(UINT8 x, UINT8 y);
	void CreateNewTileSpriteMap();

	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload);
	void OnDeviceLost();
	bool LoadTextureFromMemory(const unsigned char* image_data, Microsoft::WRL::ComPtr <ID3D12Resource>* out_tex_resource,
		DXGI_FORMAT tex_format, int width, int height);

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

	AutoMap(std::unique_ptr<DX::DeviceResources>& deviceResources, 
		std::unique_ptr<DirectX::DescriptorHeap>&  resourceDescriptors)
	{
		m_currentMapRect = { 0,0,0,0 };
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		CreateNewTileSpriteMap();
	}

	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_autoMapTextureBG;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_autoMapTexture;
	D3D12_SUBRESOURCE_DATA m_textureDataMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUploadHeapMap;

	std::map<UINT32, UINT8> m_currentMapTiles;	// All mapPos -> tileid for the whole map
												// to ensure we don't redraw what's already there

	RECT m_currentMapRect;
};

