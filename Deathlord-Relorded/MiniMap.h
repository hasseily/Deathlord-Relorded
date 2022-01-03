#pragma once
#include "DeviceResources.h"
#include "Descriptors.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;


// Where to put the pin on the minimap.
constexpr UINT16 MINIMAP_ORIGIN_X = 1297;
constexpr UINT16 MINIMAP_ORIGIN_Y = 42;
constexpr float MINIMAP_X_INCREMENT = 17.5f;
constexpr float MINIMAP_Y_INCREMENT = 20.f;

constexpr UINT8 MINIMAP_SPRITE_WIDTH = 28;
constexpr UINT8 MINIMAP_SPRITE_HEIGHT = 32;

class MiniMap	// Singleton
{
public:
	// Methods
	// Call Render() at the end of the rendering stage to draw the MiniMap stuff
	// r is the rectangle of the game itself
	void Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch);
	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload);
	void OnDeviceLost();

	// Properties

	// public singleton code
	static MiniMap* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new MiniMap(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static MiniMap* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~MiniMap()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();

	static MiniMap* s_instance;
	MiniMap(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_MiniMapSpriteSheet;
};
