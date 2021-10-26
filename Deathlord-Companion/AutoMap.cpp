#include "pch.h"
#include "AutoMap.h"
#include "Game.h"

using namespace DirectX;

// below because "The declaration of a static data member in its class definition is not a definition"
AutoMap* AutoMap::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();

void AutoMap::UpdateAvatarPositionOnAutoMap(UINT8 x, UINT8 y)
{
	// TODO
	// Draw an avatar "@" marker on the tile?
}

void AutoMap::CreateNewTileSpriteMap()
{
	// The tile spritemap needs to update at every level change
	// Sprite sheet only exists when the player is in-game! Make sure this is called when player goes in game
	// And make sure the map only renders when in game.
	auto device = m_deviceResources->GetD3DDevice();
	auto tileset = TilesetCreator::GetInstance();
	LoadTextureFromMemory((const unsigned char*)tileset->GetCurrentTilesetBuffer(),
		&m_autoMapTexture, DXGI_FORMAT_R8G8B8A8_UNORM, PNGBUFFERWIDTH, PNGBUFFERHEIGHT);
	//OutputDebugStringA("Loaded map into GPU\n");
}

void AutoMap::DrawAutoMap(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, RECT* mapRect)
{
	// Drawing automap
	CopyRect(&m_currentMapRect, mapRect);
	auto tileset = TilesetCreator::GetInstance();
	auto commandList = m_deviceResources->GetCommandList();
	spriteBatch->Begin(commandList, DirectX::SpriteSortMode_Deferred);

	// Now draw the automap tiles
	if (g_isInGameMap && m_autoMapTexture != NULL)
	{
		float mapScale = (float)MAP_WIDTH_IN_VIEWPORT / (float)(MAP_WIDTH * PNGTW);
		// Use the tilemap texture
		commandList->SetGraphicsRootDescriptorTable(0, m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileSheet));
		auto mmTexSize = GetTextureSize(m_autoMapTexture.Get());
		XMUINT2 tileTexSize(PNGTW, PNGTH);
		// Loop through the in-memory map that has all the tile IDs for the current map
		LPBYTE mapMemPtr = GetCurrentGameMap();
		for (size_t mapPos = 0; mapPos < MAP_LENGTH; mapPos++)
		{
			//OutputDebugStringA((std::to_string(mapPos)).append(std::string(" tile on screen\n")).c_str());
			// TODO: we now redraw every frame. Not efficient!
			// We would need to redraw both front and back buffers when a tile changes, as well as forbid ClearRenderTargetView
//				if (currentMapTiles[mapPos] == (UINT8)mapMemPtr[mapPos])    // it's been drawn
//					continue;
			RECT spriteRect = tileset->tileSpritePositions.at(mapMemPtr[mapPos]);
			XMFLOAT2 spriteOrigin(0, 0);
			int posX = mapPos % MAP_WIDTH;
			int posY = mapPos / MAP_WIDTH;
			XMFLOAT2 tilePosInMap(
				m_currentMapRect.left + (posX * PNGTW * mapScale),
				m_currentMapRect.top + (posY * PNGTH * mapScale)
			);
			spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileSheet), mmTexSize,
				tilePosInMap, &spriteRect, Colors::White, 0.f, spriteOrigin, mapScale);
			m_currentMapTiles[mapPos] = (UINT8)mapMemPtr[mapPos];
			//OutputDebugStringA((std::to_string(mapPos)).append(std::string(" tile DRAWN on screen\n")).c_str());

		}
	}
	else // draw the background if not in game
	{
		auto mmBGTexSize = GetTextureSize(m_autoMapTextureBG.Get());
		// nullptr here is the source rectangle. We're drawing the full background
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapBackground), mmBGTexSize,
			m_currentMapRect, nullptr, Colors::White, 0.f, XMFLOAT2());
		// write text on top of automap area
		Vector2 awaitTextPos(
			m_currentMapRect.left + (mapRect->right - mapRect->left) / 2 - 200.f,
			m_currentMapRect.top + (mapRect->bottom - mapRect->top) / 2 - 20.f);
		auto gamePtr = GetGamePtr();
		(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), "Awaiting Masochists...",
			awaitTextPos - Vector2(2.f, 2.f), Colors::White, 0.f, Vector2(0.f, 0.f), 3.f);
		(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), "Awaiting Masochists...",
			awaitTextPos, COLOR_APPLE2_VIOLET, 0.f, Vector2(0.f, 0.f), 3.f);
	}

	/*
	// TODO: delete, this is a test
	RECT sTestRect = { 0, 0, 448, 512 };
	RECT dTestRect = { 500, 200, 948, 712 };
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileSheet),
		GetTextureSize(m_autoMapTexture.Get()), dTestRect, &sTestRect);
	*/

	spriteBatch->End();
	// End drawing automap

}

# pragma region D3D stuff
void AutoMap::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Background_NoMap.png",
			m_autoMapTextureBG.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_autoMapTextureBG.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapBackground));
}

void AutoMap::OnDeviceLost()
{
	m_autoMapTexture.Reset();
	m_autoMapTextureBG.Reset();
}
#pragma endregion

#pragma region DX Helpers

// Simple helper function to load an image as a R8B8G8 memory buffer into a DX12 texture with common settings
// Returns true on success, with the SRV CPU handle having an SRV for the newly-created texture placed in it
// (srv_cpu_handle must be a handle in a valid descriptor heap)
bool AutoMap::LoadTextureFromMemory(const unsigned char* image_data,
	Microsoft::WRL::ComPtr <ID3D12Resource>* out_tex_resource,
	DXGI_FORMAT tex_format, int width, int height)
{
	int image_width = width;
	int image_height = height;
	if (image_data == NULL)
		return false;

	auto device = m_deviceResources->GetD3DDevice();

	// Create texture resource
	D3D12_HEAP_PROPERTIES props;
	memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
	props.Type = D3D12_HEAP_TYPE_DEFAULT;
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = image_width;
	desc.Height = image_height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = tex_format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* pTexture = NULL;
	device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(pTexture, 0, 1);

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

	m_textureDataMap.pData = image_data;
	m_textureDataMap.SlicePitch = static_cast<UINT64>(width) * height * sizeof(uint32_t);
	m_textureDataMap.RowPitch = static_cast<LONG_PTR>(desc.Width * sizeof(uint32_t));

	auto commandList = m_deviceResources->GetCommandList();
	commandList->Reset(m_deviceResources->GetCommandAllocator(), nullptr);


	UpdateSubresources(commandList, pTexture, m_textureUploadHeapMap.Get(), 0, 0, 1, &m_textureDataMap);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	// Describe and create a SRV for the texture.
	// Using CreateShaderResourceView removes a ton of boilerplate
	CreateShaderResourceView(device, pTexture,
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapTileSheet));

	// finish up
	DX::ThrowIfFailed(commandList->Close());
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(1, CommandListCast(&commandList));
	// Wait until assets have been uploaded to the GPU.
	m_deviceResources->WaitForGpu();

	// Return results
	out_tex_resource->Attach(pTexture);

	return true;
}

#pragma endregion