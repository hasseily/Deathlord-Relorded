#include "pch.h"
#include "AppleWinDXVideo.h"
#include "DeathlordHacks.h"
#include "Emulator/Video.h"
#include "Descriptors.h"

// The applewin video displays differently from the other "easy" display classes
// because it uses a texture that needs to be uploaded to the GPU every frame
// A simple UpdateSubresources is necessary with a proper barrier, but otherwise
// it's the same thing as anything else
// TODO: Use its own PSO for complete independent control?
// TODO: Fix the render to use the correct rendering
// TODO: Put all the necessary device stuff from Game.cpp into here
// TODO: Remove all applewin video rendering from Game
// TODO: Allow toggling of the applewin video display.
//			If not in game, the BG shouldn't display and nothing else should (in game.cpp)
//			If in game, here display a curtain bg over the relorded UI before displaying the video
// TODO: Put a nice border around the video

// below because "The declaration of a static data member in its class definition is not a definition"
AppleWinDXVideo* AppleWinDXVideo::s_instance;

#pragma region main
void AppleWinDXVideo::Initialize()
{
	bIsDisplayed = false;
}

void AppleWinDXVideo::ShowApple2Video()
{
	bIsDisplayed = true;
}

void AppleWinDXVideo::HideApple2Video()
{
	bIsDisplayed = false;
}

void AppleWinDXVideo::ToggleApple2Video()
{
	IsApple2VideoDisplayed() ? HideApple2Video() : ShowApple2Video();
}

bool AppleWinDXVideo::IsApple2VideoDisplayed()
{
	return bIsDisplayed;
}
#pragma endregion

void AppleWinDXVideo::Render(SimpleMath::Rectangle r, 
	ResourceUploadBatch* uploadBatch)
{
	// First, update the GPU texture with the latest AppleWin video frame
	auto command_queue = m_deviceResources->GetCommandQueue();
	uploadBatch->Begin();
	uploadBatch->Upload(m_AppleWinDXVideoSpriteSheet.Get(), 0, &m_AppleWinTextureData, 1);
	auto uploadResourcesFinished = uploadBatch->End(command_queue);
	uploadResourcesFinished.wait();
	auto mmTexSize = GetTextureSize(m_AppleWinDXVideoSpriteSheet.Get());

	// Now draw
	auto commandList = m_deviceResources->GetCommandList();
	m_dxtEffect->SetProjection(XMMatrixOrthographicOffCenterRH(r.x, r.x + r.width, r.y + r.height, r.y, 0, 1));
	m_dxtEffect->Apply(commandList);
	m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
	m_primitiveBatch->Begin(commandList);
	m_spriteBatch->Begin(commandList, SpriteSortMode_Deferred);

	// Draw the applewin video in the dead center of r
	XMFLOAT2 _texCenter = { (float)mmTexSize.x / 2.f, (float)mmTexSize.y / 2.f };
	XMFLOAT2 _winCenter = r.Center();
	RECT texRect = {
		_winCenter.x - _texCenter.x,	// left
		_winCenter.y - _texCenter.y,	// top
		_winCenter.x + _texCenter.x,	// right
		_winCenter.y + _texCenter.y		// bottom
	};
	UINT8 borderSize = 5;
	m_primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(r.x, r.y, 0), ColorCurtain),
		VertexPositionColor(XMFLOAT3(r.x + r.width, r.y, 0), ColorCurtain),
		VertexPositionColor(XMFLOAT3(r.x + r.width, r.y + r.height, 0), ColorCurtain),
		VertexPositionColor(XMFLOAT3(r.x, r.y + r.height, 0), ColorCurtain)
	);
	m_primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(texRect.left - borderSize, texRect.top - borderSize, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(texRect.right + borderSize, texRect.top - borderSize, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(texRect.right + borderSize, texRect.bottom + borderSize, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(texRect.left - borderSize, texRect.bottom + borderSize, 0), ColorAmber)
	);
	m_primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(texRect.left, texRect.top, 0), ColorBlack),
		VertexPositionColor(XMFLOAT3(texRect.right, texRect.top, 0), ColorBlack),
		VertexPositionColor(XMFLOAT3(texRect.right, texRect.bottom, 0), ColorBlack),
		VertexPositionColor(XMFLOAT3(texRect.left, texRect.bottom, 0), ColorBlack)
	);
	m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::Apple2Video2), mmTexSize,
		r.Center(), NULL, Colors::White, 0.f, _texCenter);
	m_primitiveBatch->End();
	m_spriteBatch->End();
}

