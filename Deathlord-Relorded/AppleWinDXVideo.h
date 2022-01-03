#pragma once
#include "DeviceResources.h"
#include "Descriptors.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;


// Where to put the pin on the AppleWinDXVideo.
constexpr UINT16 APPLEWINDXVIDEO_ORIGIN_X = 1297;
constexpr UINT16 APPLEWINDXVIDEO_ORIGIN_Y = 42;

constexpr UINT8 APPLEWINDXVIDEO_SPRITE_WIDTH = 28;
constexpr UINT8 APPLEWINDXVIDEO_SPRITE_HEIGHT = 32;

class AppleWinDXVideo	// Singleton
{
public:
	// Methods
	
	void ShowApple2Video();
	void HideApple2Video();
	void ToggleApple2Video();
	bool IsApple2VideoDisplayed();

	// Call Render() at the end of the rendering stage to draw the AppleWinDXVideo stuff
	// r is the rectangle of the game itself
	// Note: AppleWinDXVideo needs uploadBatch because on every frame it reuploads its texture
	void Render(SimpleMath::Rectangle r,
		DirectX::ResourceUploadBatch* uploadBatch);
	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload);
	void OnDeviceLost();

	// Properties

	// public singleton code
	static AppleWinDXVideo* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new AppleWinDXVideo(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static AppleWinDXVideo* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~AppleWinDXVideo()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();

	static AppleWinDXVideo* s_instance;
	AppleWinDXVideo(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}

	bool bIsDisplayed;

	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	D3D12_SUBRESOURCE_DATA m_AppleWinTextureData; // AppleWin video data in memory
	Microsoft::WRL::ComPtr<ID3D12Resource> m_AppleWinDXVideoSpriteSheet;	// Same in GPU
	Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUploadHeapMap;	// For uploading the texture
	// It's totally independent, and uses its own DTX pieces
	std::unique_ptr<CommonStates>m_states;
	std::unique_ptr<SpriteBatch>m_spriteBatch;
	std::unique_ptr<PrimitiveBatch<VertexPositionColor>>m_primitiveBatch;
	std::unique_ptr<BasicEffect> m_dxtEffect;
};

