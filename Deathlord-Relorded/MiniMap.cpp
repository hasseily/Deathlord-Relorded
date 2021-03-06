#include "pch.h"
#include "MiniMap.h"
#include "DeathlordHacks.h"
#include "Game.h"
#include <algorithm>

// below because "The declaration of a static data member in its class definition is not a definition"
MiniMap* MiniMap::s_instance;

void MiniMap::Initialize()
{
	m_sectorsSeen = g_nonVolatile.sectorsSeen;
	if (m_sectorsSeen.size() != 256)	// Looks like it's not full, let's reset it
		m_sectorsSeen = std::vector<UINT8>(256);
	maskSprites = std::vector<MaskSpriteType>(256);
	bShouldRenderNew = true;
}

bool MiniMap::Update(UINT8 overlandX, UINT8 overlandY)
{
	if (!g_hasBeenIdleOnce)
		return false;
	if ((overlandX > 0xF) || (overlandY > 0xF))	// shouldn't happen
		return false;
	if ((overlandX == m_overlandX) && (overlandY == m_overlandY))	// nothing to do
	{
		bShouldRenderNew = true;
		return false;
	}
	m_overlandX = overlandX;
	m_overlandY = overlandY;
	UINT8 XY = (m_overlandY << 4) + m_overlandX;
	if (m_sectorsSeen.at(XY) > 0)	// already been in this sector
		return false;
	// Update sector to seen
	m_sectorsSeen.at(XY) = 1;
	g_nonVolatile.sectorsSeen = m_sectorsSeen;
	g_nonVolatile.SaveToDisk();
	bShouldRenderNew = true;
	return true;
}

void MiniMap::Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch)
{
	auto mmTexSize = GetTextureSize(m_MiniMapSpriteSheet.Get());

	// Draw the fog mask
	MaskSpriteType mst;
	XMFLOAT2 _overlayMaskPosInMap;
	RECT _maskRect;
	if (bShouldRenderNew)	// new sector found, update the mask
	{
		for (UINT8 row = 0; row < 16; row++)
		{
			for (UINT8 col = 0; col < 16; col++)
			{
				if (m_sectorsSeen.at((row << 4) + col) & 0b1)	// No need for a mask, it's been seen
					continue;
				mst = MaskTypeForCoords(col, row);
				_maskRect = { SPRITE_FOG_WIDTH * (int)mst, 0 * SPRITE_FOG_HEIGHT,
						SPRITE_FOG_WIDTH * ((int)mst + 1), 1 * SPRITE_FOG_HEIGHT };
				_overlayMaskPosInMap = {
					(float)r.x + MINIMAP_ORIGIN_X + MINIMAP_X_INCREMENT * col,
					(float)r.y + MINIMAP_ORIGIN_Y + MINIMAP_Y_INCREMENT * row
				};
				spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::MiniMapSpriteSheet),
					mmTexSize, _overlayMaskPosInMap, &_maskRect, Colors::White, 0.f, XMFLOAT2(), 1.f);
			}
		}
		bShouldRenderNew = false;
	}

	// Now draw the pin
	XMFLOAT2 _originPin = { 15.f, 18.f };	// Where the pin points within its sprite
	RECT _pinRect = {	SPRITE_PIN_X, SPRITE_PIN_Y,
						SPRITE_PIN_X + SPRITE_PIN_WIDTH, SPRITE_PIN_Y + SPRITE_PIN_HEIGHT };
	XMFLOAT2 _overlayPinPosInMap = {
		(float)r.x + MINIMAP_ORIGIN_X + MINIMAP_X_INCREMENT * m_overlandX + 12,
		(float)r.y + MINIMAP_ORIGIN_Y + MINIMAP_Y_INCREMENT * m_overlandY + 7
	};
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::MiniMapSpriteSheet), mmTexSize,
		_overlayPinPosInMap, &_pinRect, Colors::White, 0.f, _originPin);
}

#pragma region D3D stuff
void MiniMap::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/MinimapSpriteSheet.png",
			m_MiniMapSpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_MiniMapSpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::MiniMapSpriteSheet));
}

void MiniMap::OnDeviceLost()
{
	m_MiniMapSpriteSheet.Reset();
}

#pragma endregion

#pragma region Other
MaskSpriteType MiniMap::MaskTypeForCoords(UINT8 _x, UINT8 _y)
{
	UINT8 _mask = 0;	// left right top bottom
	UINT8 XY;
	int x = _x;
	int y = _y;
	// Check each cardinal point for a seen sector
	if (x == 0)		// Western border
		_mask += 0b1000;
	else
	{
		XY = (_y << 4) + (_x - 1);
		if (m_sectorsSeen.at(XY) & 0b1)
			_mask += 0b1000;
	}
	if (x == 0xF)	// Easter border
		_mask += 0b0100;
	else
	{
		XY = (_y << 4) + (_x + 1);
		if (m_sectorsSeen.at(XY) & 0b1)
			_mask += 0b0100;
	}
	if (y == 0)		// Northern border
		_mask += 0b0010;
	else
	{
		XY = ((_y - 1) << 4) + x;
		if (m_sectorsSeen.at(XY) & 0b1)
			_mask += 0b0010;
	}
	if (y == 0xF)	// Southern border
		_mask += 0b001;
	else
	{
		XY = ((_y + 1) << 4) + x;
		if (m_sectorsSeen.at(XY) & 0b1)
			_mask += 0b001;
	}
	// The mask is actually the opposite since we were looking for seen sectors,
	// and the MaskSpriteTypes instead is based on what unseen neighbors exist
	return ((MaskSpriteType)(_mask ^ 0b1111));

}
#pragma endregion