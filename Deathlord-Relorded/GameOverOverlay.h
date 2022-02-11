#pragma once

#include "Overlay.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

class GameOverOverlay : public Overlay
{
public:

	void Initialize();		// reinitialized on a reboot
	void Update();
	void Render(SimpleMath::Rectangle r);

	int m_monstersKilled = 0;
	float m_transitionTime;   // in seconds

	// public singleton code
	static GameOverOverlay* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new GameOverOverlay(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static GameOverOverlay* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}

private:

	float m_scale;
	static GameOverOverlay* s_instance;
	std::wstring m_line1;
	std::wstring m_line2;
	std::wstring m_line3;

	GameOverOverlay(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		Initialize();
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
	}

};

