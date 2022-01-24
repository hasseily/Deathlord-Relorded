#include "pch.h"
#include "AnimationBattle.h"
#include "TilesetCreator.h"
#include "StepTimer.h"
#include "Descriptors.h"

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
	XMUINT2 monsterSpriteSheetSize, UINT8 battlePosition, XMUINT2 battleSpriteSheetSize)
{
	m_resourceDescriptors = resourceDescriptors;
	m_spriteSheetSize = monsterSpriteSheetSize;
	m_battleSpriteSheetSize = battleSpriteSheetSize;
	m_nextFrameTick = SIZE_T_MAX;
	m_tickFrameLength = std::vector<size_t>{ TICKS_30FPS, TICKS_30FPS, TICKS_30FPS, TICKS_30FPS, TICKS_30FPS, TICKS_30FPS, TICKS_30FPS };
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
		m_nextFrameTick = 0;
	}
}

void AnimationBattleChar::Render(SpriteBatch* spriteBatch, RECT* overlayRect)
{
	// The battle animation uses a single image for all frames
	// but it moves depending on the AnimationBattleState
	// Never loop the frames unless you're idle;

	m_frameRectangles.at(0).x = FBTW * (m_monsterId % 0x10);	// The monster spritesheet has 16 monsters per row
	m_frameRectangles.at(0).y = FBTH * (m_monsterId / 0x10);

	XMFLOAT2 overlayOrigin = { (float)overlayRect->left, (float)overlayRect->top };
	float _scale = 1.000000000f;
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
			break;
		default:
			break;
		}
		m_renderCurrent.x += _moveX;
		m_renderCurrent.y += _moveY;
		_origin.x = (FBTW / 2) * (1 - _scale);
		_origin.y = (FBTH / 2) * (1 - _scale);
	}
	m_renderRectangle.x = m_renderCurrent.x + overlayOrigin.x + _origin.x;
	m_renderRectangle.y = m_renderCurrent.y + overlayOrigin.y + _origin.y;
	m_renderRectangle.width = m_frameRectangles.at(0).width * _scale;
	m_renderRectangle.height = m_frameRectangles.at(0).height * _scale;
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapMonsterSpriteSheet),
		m_spriteSheetSize, (RECT)m_renderRectangle, &(RECT)m_frameRectangles.at(0), Colors::White, 0.f, XMFLOAT2());
	
	// Finalize
	auto gamePtr = GetGamePtr();
	auto tick = (*gamePtr)->GetTotalTicks();

	if (tick > m_nextFrameTick)		// go to the next frame
	{
		// Reset to idle if the animation has completed
		if ((m_state != AnimationBattleState::idle) && (m_currentFrame == (m_frameCount - 1)))
			Update(AnimationBattleState::idle);
		else
		{
			m_currentFrame = (m_currentFrame + 1) % m_frameCount;
			m_nextFrameTick = m_tickFrameLength[m_currentFrame] + tick;
		}
	}
}
#pragma endregion AnimationBattleChar


#pragma region AnimationBattleTransition
constexpr int SPRITE_WIDTH = 155;
constexpr int SPRITE_HEIGHT = 190;

AnimationBattleTransition::AnimationBattleTransition(DescriptorHeap* resourceDescriptors,
	XMUINT2 spriteSheetSize)
{
	m_resourceDescriptors = resourceDescriptors;
	m_spriteSheetSize = spriteSheetSize;
	m_nextFrameTick = 0;
	m_tickFrameLength = std::vector<size_t>{	TICKS_30FPS, TICKS_30FPS, TICKS_30FPS, 8000000,
												TICKS_30FPS, TICKS_30FPS, TICKS_30FPS, TICKS_30FPS, 
												TICKS_30FPS, TICKS_30FPS, TICKS_30FPS };
	m_transparency = std::vector<float>{ 
											.4f, .6f, .8f, 1.f, .9f, .8f, .7f, .6f, .5f, .4f, .2f };
	m_scale = std::vector<float>{ 
											.4f, .6f, .8f, 1.f, 1.25f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f };
	m_frameCount = m_tickFrameLength.size();
	m_currentFrame = 0;
	b_isFinished = false;
	// There's only a single frame rectangle
	// the frame will be zoomed out and faded
	m_frameRectangles = std::vector<SimpleMath::Rectangle>();
	m_frameRectangles.emplace_back(28, 0, 28 + SPRITE_WIDTH, SPRITE_HEIGHT);
	SetRenderOrigin(SimpleMath::Vector2());
}

void AnimationBattleTransition::Render(SpriteBatch* spriteBatch, RECT* overlayRect)
{
	if (b_isFinished)
		return;

	XMVECTORF32 _color = { { { 1.000000000f, 1.000000000f, 1.000000000f,  m_transparency[m_currentFrame] } } };
	float _scale = m_scale[m_currentFrame];
	// Display the sprite towards the top of the overlay to leave space to display the start of the fight
	XMFLOAT2 _pos = {	(float)overlayRect->left + (overlayRect->right - overlayRect->left) / 2,
						(float)overlayRect->top + SPRITE_HEIGHT };
	_pos.x -= (SPRITE_WIDTH / 2) * _scale;
	_pos.y -= (SPRITE_HEIGHT / 2) * _scale;
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::BattleOverlaySpriteSheet),
		m_spriteSheetSize, _pos, &(RECT)m_frameRectangles.at(0), _color, 0.f, XMFLOAT2(), _scale);

	// Finalize
	auto gamePtr = GetGamePtr();
	auto tick = (*gamePtr)->GetTotalTicks();

	if (tick > m_nextFrameTick)		// go to the next frame
	{
		m_currentFrame = (m_currentFrame + 1) % m_frameCount;
		m_nextFrameTick = m_tickFrameLength[m_currentFrame] + tick;
		if (m_currentFrame == (m_frameCount - 1))
			b_isFinished = true;
	}
}
#pragma endregion AnimationBattleTransition
