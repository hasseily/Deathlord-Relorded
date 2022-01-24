#pragma once
#include "DeviceResources.h"
#include "Descriptors.h"
#include "AnimationTextFloating.h"

class AnimTextManager
{
public:
	AnimationTextFloating* CreateAnimation(SimpleMath::Vector2 centeredOrigin,
		std::wstring text = L"", AnimationTextTypes type = AnimationTextTypes::Info);
	void RenderAnimations(SpriteBatch* spriteBatch);

	// public singleton code
	static AnimTextManager* GetInstance(DX::DeviceResources* deviceResources, DirectX::DescriptorHeap* resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new AnimTextManager(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static AnimTextManager* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~AnimTextManager()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();

	static AnimTextManager* s_instance;
	AnimTextManager(DX::DeviceResources* deviceResources, DirectX::DescriptorHeap* resourceDescriptors)
	{
		m_deviceResources = deviceResources;
		m_resourceDescriptors = resourceDescriptors;
		Initialize();
	}

	std::vector<AnimationTextFloating> m_animations;

	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
};

