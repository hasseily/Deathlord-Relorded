#include "pch.h"
#include "Animation.h"


#pragma region Animation

void Animation::SetRenderOrigin(SimpleMath::Vector2 origin)
{
	m_renderOrigin = origin;
	m_renderCurrent = origin;
}

void Animation::SetFrameCount(int frameCount, int frameLength)
{
	m_frameCount = frameCount;
	m_tickFrameLength.clear();
	for (size_t i = 0; i < frameCount; i++)
	{
		m_tickFrameLength.emplace_back(frameLength);
	}
}

void Animation::StartAnimation()
{
	m_currentFrame = 0;
	ResumeAnimation();
	return;
}

void Animation::StopAnimation()
{
	m_nextFrameTick = SIZE_T_MAX;
}

void Animation::ResumeAnimation()
{
	auto gamePtr = GetGamePtr();
	auto tick = (*gamePtr)->GetTotalTicks();
	m_nextFrameTick = tick + m_tickFrameLength[m_currentFrame];
}

#pragma endregion
