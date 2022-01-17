##pragma once
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
	bool IsFinished() { return b_isFinished; };
	virtual void Update() = 0;
	virtual void Render(size_t tick, SpriteBatch* spriteBatch) = 0;
	virtual ~Animation() {};

protected:
	size_t m_nextFrameTick;		// Tick after which we should swap to the next frame
	int m_currentFrame;
	int m_frameCount;
	std::vector<size_t> m_tickFrameLength;		// max tick count per frame (TicksPerSecond 10,000,000)
	SimpleMath::Vector2 m_renderOrigin;			// Starting XY position for rendering
	SimpleMath::Vector2 m_renderCurrent;		// Current XY position for rendering (changes by frame)
	bool b_isFinished;							// Is the animation finished
	std::vector<SimpleMath::Rectangle> m_frameRectangles;
	DescriptorHeap* m_resourceDescriptors;
	XMUINT2 m_spriteSheetSize;
};

// Animation class for battle monsters/chars
// When someone attacks or evades or is hit

enum class AnimationBattleState
{
	idle = 0,
	attacking,
	hit,
	dodged
};

class AnimationBattleChar : public Animation
{
public:
	void Update(AnimationBattleState state);
	void Render(size_t tick, SpriteBatch* spriteBatch);
	AnimationBattleChar(DescriptorHeap* resourceDescriptors,
		XMUINT2 spriteSheetSize, UINT8 battlePosition);

	UINT8 m_monsterId;							// id of the monster or party member class
	UINT16 m_health;							// Amount of health
	UINT8 m_power;								// Amount of power for magic users in party
	AnimationBattleState m_state;				// Type of animation that will play based on state
	bool b_isParty;								// is a party member (not a monster)
};