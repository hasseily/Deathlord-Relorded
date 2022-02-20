#include "pch.h"
#include "AnimationSpriteRotating.h"
#include "Descriptors.h"


AnimationSpriteRotating::AnimationSpriteRotating(DescriptorHeap* resourceDescriptors, 
	XMUINT2 spriteSheetSize, Vector2 spriteSheetOrigin, int width, int height, float rotationsPerSecond) :
	AnimationSprite(resourceDescriptors, spriteSheetSize,
		spriteSheetOrigin, width, height)
{
	auto gamePtr = GetGamePtr();
	m_startTick = (*gamePtr)->GetTotalTicks();
	m_rotationsPerSecond = rotationsPerSecond;
	m_nextFrameTick = m_startTick + TICKS_30FPS;
	m_frameCount = 1;
	m_loopsRemaining = 1;
	m_rotation = 0.f;
	m_maxRotations = 1;
	m_center = XMFLOAT2();
	m_tint = Colors::White;
}

void AnimationSpriteRotating::Render(SpriteBatch* spriteBatch, RECT* overlayRect)
{
	if (b_isFinished)
		return;

	// Use neither the overlayRect nor m_renderCurrent
	m_renderCurrent = m_renderOrigin;
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)m_textureDescriptor),
		m_spriteSheetSize, m_renderOrigin, &(RECT)m_frameRectangles.at(0),
		m_tint, m_rotation, m_center, m_scale);

	// Finalize
	auto gamePtr = GetGamePtr();
	auto tick = (*gamePtr)->GetTotalTicks();
	auto maxRot = m_maxRotations * (2 * M_PI);

	if (tick > m_nextFrameTick)		// go to the next frame
	{
		// update m_rotation
		m_rotation = (tick - m_startTick) * (2 * M_PI) * m_rotationsPerSecond / 10000000;
		// make the tint -> transparent in the last few frames before the end
		if (m_rotation < maxRot)
		{
			m_nextFrameTick = tick + TICKS_30FPS;
			if ((m_rotation / maxRot) > 0.95f)
			{
				m_tint.f[3] *= (1 - m_rotation / maxRot) / 0.05f;
			}
		}
		else
		{
			b_isFinished = true;
		}
	}
}
