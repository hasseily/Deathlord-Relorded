#pragma once

#include "DeviceResources.h"
#include "Descriptors.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

enum class OverlayType
{
	Bare = 0,
	Bordered
};


class Overlay\
{
public:
	void Render(SimpleMath::Rectangle r);	// To override
	void ShowOverlay();
	void HideOverlay();
	void ToggleOverlay();
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
	bool bIsDisplayed;
	bool bShouldBlockKeystrokes = false;
	RECT m_currentRect;	// Rect of the overlay
	int m_width;
	int m_height;
	XMFLOAT4 m_curtainColor = { 0.f, 0.f, 0.f, 0.f };
	TextureDescriptors m_spritesheetDescriptor;
	std::wstring m_spritesheetPath;

	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_overlaySpriteSheet;
	// It's totally independent, and uses its own DTX pieces
	std::unique_ptr<SpriteBatch>m_spriteBatch;
	std::unique_ptr<PrimitiveBatch<VertexPositionColor>>m_primitiveBatch;
	std::unique_ptr<BasicEffect> m_dxtEffect;
};
