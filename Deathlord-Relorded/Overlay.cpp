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

enum class OverlayRootParameterIndex
{
	TextureSRV,
	ConstantBuffer,
	MyConstantBuffer,
	Count
};

#pragma region main
void Overlay::Initialize()
{
	m_overlayState = OverlayState::Hidden;
	auto gamePtr = GetGamePtr();
	m_overlayState = OverlayState::Hidden;
	bShouldDisplay = false;
	m_currentRect = { 0,0,0,0 };
	m_width = 600;
	m_height = 600;
	m_shaderParameters.barThickness = 0.005f;
	m_shaderParameters.deltaT = 0;
	m_shaderParameters.maxInterference = 0.f;
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

	return (m_overlayState == OverlayState::Displayed);
}


bool Overlay::ShouldRenderOverlay()
{
	return bShouldDisplay;
}

#pragma endregion


#pragma region actions
// Update the state based on the game's data
void Overlay::Update()
{

}

#pragma endregion

#pragma region D3D stuff
void Overlay::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload, 
	CommonStates* states, const wchar_t* pixelShaderName)
{
	auto device = m_deviceResources->GetD3DDevice();
	m_states = states;

	// Check Shader Model 6 support
	D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_0 };
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))
		|| (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0))
	{
#ifdef _DEBUG
		OutputDebugStringA("ERROR: Shader Model 6.0 is not supported!\n");
#endif
		throw std::runtime_error("Shader Model 6.0 is not supported!");
	}

	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, m_spritesheetPath.c_str(),
			m_overlaySpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_overlaySpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)m_spritesheetDescriptor));

	// Create the DTX pieces

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

	SpriteBatchPipelineStateDescription spd(rtState, &CommonStates::NonPremultiplied, nullptr, nullptr, nullptr);
	auto vsBlob = DX::ReadData(L"SinglePostProcessVS.cso");
	DX::ThrowIfFailed(
		device->CreateRootSignature(0, vsBlob.data(), vsBlob.size(),
			IID_PPV_ARGS(m_rootSig.ReleaseAndGetAddressOf())));
	spd.customRootSignature = m_rootSig.Get();
	spd.customVertexShader = { vsBlob.data(), vsBlob.size() };
	auto blob = DX::ReadData(pixelShaderName);
	spd.customPixelShader = { blob.data(), blob.size() };
	m_overlaySB = std::make_unique<SpriteBatch>(device, *resourceUpload, spd);
	m_overlaySB->SetViewport(m_deviceResources->GetScreenViewport());

}

void Overlay::OnDeviceLost()
{
	m_shaderParamsResource.Reset();
	m_overlaySpriteSheet.Reset();
	m_overlaySB.reset();
	m_primitiveBatch.reset();
	m_dxtEffect.reset();
	m_rootSig.Reset();
}

#pragma endregion

#pragma region Drawing

void Overlay::Render(SimpleMath::Rectangle r)
{
	// This should be always overridden by a child class

	if (!bShouldDisplay)
	{
		if (m_overlayState == OverlayState::Displayed)
		{
			// Remove overlay
			// Optionally trigger removal animation
			m_overlayState = OverlayState::Hidden;
		}
		return;
	}

	// Optionally trigger display animation
	if (m_overlayState != OverlayState::Displayed)
	{
		// m_state = OverlayState::TransitionIn;
		m_overlayState = OverlayState::Displayed;
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
	auto gamePtr = GetGamePtr();

	// Now set the command list data
	commandList->SetGraphicsRootSignature(m_rootSig.Get());
	m_shaderParamsResource = (*gamePtr)->GetGraphicsMemory()->AllocateConstant(m_shaderParameters);
	commandList->SetGraphicsRootConstantBufferView((int)OverlayRootParameterIndex::MyConstantBuffer,
		m_shaderParamsResource.GpuAddress());


	m_dxtEffect->SetProjection(XMMatrixOrthographicOffCenterRH(
		r.x, r.x + r.width,
		r.y + r.height, r.y, 0, 1));
	m_dxtEffect->Apply(commandList);
	m_overlaySB->SetViewport(m_deviceResources->GetScreenViewport());
	m_primitiveBatch->Begin(commandList);
	m_overlaySB->Begin(commandList, SpriteSortMode_Deferred);

	if (m_overlayState == OverlayState::Displayed)
	{
		// Don't display curtain and border until the whole thing is displayed.
		// It adds to the impact

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

}

void Overlay::PostRender(SimpleMath::Rectangle r)
{
	// Finish up
	m_primitiveBatch->End();
	m_overlaySB->End();
}

#pragma endregion