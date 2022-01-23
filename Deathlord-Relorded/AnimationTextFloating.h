#pragma once
#include "Animation.h"
#include <string>

using namespace DirectX;

enum class AnimationTextTypes
{
	Info = 0,
	Damage,
	Healing,
	Warning
};


class AnimationTextFloating : public Animation
{
public:

	void Update() override { };
	void Render(size_t tick, SpriteBatch* spriteBatch);
	void Render(size_t tick, SpriteBatch* spriteBatch, RECT* overlayRect) override { Render(tick, spriteBatch); };
	void SetText(std::wstring text);
	AnimationTextFloating(DescriptorHeap* resourceDescriptors, SimpleMath::Vector2 centeredOrigin, 
		std::wstring text = L"", AnimationTextTypes type = AnimationTextTypes::Info);
	~AnimationTextFloating() {};

	AnimationTextTypes m_type = AnimationTextTypes::Info;
	XMVECTORF32 m_color = Colors::GhostWhite;
	float m_scale = 1.0f;
	float m_transparency = 1.0f;
	SpriteFont* m_font;
protected:
	int OffsetFromCenter();		// calculates half length of text string

	std::wstring m_text = L"";
};

