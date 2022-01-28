#include "pch.h"
#include "BattleOverlay.h"
#include "resource.h"
#include "Game.h"
#include "DeathlordHacks.h"
#include "PartyLayout.h"
#include "Descriptors.h"
#include "AnimationBattle.h"
#include "AnimTextManager.h"
#include "AutoMap.h"
#include "Emulator/CPU.h"
#include <SimpleMath.h>
#include <vector>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;

// below because "The declaration of a static data member in its class definition is not a definition"
BattleOverlay* BattleOverlay::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();

constexpr int OVERLAY_WIDTH = 600;
constexpr int OVERLAY_HEIGHT = 600;
constexpr RECT RECT_SPRITE_CURSOR = { 0, 200, 32, 236};

constexpr int TOTAL_SPRITES = 6 + 32;
AnimTextManager* m_animTextManager;
auto m_animations = std::array<std::unique_ptr<AnimationBattleChar>, TOTAL_SPRITES>();
auto m_animationTransition = std::unique_ptr<AnimationBattleTransition>();
int m_enemyMaxHP = 0x01;	// The highest HP of the monsters we're facing. Every monster will have its health bar relative to this
bool m_enemyHPIsSet = false;
std::wstring m_wstrHit = L"-%d hp";
std::wstring m_wstrHealed = L"+%d hp";
std::wstring m_wstrDied = L"+%d xp";
wchar_t m_bufHit[30];
wchar_t m_bufHealed[30];
wchar_t m_bufDied[30];

#pragma region main
void BattleOverlay::Initialize()
{
	bIsDisplayed = false;
	bShouldDisplay = false;
	m_currentRect = { 0,0,0,0 };
	m_animTextManager = AnimTextManager::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_activeActor = 0xFF;
}

void BattleOverlay::ShowOverlay()
{
	bShouldDisplay = true;
}

void BattleOverlay::HideOverlay()
{
	bShouldDisplay = false;
}

void BattleOverlay::ToggleOverlay()
{
	IsOverlayDisplayed() ? HideOverlay() : ShowOverlay();
}

bool BattleOverlay::IsOverlayDisplayed()
{
	return bIsDisplayed;
}

void BattleOverlay::BattleEnemyHPIsSet()
{
	m_enemyHPIsSet = true;
	BattleSetEnemyMaxHP();
}

void BattleOverlay::BattleSetEnemyMaxHP()
{
	// This must only run once per fight, setting the max HP for the enemy type
	// Enemy HP is random within a range. It's calculated by:
	// loop MONSTER_CURR_HPMULT times RAND(1-7), adding them up.
	// So you throw a 7-sided die MONSTER_CURR_HPMULT times
	// Hence the max starting HP is MONSTER_CURR_HPMULT * 7
	if (!m_enemyHPIsSet)
		return;
	m_enemyMaxHP = 7 * MemGetMainPtr(MONSTER_CURR_HPMULT)[0];
	m_enemyHPIsSet = false;	// Only set it at the start of the fight
}

void BattleOverlay::SetActiveActor(UINT8 actorNumber)
{
	m_activeActor = actorNumber;
}
#pragma endregion


#pragma region animations

// A characters begins his attack
void BattleOverlay::SpriteBeginAttack(UINT8 charPosition)
{
	if (m_animations[charPosition] == nullptr)
		Update();
	m_animations[charPosition]->Update(AnimationBattleState::attacking);
}
// etc...
void BattleOverlay::SpriteDodge(UINT8 charPosition)
{
	if (m_animations[charPosition] == nullptr)
		Update();
	m_animations[charPosition]->Update(AnimationBattleState::dodged);
}
void BattleOverlay::SpriteIsHit(UINT8 charPosition, UINT8 damage)
{
	auto _anim = m_animations[charPosition].get();
	if (_anim == nullptr)
		Update();
	_anim->Update(AnimationBattleState::hit);
	// Trigger floating text animation
	auto _animRect = _anim->CurrentFrameRectangle();
	auto _animCenter = _animRect.Center();
	_animCenter.y -= _animRect.height - 4;
	swprintf_s(m_bufHit, m_wstrHit.c_str(), damage);
	m_animTextManager->CreateAnimation(_animCenter, std::wstring(m_bufHit), AnimationTextTypes::Damage);
}
void BattleOverlay::SpriteIsHealed(UINT8 charPosition, UINT16 healingAmount)
{
	auto _anim = m_animations[charPosition].get();
	if (_anim == nullptr)
		Update();
	_anim->Update(AnimationBattleState::healed);
	// Trigger floating text animation
	auto _animRect = _anim->CurrentFrameRectangle();
	auto _animCenter = _animRect.Center();
	_animCenter.y -= _animRect.height - 4;
	swprintf_s(m_bufHealed, m_wstrHealed.c_str(), healingAmount);
	m_animTextManager->CreateAnimation(_animCenter, std::wstring(m_bufHealed), AnimationTextTypes::Healing);
}
void BattleOverlay::SpriteDied(UINT8 charPosition)
{
	auto _anim = m_animations[charPosition].get();
	if (_anim == nullptr)
	{
		Update();
		_anim = m_animations[charPosition].get();
	}
	_anim->Update(AnimationBattleState::died);
	// Trigger floating text animation
	auto _animRect = _anim->CurrentFrameRectangle();
	auto _animCenter = _animRect.Center();
	swprintf_s(m_bufDied, m_wstrDied.c_str(), MemGetMainPtr(MONSTER_CURR_XP)[0]);
	m_animTextManager->CreateAnimation(_animCenter, std::wstring(m_bufDied), AnimationTextTypes::Info);
}
#pragma endregion


