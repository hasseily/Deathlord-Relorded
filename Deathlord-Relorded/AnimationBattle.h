#pragma once
#include "Animation.h"

using namespace DirectX;

// Animation class for battle monsters/chars
// When someone attacks or evades or is hit

enum class AnimationBattleState
{
	idle = 0,
	attacking,
	hit,
	dodged,
	died
};

class AnimationBattleChar : public Animation
{
public:
	void Update() override { Update(AnimationBattleState::idle); };
	void Update(AnimationBattleState state);
	void Render(size_t tick, SpriteBatch* spriteBatch, SimpleMath::Vector2 overlayOrigin);
	AnimationBattleChar(DescriptorHeap* resourceDescriptors,
		XMUINT2 spriteSheetSize, UINT8 battlePosition);
	~AnimationBattleChar() {};

	UINT8 m_monsterId;							// id of the monster or party member class
	UINT16 m_health;							// Amount of health
	UINT8 m_power;								// Amount of power for magic users in party
	AnimationBattleState m_state;				// Type of animation that will play based on state
	bool b_isParty;								// is a party member (not a monster)
};

