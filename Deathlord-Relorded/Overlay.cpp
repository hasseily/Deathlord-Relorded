#include "pch.h"
#include "Overlay.h"
#include "resource.h"
#include "Game.h"


using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;

// In main
extern std::unique_ptr<Game>* GetGamePtr();
extern void SetSendKeystrokesToAppleWin(bool shouldSend);

#pragma region main
void Overlay::Initialize()
{
	bIsDisplayed = false;
	bShouldDisplay = false;
	m_currentRect = { 0,0,0,0 };
	m_width = 600;
	m_height = 600;
	//m_spritesheetDescriptor = TextureDescriptors::DLRLGameOver;
	//m_spritesheetPath = L"Assets/SpriteSheet.png";
}

void Overlay::ShowOverlay()
{
	bShouldDisplay = true;
	if (bShouldBlockKeystrokes)
		SetSendKeystrokesToAppleWin(false);
}

void Overlay::HideOverlay()
{
	bShouldDisplay = false;
	if (bShouldBlockKeystrokes)
		SetSendKeystrokesToAppleWin(true);
}

void Overlay::ToggleOverlay()
{
	IsOverlayDisplayed() ? HideOverlay() : ShowOverlay();
}

bool Overlay::IsOverlayDisplayed()
{
	return bIsDisplayed;
}
#pragma endregion


#pragma region actions
// Update the state based on the game's data
void Overlay::Update()
{

}

#pragma endregion

#pragma region D3D stuff
void Overlay::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload, CommonStates* states)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, m_spritesheetPath.c_str(),
			m_overlaySpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_overlaySpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)m_spritesheetDescriptor));

	// Create the DTX pieces

	auto sampler = states->LinearWrap();
	RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

	EffectPipelineStateDescription epdTriangles(
		&VertexPositionColor::InputLayout,
		CommonStates::AlphaBlend,
		CommonStates::DepthDefault,
		CommonStates::CullNone,		// Note: don't cull because some quadlines may be drawn clockwise
		rtState,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_dxtEffect = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, epdTriangles);
	m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(device);

	SpriteBatchPipelineStateDescription spd(rtState, &CommonStates::NonPremultiplied, nullptr, nullptr, &sampler);
	m_spriteBatch = std::make_unique<SpriteBatch>(device, *resourceUpload, spd);
	m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
}

void Overlay::OnDeviceLost()
{
	m_overlaySpriteSheet.Reset();
	m_spriteBatch.reset();
	m_primitiveBatch.reset();
	m_dxtEffect.reset();
}

#pragma endregion

#pragma region Drawing

void Overlay::Render(SimpleMath::Rectangle r)
{
	// This should be always overridden by a child class

	if (!bShouldDisplay)
	{
		if (bIsDisplayed)
		{
			// Remove overlay
			// Optionally trigger removal animation
			bIsDisplayed = false;
		}
		return;
	}

	// Optionally trigger display animation
	if (!bIsDisplayed)
	{

	}
	PreRender(r);
	// Here do child rendering
	PostRender(r);
}

void Overlay::PreRender(SimpleMath::Rectangle r)
{
	auto mmBGTexSize = DirectX::XMUINT2(m_width, m_height);
	auto mmSSTextureSize = GetTextureSize(m_overlaySpriteSheet.Get());
	Vector2 _overlayCenter = r.Center();
	m_currentRect.left = _overlayCenter.x - mmBGTexSize.x / 2;
	m_currentRect.top = _overlayCenter.y - mmBGTexSize.y / 2;
	m_currentRect.right = _overlayCenter.x + mmBGTexSize.x / 2;
	m_currentRect.bottom = _overlayCenter.y + mmBGTexSize.y / 2;

	// Now draw
	auto commandList = m_deviceResources->GetCommandList();
	m_dxtEffect->SetProjection(XMMatrixOrthographicOffCenterRH(
		r.x, r.x + r.width,
		r.y + r.height, r.y, 0, 1));
	m_dxtEffect->Apply(commandList);
	m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
	m_primitiveBatch->Begin(commandList);
	m_spriteBatch->Begin(commandList, SpriteSortMode_Deferred);

	if (m_curtainColor.w != 0.f)		// Draw a screen curtain to partially/fully hide the underlying rendering
	{
		m_primitiveBatch->DrawQuad(
			VertexPositionColor(XMFLOAT3(r.x, r.y, 0), m_curtainColor),
			VertexPositionColor(XMFLOAT3(r.x + r.width, r.y, 0), m_curtainColor),
			VertexPositionColor(XMFLOAT3(r.x + r.width, r.y + r.height, 0), m_curtainColor),
			VertexPositionColor(XMFLOAT3(r.x, r.y + r.height, 0), m_curtainColor)
		);
	}

	if (m_type == OverlayType::Bordered)
	{
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
	}
}

void Overlay::PostRender(SimpleMath::Rectangle r)
{
	// Finish up
	m_primitiveBatch->End();
	m_spriteBatch->End();
	bIsDisplayed = true;
}

#pragma endregion