#pragma region D3D stuff
void AppleWinDXVideo::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();

	// Texture property
	m_AppleWinTextureData = {};
	auto fbI = g_pFramebufferinfo->bmiHeader;
	m_AppleWinTextureData.pData = g_pFramebufferbits;
	m_AppleWinTextureData.SlicePitch = static_cast<UINT64>(fbI.biWidth) * fbI.biHeight * sizeof(uint32_t);
	m_AppleWinTextureData.RowPitch = static_cast<LONG_PTR>(fbI.biWidth * sizeof(uint32_t));


	// Create texture resource
	D3D12_HEAP_PROPERTIES props;
	memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
	props.Type = D3D12_HEAP_TYPE_DEFAULT;
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = fbI.biWidth;
	desc.Height = fbI.biHeight;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	DX::ThrowIfFailed(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, NULL, 
		IID_PPV_ARGS(m_AppleWinDXVideoSpriteSheet.ReleaseAndGetAddressOf())));


	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_AppleWinDXVideoSpriteSheet.Get(), 0, 1);

	props.Type = D3D12_HEAP_TYPE_UPLOAD;
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// Create the GPU upload buffer.
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

	DX::ThrowIfFailed(
		device->CreateCommittedResource(
			&props,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_textureUploadHeapMap.GetAddressOf())));

	auto commandList = m_deviceResources->GetCommandList();
	commandList->Reset(m_deviceResources->GetCommandAllocator(), nullptr);
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_AppleWinDXVideoSpriteSheet.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &barrier);
	UpdateSubresources(commandList, m_AppleWinDXVideoSpriteSheet.Get(), m_textureUploadHeapMap.Get(), 0, 0, 1, &m_AppleWinTextureData);
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_AppleWinDXVideoSpriteSheet.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	// Describe and create a SRV for the texture.
	// Using CreateShaderResourceView removes a ton of boilerplate
	CreateShaderResourceView(device, m_AppleWinDXVideoSpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::Apple2Video2));

	// finish up
	DX::ThrowIfFailed(commandList->Close());
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(1, CommandListCast(&commandList));
	// Wait until assets have been uploaded to the GPU.
	m_deviceResources->WaitForGpu();


	// Now that we're finished creating the texture resources, create the DTX stuff
	// We'll use it in Render() to draw on screen.
	// Much easier than dealing with vertices

	m_states = std::make_unique<CommonStates>(device);
	auto sampler = m_states->LinearWrap();
	RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

	EffectPipelineStateDescription epdTriangles(
		&VertexPositionColor::InputLayout,
		CommonStates::AlphaBlend,
		CommonStates::DepthDefault,
		CommonStates::CullNone,
		rtState,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_dxtEffect = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, epdTriangles);
	m_dxtEffect->SetProjection(XMMatrixOrthographicOffCenterRH(0, fbI.biWidth, fbI.biHeight, 0, 0, 1));
	m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(device);

	// The applewin texture is AlphaBlend
	SpriteBatchPipelineStateDescription spd(rtState, &CommonStates::AlphaBlend, nullptr, nullptr, &sampler);
	m_spriteBatch = std::make_unique<SpriteBatch>(device, *resourceUpload, spd);
	m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
}

void AppleWinDXVideo::OnDeviceLost()
{
	m_AppleWinDXVideoSpriteSheet.Reset();
}

#pragma endregion