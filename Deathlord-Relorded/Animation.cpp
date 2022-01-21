#include "pch.h"
#include "Animation.h"
#include "TilesetCreator.h"
#include "StepTimer.h"
#include "Descriptors.h"

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
	m_nextFrameTick = SIZE_T_MAX;
}

#pragma endregion

#pragma region AnimationBattleChar
// Fixed X and Y positions for battle module for all possible character slots
constexpr int ARRAY_BATTLE_POS_X[6 + 32]{
	210, 282, 350, 280, 225, 335,						// Player Chars
	276, 236, 330,										// Monsters melee range
	180, 237, 310, 375,									// Monsters layer 2
	150, 195, 242, 292, 348, 400,						// Monster layer 3
	120, 165, 212, 268, 312, 352, 395, 442,				// Monster layer 4
	43, 92, 137, 185, 229, 274, 316, 382, 411, 451, 498	// Monster layer 5
};
constexpr int ARRAY_BATTLE_POS_Y[6 + 32]{
	355, 349, 352, 443, 485, 488,						// Player Chars
	300, 287, 292,										// Monsters melee range
	238, 234, 236, 239,									// Monsters layer 2
	163, 160, 164, 158, 162, 160,						// Monster layer 3
	113, 116, 110, 115, 117, 112, 110, 114,				// Monster layer 4
	55, 52, 60, 59, 48, 52, 56, 51, 54, 53, 60			// Monster layer 5
};

AnimationBattleChar::AnimationBattleChar(DescriptorHeap* resourceDescriptors,
	XMUINT2 spriteSheetSize, UINT8 battlePosition)
{
	m_resourceDescriptors = resourceDescriptors;
	m_spriteSheetSize = spriteSheetSize;
	m_nextFrameTick = SIZE_T_MAX;
	m_tickFrameLength = std::vector<size_t>{ 300000, 300000, 300000, 300000, 300000, 300000, 300000 };
	m_frameCount = m_tickFrameLength.size();
	m_monsterId = 1;
	m_health = 1;
	m_power = 0;
	b_isParty = false;
	b_isFinished = false;	// is never set to true, defaults to idle animation
	// There's only a single frame rectangle
	// the frame will be rendered at different positions based on hit, miss, attack
	m_frameRectangles = std::vector<SimpleMath::Rectangle>();
	m_frameRectangles.emplace_back(0, 0, FBTW, FBTH);
	SimpleMath::Vector2 _origin(ARRAY_BATTLE_POS_X[battlePosition], ARRAY_BATTLE_POS_Y[battlePosition]);
	SetRenderOrigin(_origin);
}

void AnimationBattleChar::Update(AnimationBattleState state)
{
	if (m_state != state)
	{
		m_state = state;
		m_currentFrame = 0;
		m_renderCurrent = m_renderOrigin;
		m_nextFrameTick = m_tickFrameLength[m_currentFrame];
	}
}

void AnimationBattleChar::Render(size_t tick, SpriteBatch* spriteBatch, SimpleMath::Vector2 overlayOrigin)
{
	// The battle animation uses a single image for all frames
	// but it moves depending on the AnimationBattleState
	// Never loop the frames unless you're idle;

	m_frameRectangles.at(0).x = FBTW * (m_monsterId % 0x10);	// The monster spritesheet has 16 monsters per row
	m_frameRectangles.at(0).y = FBTH * (m_monsterId / 0x10);
	if (tick > m_nextFrameTick)		// go to the next frame
	{
		// Reset to idle if the animation has completed
		if ((m_state != AnimationBattleState::idle) && (m_currentFrame == (m_frameCount - 1)))
			Update(AnimationBattleState::idle);
		else
		{
			m_currentFrame = (m_currentFrame + 1) % m_frameCount;
			m_nextFrameTick = m_tickFrameLength[m_currentFrame];
		}

	}
	float _scale = 1.0f;
	auto _origin = XMFLOAT2();
	if (m_state != AnimationBattleState::idle)	// update position
	{
		int _moveX = 0;
		int _moveY = 0;
		int _mP = (b_isParty ? -1 : +1);
		switch (m_state)
		{
		case AnimationBattleState::idle:
			break;
		case AnimationBattleState::attacking:	// edge forward
			_moveY = _mP * (m_currentFrame > (m_frameCount/2) ? -2 : +2);
			break;
		case AnimationBattleState::hit:			// shake
			if (m_currentFrame == 1 || m_currentFrame == 4)
				_moveX = _mP * 2;
			else if (m_currentFrame == 2 || m_currentFrame == 3)
				_moveX = -_mP * 2;
			break;
		case AnimationBattleState::dodged:		// edge back diagonally
			_moveX = _mP * (m_currentFrame > (m_frameCount / 2) ? -2 : +2);
			_moveY = _mP * (m_currentFrame > (m_frameCount / 2) ? +2 : -2);
			break;
		case AnimationBattleState::died:		// shrink to nothingness
			_scale = ((float)m_frameCount - m_currentFrame) / (float)m_frameCount;
			_origin.x = (FBTW / 2) * _scale;
			_origin.y = (FBTH / 2) * _scale;
			break;
		default:
			break;
		}
		m_renderCurrent.x += _moveX;
		m_renderCurrent.y += _moveY;
	}
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapMonsterSpriteSheet),
		m_spriteSheetSize, m_renderCurrent + overlayOrigin + _origin, &(RECT)m_frameRectangles.at(0), Colors::White, 0.f, XMFLOAT2(), _scale);
	// TODO: Draw health and power bars
}
#pragma endregion AnimationBattleChar
