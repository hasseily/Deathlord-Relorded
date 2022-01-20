#include "pch.h"
#include "BattleOverlay.h"
#include "resource.h"
#include "Game.h"
#include "DeathlordHacks.h"
#include "PartyLayout.h"
#include "Descriptors.h"
#include "Animation.h"
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

constexpr int TOTAL_SPRITES = 6 + 32;
auto m_animations = std::array<std::unique_ptr<AnimationBattleChar>, TOTAL_SPRITES>();

#pragma region main
void BattleOverlay::Initialize()
{
	bIsDisplayed = false;
	m_currentRect = { 0,0,0,0 };
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
	return bShouldDisplay;	// because at some point it'll display
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
		HideOverlay();
		return;
	}
	ShowOverlay();

	// Second, dereference into the array of tiles in the map
	// where the monster tiles start at 0x40
	// Now we have the tile index of the type of monster we're fighting.
	_mIndex = MemGetMainPtr(GAMEMAP_ARRAY_MONSTER_ID)[_mIndex] - 0x40;
	// And FINALLY, look for the mapping of monsters to tiles for this specific map
	// and go back into it
	_mIndex = MemGetMainPtr(GAMEMAP_START_MONSTERS_IN_LEVEL_IDX)[_mIndex];
	// We could cache this dereferencing, but it really doesn't matter

	// Fill animations correctly
	UINT8 _enemyCount = MemGetMainPtr(MEM_ENEMY_COUNT)[0];
	auto _animSpriteSheetSize = GetTextureSize(AutoMap::GetInstance()->GetMonsterSpriteSheet());
	AnimationBattleChar* _anim;
	for (int i = 0; i < TOTAL_SPRITES; i++)
	{
		if (i >= (6 + _enemyCount))		// Clear unused animations
		{
			m_animations[i] = NULL;
			continue;
		}
		_anim = m_animations[i].get();
		if (_anim == nullptr)
		{
			m_animations[i] = std::make_unique<AnimationBattleChar>(m_resourceDescriptors, _animSpriteSheetSize, i);
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
		}
		return;
	}
	
	// Now check if we should animate the display as it appears
	if (!bIsDisplayed)
	{
		// TODO: Show "FIGHT!" animation
	}

	auto mmBGTexSize = DirectX::XMUINT2(1000, 800);
	auto mmSSTextureSize = GetTextureSize(m_overlaySpriteSheet.Get());
	SimpleMath::Rectangle overlayScissorRect(r);
	Vector2 _overlayCenter = overlayScissorRect.Center();
	m_currentRect.left = _overlayCenter.x - mmBGTexSize.x / 2;
	m_currentRect.top = _overlayCenter.y - mmBGTexSize.y / 2;
	m_currentRect.right = _overlayCenter.x + mmBGTexSize.x / 2;
	m_currentRect.bottom = _overlayCenter.y + mmBGTexSize.y / 2;

	auto gamePtr = GetGamePtr();
	auto font = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular);
	auto timer = (*gamePtr)->m_timer;
	size_t ticks = timer.GetElapsedTicks();
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

	// Draw the party and monsters
	AnimationBattleChar* _anim;
	for (int i = 0; i < TOTAL_SPRITES; i++)
	{
		_anim = m_animations[i].get();
		if (_anim != nullptr)
		{
			_anim->Render(ticks, m_spriteBatch.get());
		}
	}

	// Finish up
	m_primitiveBatch->End();
	m_spriteBatch->End();
	bIsDisplayed = true;
}

#pragma endregion