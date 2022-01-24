#include "pch.h"
#include "AnimTextManager.h"
#include "AnimationTextFloating.h"
#include <vector>
#include "Game.h"
#include "HAUtils.h"

extern std::unique_ptr<Game>* GetGamePtr();
extern HWND g_hFrameWindow;

// below because "The declaration of a static data member in its class definition is not a definition"
AnimTextManager* AnimTextManager::s_instance;

void AnimTextManager::Initialize()
{
	m_animations = std::vector<AnimationTextFloating>();
}

AnimationTextFloating* AnimTextManager::CreateAnimation(SimpleMath::Vector2 centeredOrigin, 
	std::wstring text /*= L""*/, AnimationTextTypes type /*= AnimationTextTypes::Info*/)
{
	try
	{
		AnimationTextFloating _anim = AnimationTextFloating(m_resourceDescriptors, centeredOrigin, text, type);
		m_animations.push_back(_anim);
		return &m_animations.back();
	}
	catch (const std::exception&)
	{
		HA::AlertIfError(g_hFrameWindow);
		return nullptr;
	}
}

void AnimTextManager::RenderAnimations(SpriteBatch* spriteBatch)
{
	auto _toDelete = std::vector<size_t>();
	for (size_t i = 0; i < m_animations.size(); i++)
	{
		if (m_animations[i].IsFinished())
		{
			_toDelete.push_back(i);
			continue;
		}
		m_animations[i].Render(spriteBatch);
	}
	for (size_t i = 0; i < _toDelete.size(); i++)
	{
		_toDelete.erase(_toDelete.begin() + i);
	}
}
