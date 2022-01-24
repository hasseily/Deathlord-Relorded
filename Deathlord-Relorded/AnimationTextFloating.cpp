#include "pch.h"
#include "AnimationTextFloating.h"
#include "Descriptors.h"
#include "Game.h"

extern std::unique_ptr<Game>* GetGamePtr();

AnimationTextFloating::AnimationTextFloating(DescriptorHeap* resourceDescriptors, SimpleMath::Vector2 centeredOrigin,
	std::wstring text /*= L""*/, AnimationTextTypes type /*= AnimationTextTypes::Info*/)
{
	m_resourceDescriptors = resourceDescriptors;
	m_renderOrigin = centeredOrigin;
	m_renderCurrent = m_renderOrigin;
	m_tickFrameLength = std::vector<size_t>{	300000, 300000, 300000, 300000, 300000, 300000, 300000,
												300000, 300000, 300000, 300000, 300000, 300000, 300000,
												300000, 300000, 300000, 300000, 300000, 300000, 300000, 300000 };
	m_nextFrameTick = m_tickFrameLength[0];
	m_currentFrame = 0;
	b_isFinished = false;

	auto gamePtr = GetGamePtr();
	m_font = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular);

	SetText(text);

	switch (type)
	{
	case AnimationTextTypes::Info:
		m_color = Colors::AntiqueWhite;
		break;
	case AnimationTextTypes::Damage:
		m_color = Colors::Red;
		break;
	case AnimationTextTypes::Healing:
		m_color = Colors::Green;
		break;
	case AnimationTextTypes::Warning:
		m_color = Colors::LightYellow;
		break;
	default:
		m_color = Colors::Gray;
		break;
	}
}

int AnimationTextFloating::OffsetFromCenter()
{
	auto _sSizeX = XMVectorGetX(m_font->MeasureString(m_text.c_str(), false)) * m_scale;
	return (_sSizeX / 2);
}

void AnimationTextFloating::SetText(std::wstring text)
{
	// Re-offset the origin from center to beginning of string
	m_renderOrigin.x = m_renderOrigin.x + OffsetFromCenter();
	m_text = text;
	m_renderOrigin.x = m_renderOrigin.x - OffsetFromCenter();
	m_renderCurrent.x = m_renderOrigin.x;
}

void AnimationTextFloating::Render(size_t tick, SpriteBatch* spriteBatch)
{
	if (b_isFinished)
		return;
	m_font->DrawString(spriteBatch, m_text.c_str(), m_renderCurrent, m_color, 0.f, { 0,0 }, m_scale);
	if (tick > m_nextFrameTick)		// go to the next frame
	{
		++m_currentFrame;
		if (m_currentFrame == m_tickFrameLength.size())
		{
			b_isFinished = true;
			return;
		}
		m_nextFrameTick = m_tickFrameLength[m_currentFrame] + tick;
		m_color = { {{ m_color[0], m_color[1], m_color[2], 1 - ((float)m_currentFrame / m_tickFrameLength.size()) }} };
		m_renderCurrent.x += (rand() % 4) - 2;		// sway sideways up to 2 pixels for next frame
		m_renderCurrent.y -= 3;
	}

}
