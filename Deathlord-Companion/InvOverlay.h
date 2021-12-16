#pragma once

#include "DeviceResources.h"
#include <array>
#include <string>
#include "InvItem.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

class InvOverlay	// Singleton
{
public:
	void DrawInvOverlay(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, std::shared_ptr<DirectX::PrimitiveBatch<VertexPositionColor>>& primitiveBatch, RECT* overlayRect);
	void ShowInvOverlay();
	void HideInvOverlay();
	bool IsInvOverlayDisplayed();
	void UpdateState();
	void LeftMouseButtonClicked(int x, int y);
	void MousePosInPixels(int x, int y);

	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload);
	void OnDeviceLost();

	XMFLOAT4 ColorAmber = { 0.5f, 0.2f, 0.f, 1.000000000f };
	XMVECTORF32 VColorAmber = { { { 0.5f, 0.2f, 0.f, 1.000000000f } } };
	XMFLOAT4 ColorAmberDark = { 0.25f, 0.1f, 0.f, 1.000000000f };
	XMVECTORF32 VColorAmberDark = { { { 0.25f, 0.1f, 0.f, 1.000000000f } } };

	// public singleton code
	static InvOverlay* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new InvOverlay(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static InvOverlay* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~InvOverlay()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();
	void DrawItem(InvItem* item, UINT8 charges, std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, DirectX::SpriteFont* font, int* xCol, int* yCol);

	static InvOverlay* s_instance;
	bool bIsDisplayed;
	RECT m_currentRect;	// Rect of the overlay

	InvOverlay(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_invOverlaySpriteSheet;
};
