#include "pch.h"
#include "BattleOverlay.h"
#include "resource.h"
#include "Game.h"
#include "DeathlordHacks.h"
#include "Descriptors.h"
#include <SimpleMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// below because "The declaration of a static data member in its class definition is not a definition"
BattleOverlay* BattleOverlay::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();

#pragma region main
void BattleOverlay::Initialize()
{
	bIsDisplayed = false;
	m_currentRect = { 0,0,0,0 };
}

void BattleOverlay::ShowOverlay()
{
	bIsDisplayed = true;
}

void BattleOverlay::HideOverlay()
{
	bIsDisplayed = false;
}

void BattleOverlay::ToggleOverlay()
{
	IsOverlayDisplayed() ? HideOverlay() : ShowOverlay();
}

bool BattleOverlay::IsOverlayDisplayed()
{
	return bIsDisplayed;
}
#pragma endregion


#pragma region actions

// Update the inventory state based on the game's data
void BattleOverlay::UpdateState()
{
	
}

void BattleOverlay::MousePosInPixels(int x, int y)
{
	if (!bIsDisplayed)
		return;
	Vector2 mousePoint(x, y);
}

void BattleOverlay::LeftMouseButtonClicked(int x, int y)
{
	if (!bIsDisplayed)
		return;
	Vector2 mousePoint(x, y);

ENDLEFTCLICK:
	UpdateState();
}

#pragma endregion

#pragma region D3D stuff
void BattleOverlay::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload, CommonStates* states)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/InvOverlaySpriteSheet.png",
			m_overlaySpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_overlaySpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet));

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

	auto mmBGTexSize = DirectX::XMUINT2(1150, 600);
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

	// Finish up
	m_primitiveBatch->End();
	m_spriteBatch->End();
	bIsDisplayed = true;
}

#pragma endregion