#pragma region actions
// Update the state based on the game's data
void BattleOverlay::Update()
{
	// Ensure that we have a sprite animation for every active actor in this battle
	
	// First figure out what monster we're fighting
	// There is double-dereferencing going on.
	// First get the index of the monster for the fight.
	// It's an index into the list of instanced monsters in the map
	UINT8 _mIndex = MemGetMainPtr(MEM_BATTLE_MONSTER_INDEX)[0];
	if (_mIndex == 0xFF)	// No monster, no battle
	{
		// This is after the end of the battle
		// during the post battle looting
		return;
	}

	// Second, dereference into the array of tiles in the map
	// where the monster tiles start at 0x40
	// Now we have the tile index of the type of monster we're fighting.
	if (MemGetMainPtr(MAP_TYPE)[0] == (int)MapType::Dungeon)
	{
		_mIndex = MemGetMainPtr(DUNGEON_ARRAY_MONSTER_ID)[_mIndex] - 0x40;

	}
	else
	{
		_mIndex = MemGetMainPtr(OVERLAND_ARRAY_MONSTER_ID)[_mIndex] - 0x40;
	}
	// And FINALLY, look for the mapping of monsters to tiles for this specific map
	// and go back into it
	_mIndex = MemGetMainPtr(GAMEMAP_START_MONSTERS_IN_LEVEL_IDX)[_mIndex];
	// We could cache this dereferencing, but it really doesn't matter

	// Fill animations correctly
	UINT8 _enemyCount = MemGetMainPtr(MEM_ENEMY_COUNT)[0];
	auto _animSpriteSheetSize = GetTextureSize(AutoMap::GetInstance()->GetMonsterSpriteSheet());
	auto _battleSpriteSheetSize = GetTextureSize(m_overlaySpriteSheet.Get());
	AnimationBattleChar* _anim;
	for (int i = 0; i < TOTAL_SPRITES; i++)
	{
		if (i >= (6 + _enemyCount))		// Clear unused animations
		{
			m_animations[i] = nullptr;
			continue;
		}
		_anim = m_animations[i].get();
		if (_anim == nullptr)
		{
			m_animations[i] = std::make_unique<AnimationBattleChar>(m_resourceDescriptors, _animSpriteSheetSize, i, _battleSpriteSheetSize);
			_anim = m_animations[i].get();
		}
		if (i < 6)	// party
		{
			_anim->m_monsterId = MemGetMainPtr(PARTY_CLASS_START)[i];
			_anim->b_isParty = true;
			_anim->m_health = (MemGetMainPtr(PARTY_HEALTH_HIBYTE_START)[i] << 8) +
				MemGetMainPtr(PARTY_HEALTH_LOBYTE_START)[i];
			if (MemGetMainPtr(PARTY_MAGIC_USER_TYPE_START)[i] == 0xFF)
				_anim->m_power = 0;
			else
				_anim->m_power = MemGetMainPtr(PARTY_HEALTH_LOBYTE_START)[i];
		}
		else // This is the id of the monster in the global monster sheet
		{
			_anim->m_monsterId = _mIndex;
			_anim->b_isParty = false;
			_anim->m_health = MemGetMainPtr(MEM_ENEMY_HP_START)[i - 6];
			_anim->m_power = 0;
		}
	}
}

#pragma endregion

