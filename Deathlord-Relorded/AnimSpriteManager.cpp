#include "pch.h"
#include "AnimSpriteManager.h"
#include <vector>
#include "Game.h"
#include "HAUtils.h"

extern std::unique_ptr<Game>* GetGamePtr();
extern HWND g_hFrameWindow;

// below because "The declaration of a static data member in its class definition is not a definition"
AnimSpriteManager* AnimSpriteManager::s_instance;

void AnimSpriteManager::Initialize()
{
	m_animations = std::vector<std::shared_ptr<AnimationSprite>>();
	m_animationsRotating = std::vector<std::shared_ptr<AnimationSpriteRotating>>();
}

AnimationSprite* AnimSpriteManager::CreateAnimation(TextureDescriptors td, XMUINT2 spriteSheetSize,
	Vector2 spriteSize, Vector2 spriteSheetOrigin, int frameCount, Vector2 renderOrigin)
{
	try
	{
		auto _anim = std::make_shared<AnimationSprite>(m_resourceDescriptors, spriteSheetSize,
			spriteSheetOrigin, spriteSize.x, spriteSize.y, frameCount);
		_anim->m_textureDescriptor = td;
		_anim->SetRenderOrigin(renderOrigin);
		m_animations.push_back(_anim);
		return _anim.get();
	}
	catch (const std::exception&)
	{
		HA::AlertIfError(g_hFrameWindow);
		return nullptr;
	}
}

AnimationSpriteRotating* AnimSpriteManager::CreateRotatingAnimation(TextureDescriptors td, XMUINT2 spriteSheetSize,
	Vector2 spriteSize, Vector2 spriteSheetOrigin, XMFLOAT2 center, int maxRotations, 
	float rotationsPerSecond, Vector2 renderOrigin)
{
	try
	{
		auto _anim = std::make_shared<AnimationSpriteRotating>(m_resourceDescriptors, spriteSheetSize,
			spriteSheetOrigin, spriteSize.x, spriteSize.y, rotationsPerSecond);
		_anim->m_textureDescriptor = td;
		_anim->SetRenderOrigin(renderOrigin);
		_anim->m_maxRotations = maxRotations;
		_anim->m_center = center;
		m_animationsRotating.push_back(_anim);
		return _anim.get();
	}
	catch (const std::exception&)
	{
		HA::AlertIfError(g_hFrameWindow);
		return nullptr;
	}
}

void AnimSpriteManager::RenderAnimations(SpriteBatch* spriteBatch)
{
	for (size_t i = 0; i < m_animations.size(); i++)
	{
		m_animations[i]->Render(spriteBatch);
		if (m_animations[i]->IsFinished())
		{
			m_animations[i].reset();
			continue;
		}
	}
	// clear out the nullptr animations that were finished above
	m_animations.erase(std::remove(m_animations.begin(), m_animations.end(), nullptr),
		m_animations.end());

	// Same for animationsRotating
	for (size_t i = 0; i < m_animationsRotating.size(); i++)
	{
		m_animationsRotating[i]->Render(spriteBatch);
		if (m_animationsRotating[i]->IsFinished())
		{
			m_animationsRotating[i].reset();
			continue;
		}
	}
	// clear out the nullptr animations that were finished above
	m_animationsRotating.erase(std::remove(m_animationsRotating.begin(), m_animationsRotating.end(), nullptr), 
		m_animationsRotating.end());
}
