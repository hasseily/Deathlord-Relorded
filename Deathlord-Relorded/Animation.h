#pragma once
#include <vector>
#include <SimpleMath.h>
#include "DeviceResources.h"

using namespace DirectX;

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
	virtual void Render(size_t tick, SpriteBatch* spriteBatch, RECT* overlayRect) = 0;
	virtual ~Animation() {};

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
