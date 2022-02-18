#include "pch.h"
#include "Animation.h"


#pragma region Animation

void Animation::SetRenderOrigin(SimpleMath::Vector2 origin)
{
	m_renderOrigin = origin;
	m_renderCurrent = origin;
}

void Animation::StartAnimation()
{
	m_frameCount = 0;
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
