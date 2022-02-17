#pragma once
#include <vector>
#include <SimpleMath.h>
#include "DeviceResources.h"
#include "Game.h"

using namespace DirectX;

constexpr int TICKS_15FPS = 600000;		// Use this to get 15FPS
constexpr int TICKS_30FPS = 300000;		// Use this to get 30FPS
constexpr int TICKS_60FPS = 150000;		// Use this to get 60FPS

extern std::unique_ptr<Game>* GetGamePtr();

// Base animation class (virtual)
// An animation expects the correct spritesheet
// And when calling Render(), the spriteBatch needs to have Begin() called on it already

class Animation
{
public:
	void StartAnimation();
	void StopAnimation();
	void ResumeAnimation();
	void SetRenderOrigin(SimpleMath::Vector2 origin);
	SimpleMath::Rectangle CurrentFrameRectangle() {
		return m_renderRectangle;
	};
	bool IsFinished() { return b_isFinished; };
	virtual void Update() = 0;
	virtual void Render(SpriteBatch* spriteBatch, RECT* overlayRect) = 0;
	virtual ~Animation() {};

	DirectX::SpriteFont* m_font;

protected:
	size_t m_nextFrameTick;		// Tick after which we should swap to the next frame
	int m_currentFrame;
	int m_frameCount;
	// NOTE: It's useless to set a frame length below 333,333 when running at 30 FPS for example
	std::vector<size_t> m_tickFrameLength;		// max tick count per frame (TicksPerSecond 10,000,000)
	SimpleMath::Vector2 m_renderOrigin;			// Starting XY position for rendering
	SimpleMath::Vector2 m_renderCurrent;		// Current XY position for rendering (changes by frame)
	bool b_isFinished;							// Is the animation finished
	std::vector<SimpleMath::Rectangle> m_frameRectangles;
	SimpleMath::Rectangle m_renderRectangle;	// final current render rectangle after all translations
	DescriptorHeap* m_resourceDescriptors;
	XMUINT2 m_spriteSheetSize;
};
