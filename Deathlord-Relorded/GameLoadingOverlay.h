#pragma once

#include "Overlay.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

class GameLoadingOverlay : public Overlay
{
public:

	void Initialize();		// reinitialized on a reboot
	void Update();
	void Render(SimpleMath::Rectangle r);

	float m_transitionTime;   // in seconds

	// public singleton code
	static GameLoadingOverlay* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new GameLoadingOverlay(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static GameLoadingOverlay* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}

private:

	float m_scale;
	static GameLoadingOverlay* s_instance;

	GameLoadingOverlay(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		Initialize();
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
	}

};

