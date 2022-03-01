#pragma once
#include "DeviceResources.h"
#include "Descriptors.h"
#include "AnimationSprite.h"
#include "AnimationSpriteRotating.h"

class AnimSpriteManager
{
public:
	AnimationSprite* CreateAnimation(TextureDescriptors td, XMUINT2 spriteSheetSize,
		Vector2 spriteSize, Vector2 spriteSheetOrigin,
		int frameCount, Vector2 renderOrigin);
	AnimationSpriteRotating* AnimSpriteManager::CreateRotatingAnimation(TextureDescriptors td, XMUINT2 spriteSheetSize,
		Vector2 spriteSize, Vector2 spriteSheetOrigin, XMFLOAT2 center,
		int maxRotations, float rotationsPerSecond, Vector2 renderOrigin);

	void RenderAnimations(SpriteBatch* spriteBatch);
	void StopAllAnimations();

	// public singleton code
	static AnimSpriteManager* GetInstance(DX::DeviceResources* deviceResources, DirectX::DescriptorHeap* resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new AnimSpriteManager(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static AnimSpriteManager* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~AnimSpriteManager()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();

	static AnimSpriteManager* s_instance;
	AnimSpriteManager(DX::DeviceResources* deviceResources, DirectX::DescriptorHeap* resourceDescriptors)
	{
		m_deviceResources = deviceResources;
		m_resourceDescriptors = resourceDescriptors;
		Initialize();
	}

	std::vector<std::shared_ptr<AnimationSprite>> m_animations;
	std::vector< std::shared_ptr<AnimationSpriteRotating>> m_animationsRotating;

	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
};

