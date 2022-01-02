#include "pch.h"
#include "MiniMap.h"
#include "Emulator/Memory.h"
#include "DeathlordHacks.h"

// below because "The declaration of a static data member in its class definition is not a definition"
MiniMap* MiniMap::s_instance;

void MiniMap::Initialize()
{

}

void MiniMap::Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch)
{
	auto mmTexSize = GetTextureSize(m_MiniMapSpriteSheet.Get());
	XMFLOAT2 _originPin = { 15.f, 18.f };	// Where the pin points
	RECT _pinRect = { MINIMAP_SPRITE_WIDTH, 0, 2 * MINIMAP_SPRITE_WIDTH, MINIMAP_SPRITE_HEIGHT };
	UINT8 _overlandX = MemGetMainPtr(MAP_OVERLAND_X)[0];
	UINT8 _overlandY = MemGetMainPtr(MAP_OVERLAND_Y)[0];
	XMFLOAT2 _overlayPinPosInMap = {
		r.x + MINIMAP_ORIGIN_X + MINIMAP_X_INCREMENT * _overlandX,
		r.y + MINIMAP_ORIGIN_Y + MINIMAP_Y_INCREMENT * _overlandY
	};
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::MiniMapSpriteSheet), mmTexSize,
		_overlayPinPosInMap, &_pinRect, Colors::White, 0.f, _originPin);
}

#pragma region D3D stuff
void MiniMap::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Sprite_Pushpin.png",
			m_MiniMapSpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_MiniMapSpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::MiniMapSpriteSheet));
}

void MiniMap::OnDeviceLost()
{
	m_MiniMapSpriteSheet.Reset();
}

#pragma endregion