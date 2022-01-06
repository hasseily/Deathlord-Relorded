#pragma once
#include "DeviceResources.h"
#include "Descriptors.h"
#include <vector>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;

enum class MaskSpriteType {			// 1 if mask in adjacent row/col
	TopSolo				= 0b0001,		// left right top bottom
	TopLeft				= 0b0101,
	TopCenter			= 0b1101,
	TopRight			= 0b1001,
	MiddleSolo			= 0b0000,		// a single square mask with no other next to it
	MiddleLeft			= 0b0111,
	MiddleFullCenter	= 0b1111,		// mask on all sides
	MiddleVertCenter	= 0b0011,		// masks above and below
	MiddleHorzCenter	= 0b1100,		// masks left and right
	MiddleRightSolo		= 0b1000,		// mask just to the left
	MiddleLeftSolo		= 0b0100,		// mask just to the right
	MiddleRight			= 0b1011,
	bottomSolo			= 0b0010,
	BottomLeft			= 0b0110,
	BottomCenter		= 0b1110,
	BottomRight			= 0b1010,
	Count
};

// Where to put the pin on the minimap.
constexpr UINT16 MINIMAP_ORIGIN_X = 1600;
constexpr UINT16 MINIMAP_ORIGIN_Y = 42;
constexpr float MINIMAP_X_INCREMENT = 17.5f;
constexpr float MINIMAP_Y_INCREMENT = 20.f;

// Spritesheet info
constexpr UINT8 MINIMAP_SPRITE_WIDTH = 28;
constexpr UINT8 MINIMAP_SPRITE_HEIGHT = 32;
// XY coords of pin
constexpr UINT8 PIN_X = 0;
constexpr UINT8 PIN_Y = 1;

class MiniMap	// Singleton
{
public:
	// Methods
	// Call Update(X,Y) to update the minimap
	// Returns true if this is a new unseen sector of the overland
	bool Update(UINT8 overlandX, UINT8 overlandY);

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

	MaskSpriteType MiniMap::MaskTypeForCoords(UINT8 x, UINT8 y);

	std::vector<UINT8>m_sectorsSeen;
	UINT8 m_overlandX;
	UINT8 m_overlandY;
	UINT8 bShouldRenderNew;		// whether should re-render
	// TODO: use bShouldRenderNew to optimize render to another buf and apply it to the main buf
	std::vector<MaskSpriteType>maskSprites;	// For each vector, choose the sprite to use based on center or sides or corners

	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_MiniMapSpriteSheet;
};
