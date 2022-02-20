#include "pch.h"
#include "AnimationSprite.h"
#include "TilesetCreator.h"
#include "StepTimer.h"
#include "Descriptors.h"

#pragma region AnimationSprite

constexpr int SPRITE_WIDTH = 155;
constexpr int SPRITE_HEIGHT = 190;

AnimationSprite::AnimationSprite(DescriptorHeap* resourceDescriptors, XMUINT2 spriteSheetSize,
	Vector2 spriteSheetOrigin, int width, int height, int frameCount)
{
	m_resourceDescriptors = resourceDescriptors;
	m_spriteSheetSize = spriteSheetSize;
	m_width = width;
	m_height = height;
	m_frameCount = frameCount;
	m_tickFrameLength = std::vector<size_t>(m_frameCount, TICKS_30FPS);
	m_spriteSheetOrigin = spriteSheetOrigin;
	m_loopsRemaining = 1;
	m_scale = 1.f;

	m_currentFrame = 0;
	m_nextFrameTick = m_tickFrameLength[0];
	b_isFinished = false;
	m_frameRectangles = std::vector<SimpleMath::Rectangle>();
	for (size_t i = 0; i < m_frameCount; i++)
	{
		m_frameRectangles.emplace_back(m_spriteSheetOrigin.x + i * m_width, m_spriteSheetOrigin.y, m_width, m_height);
	}
	SetRenderOrigin(SimpleMath::Vector2());
}

void AnimationSprite::SetTotalAnimationLength(float seconds)
{
	m_tickFrameLength.clear();
	for (size_t i = 0; i < m_frameCount; i++)
	{
		m_tickFrameLength.emplace_back(10000000 * seconds / m_frameCount);
	}
}
// TODO: FIX THIS. The sprite origin and width/height are set at creation, and don't allow for passing args.
void AnimationSprite::SetSpriteSheetOrigin(Vector2 origin)
{
	m_spriteSheetOrigin = origin;
	for (size_t i = 0; i < m_frameCount; i++)
	{
		m_frameRectangles.emplace_back(m_spriteSheetOrigin.x + i * m_width, m_spriteSheetOrigin.y, m_width, m_height);
	}
}

void AnimationSprite::Render(SpriteBatch* spriteBatch, RECT* overlayRect)
{
	if (b_isFinished)
		return;

	// Use neither the overlayRect nor m_renderCurrent
	m_renderCurrent = m_renderOrigin;

	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)m_textureDescriptor),
		m_spriteSheetSize, m_renderOrigin, &(RECT)m_frameRectangles.at(m_currentFrame), Colors::White, 0.f, XMFLOAT2(), m_scale);

	// Finalize
	auto gamePtr = GetGamePtr();
	auto tick = (*gamePtr)->GetTotalTicks();

	if (tick > m_nextFrameTick)		// go to the next frame
	{
		m_currentFrame = (m_currentFrame + 1) % m_frameCount;
		m_nextFrameTick = m_tickFrameLength[m_currentFrame] + tick;
		if (m_currentFrame == (m_frameCount - 1))
		{
			--m_loopsRemaining;
			if (m_loopsRemaining == 0)
				b_isFinished = true;
		}
	}
}
#pragma endregion AnimationSprite