#pragma region D3D stuff
void BattleOverlay::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload, CommonStates* states)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/BattleOverlaySpriteSheet.png",
			m_overlaySpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_overlaySpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::BattleOverlaySpriteSheet));

	// Create the DTX pieces

	auto sampler = states->LinearWrap();
	RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

	EffectPipelineStateDescription epdTriangles(
		&VertexPositionColor::InputLayout,
		CommonStates::Opaque,
		CommonStates::DepthDefault,
		CommonStates::CullNone,		// Note: don't cull because some quadlines are drawn clockwise
									// Specifically the swaplines if the recipient person is to the left of the sender
		rtState,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_dxtEffect = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, epdTriangles);
	m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(device);

	// The applewin texture is AlphaBlend
	SpriteBatchPipelineStateDescription spd(rtState, &CommonStates::NonPremultiplied, nullptr, nullptr, &sampler);
	m_spriteBatch = std::make_unique<SpriteBatch>(device, *resourceUpload, spd);
	m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
}

void BattleOverlay::OnDeviceLost()
{
	m_overlaySpriteSheet.Reset();
	m_spriteBatch.reset();
	m_primitiveBatch.reset();
	m_dxtEffect.reset();
}

#pragma endregion

#pragma region Drawing

void BattleOverlay::Render(SimpleMath::Rectangle r)
{
	if (!bShouldDisplay)
	{
		if (bIsDisplayed)
		{
			// just kill the overlay, it shouldn't be here.
			// Don't bother animating it
			bIsDisplayed = false;
			m_enemyHPIsSet = false;
			m_enemyMaxHP = 0x01;
		}
		return;
	}
	
	// Now check if we should animate the display as it appears
	if (!bIsDisplayed)
	{
		// Show "FIGHT!" animation
		if (m_animationTransition == nullptr)
		{
			m_animationTransition = std::make_unique<AnimationBattleTransition>(m_resourceDescriptors, GetTextureSize(m_overlaySpriteSheet.Get()));
		}
	}

	auto mmBGTexSize = DirectX::XMUINT2(OVERLAY_WIDTH, OVERLAY_HEIGHT);
	auto mmSSTextureSize = GetTextureSize(m_overlaySpriteSheet.Get());
	SimpleMath::Rectangle overlayScissorRect(r);
	Vector2 _overlayCenter = overlayScissorRect.Center();
	m_currentRect.left = _overlayCenter.x - mmBGTexSize.x / 2;
	m_currentRect.top = _overlayCenter.y - mmBGTexSize.y / 2;
	m_currentRect.right = _overlayCenter.x + mmBGTexSize.x / 2;
	m_currentRect.bottom = _overlayCenter.y + mmBGTexSize.y / 2;

	auto gamePtr = GetGamePtr();
	auto font = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular);
	std::wstring _bufStr;

	// Now draw
	auto commandList = m_deviceResources->GetCommandList();
	m_dxtEffect->SetProjection(XMMatrixOrthographicOffCenterRH(
		r.x, r.x + r.width,
		r.y + r.height, r.y, 0, 1));
	m_dxtEffect->Apply(commandList);
	m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
	m_primitiveBatch->Begin(commandList);
	m_spriteBatch->Begin(commandList, SpriteSortMode_Deferred);

	///// Begin Draw Border (2 quads, the black one 10px smaller per side for a 5px thickness
	m_primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(m_currentRect.left, m_currentRect.top, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(m_currentRect.right, m_currentRect.top, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(m_currentRect.right, m_currentRect.bottom, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(m_currentRect.left, m_currentRect.bottom, 0), ColorAmber)
	);
	m_primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(m_currentRect.left + 5, m_currentRect.top + 5, 0), static_cast<XMFLOAT4>(Colors::Black)),
		VertexPositionColor(XMFLOAT3(m_currentRect.right - 5, m_currentRect.top + 5, 0), static_cast<XMFLOAT4>(Colors::Black)),
		VertexPositionColor(XMFLOAT3(m_currentRect.right - 5, m_currentRect.bottom - 5, 0), static_cast<XMFLOAT4>(Colors::Black)),
		VertexPositionColor(XMFLOAT3(m_currentRect.left + 5, m_currentRect.bottom - 5, 0), static_cast<XMFLOAT4>(Colors::Black))
	);
	///// End Draw Border

	///// Begin Draw Inner Wall (boundary between party and enemies)
	m_primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(m_currentRect.left + 12, m_currentRect.top + 315, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(m_currentRect.right - 12, m_currentRect.top + 315, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(m_currentRect.right - 12, m_currentRect.top + 320, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(m_currentRect.left + 12, m_currentRect.top + 320, 0), ColorAmber)
	);
	m_primitiveBatch->DrawQuad(		// Make a hole inside the wall
		VertexPositionColor(XMFLOAT3(m_currentRect.left + 230, m_currentRect.top + 310, 0), static_cast<XMFLOAT4>(Colors::Black)),
		VertexPositionColor(XMFLOAT3(m_currentRect.right - 230, m_currentRect.top + 310, 0), static_cast<XMFLOAT4>(Colors::Black)),
		VertexPositionColor(XMFLOAT3(m_currentRect.right - 230, m_currentRect.top + 325, 0), static_cast<XMFLOAT4>(Colors::Black)),
		VertexPositionColor(XMFLOAT3(m_currentRect.left + 230, m_currentRect.top + 325, 0), static_cast<XMFLOAT4>(Colors::Black))
	);

	///// End Draw Inner Wall


	// Draw the party and monsters
	auto _battleSpriteSheetSize = GetTextureSize(m_overlaySpriteSheet.Get());
	SimpleMath::Rectangle _barRect(0, 0, 28, 5);
	AnimationBattleChar* _anim;
	for (int i = 0; i < TOTAL_SPRITES; i++)
	{
		_anim = m_animations[i].get();
		if (_anim != nullptr)
		{
			auto _animRect = _anim->CurrentFrameRectangle();
			// Draw the cursor on the active sprite
			if ((i == m_activeActor) && i < 9)	// don't bother with any enemy above 3rd
			{
				m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::BattleOverlaySpriteSheet),
					_battleSpriteSheetSize, Vector2(_animRect.x - 2, _animRect.y - 2), &RECT_SPRITE_CURSOR, 
					(i < 6 ? Colors::White : Colors::Red), 0.f, XMFLOAT2(), 1.0f);
			}
			// Render the animated sprites
			_anim->Render(m_spriteBatch.get(), &m_currentRect);
			// Draw the health and power bars
			SimpleMath::Rectangle _healthBarR(_animRect);
			_healthBarR.y += _animRect.height + 2;
			_healthBarR.height = 5;
			if (i < 6)	// party
			{
				UINT16 _mHealth = MemGetMainPtr(PARTY_HEALTH_LOBYTE_START)[i] + ((UINT16)MemGetMainPtr(PARTY_HEALTH_HIBYTE_START)[i] << 8);
				UINT16 _mHealthMax = MemGetMainPtr(PARTY_HEALTH_MAX_LOBYTE_START)[i] + ((UINT16)MemGetMainPtr(PARTY_HEALTH_MAX_HIBYTE_START)[i] << 8);
				_healthBarR.width = _animRect.width * _mHealth / _mHealthMax;
				if (MemGetMainPtr(PARTY_MAGIC_USER_TYPE_START)[i] != 0xFF)	// magic user
				{
					SimpleMath::Rectangle _powerBarR(_healthBarR);
					_powerBarR.y += _healthBarR.height + 2;
					_powerBarR.width = _animRect.width * MemGetMainPtr(PARTY_POWER_START)[i] / MemGetMainPtr(PARTY_POWER_MAX_START)[i];
					m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::BattleOverlaySpriteSheet),
						_battleSpriteSheetSize, (RECT)_powerBarR, &(RECT)_barRect, Colors::Blue, 0.f, XMFLOAT2());
				}
			}
			else // monsters
			{
				UINT8 _mMonsterHPDisplay = MemGetMainPtr(MEM_ENEMY_HP_START)[i - 6];
				int _maxHPUsed = (_mMonsterHPDisplay > m_enemyMaxHP ? _mMonsterHPDisplay : m_enemyMaxHP);
				_healthBarR.width = _animRect.width * MemGetMainPtr(MEM_ENEMY_HP_START)[i - 6] / _maxHPUsed;
			}
			m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::BattleOverlaySpriteSheet),
				_battleSpriteSheetSize, (RECT)_healthBarR, &(RECT)_barRect, Colors::Red, 0.f, XMFLOAT2());
		}
	}
	
	// Draw the battle transition animation if necessary
	if (m_animationTransition != nullptr)
	{
		m_animationTransition->Render(m_spriteBatch.get(), &m_currentRect);
		if (m_animationTransition->IsFinished())
			m_animationTransition = nullptr;
	}
	

	// Finish up
	m_primitiveBatch->End();
	m_spriteBatch->End();
	bIsDisplayed = true;
}

#pragma endregion