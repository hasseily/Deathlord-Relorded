#pragma once

#include "DeviceResources.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

class InvOverlay	// Singleton
{
public:
	void DrawInvOverlay(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, RECT* overlayRect);
	void ShowInvOverlay();
	void HideInvOverlay();
	bool IsInvOverlayDisplayed();

	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload);
	void OnDeviceLost();

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
	static InvOverlay* s_instance;
	bool bIsDisplayed;

	InvOverlay(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}

	void Initialize();
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_inventoryTextureBG;

	RECT m_currentRect;	// Rect as requested by the game engine
};