#pragma once

#include "DeviceResources.h"
#include "Descriptors.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{	// shaders
	struct VS_INTERFERENCE_PARAMETERS
	{
		float deltaT;	// 0-1, time delta: 0 is start, 1 is end
		float barThickness;
		float maxInterference;
		uint8_t na[4];
	};
	static_assert(!(sizeof(VS_INTERFERENCE_PARAMETERS) % 16),
		"VS_INTERFERENCE_PARAMETERS needs to be 16 bytes aligned");
}

enum class OverlayType
{
	Bare = 0,
	Bordered
};

enum class OverlayState
{
	Hidden = 0,
	TransitionIn,
	Displayed,
	TransitionOut
};

class Overlay\
{
public:
	void Render(SimpleMath::Rectangle r);	// To override
	void ShowOverlay();
	void HideOverlay();
	void ToggleOverlay();
	bool ShouldRenderOverlay();
	bool IsOverlayDisplayed();
	void Update();
	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload, CommonStates* states);
	void OnDeviceLost();


	~Overlay()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
protected:
	void PreRender(SimpleMath::Rectangle r);
	void PostRender(SimpleMath::Rectangle r);
	void Initialize();

	OverlayType m_type = OverlayType::Bare;
	bool bShouldDisplay;
	OverlayState m_state;
	bool bShouldBlockKeystrokes = false;
	RECT m_currentRect;	// Rect of the overlay
	int m_width;
	int m_height;
	XMFLOAT4 m_curtainColor = { 0.f, 0.f, 0.f, 0.f };
	TextureDescriptors m_spritesheetDescriptor;
	std::wstring m_spritesheetPath;
	VS_INTERFERENCE_PARAMETERS m_shaderParameters;

	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_overlaySpriteSheet;
	// It's totally independent, and uses its own DTX pieces
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSigDefault;
	std::unique_ptr<SpriteBatch>m_spriteBatch;
	std::unique_ptr<PrimitiveBatch<VertexPositionColor>>m_primitiveBatch;
	std::unique_ptr<BasicEffect> m_dxtEffect;
	DirectX::GraphicsResource m_shaderParamsResource;
};
