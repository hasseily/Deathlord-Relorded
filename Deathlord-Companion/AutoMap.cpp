#include "pch.h"
#include "AutoMap.h"
#include "Game.h"
#include "MemoryTriggers.h"
#include "resource.h"

using namespace DirectX;

// below because "The declaration of a static data member in its class definition is not a definition"
AutoMap* AutoMap::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();

constexpr UINT8 TILEID_REDRAW = 0xFF;			// when we see this nonexistent tile id we automatically redraw the tile

constexpr XMVECTORF32 COLORTRANSLUCENTWHITE = { 1.f, 1.f, 1.f, .8f };

// This is used for any animated sprite to accelerate or slow its framerate (so one doesn't have to build 30 frames!)
#ifdef _DEBUG
constexpr UINT STROBESLOWAVATAR = 1;
constexpr UINT STROBESLOWHIDDEN = 2;
#else
constexpr UINT STROBESLOWAVATAR = 3;
constexpr UINT STROBESLOWHIDDEN = 5;
#endif

UINT m_avatarStrobeIdx = 0;
constexpr UINT AVATARSTROBECT = 14;
constexpr float AVATARSTROBE[AVATARSTROBECT] = { .75f, .74f, .73f, .72f, .71f, .7f, .7f, .7f, .7f, .71f, .72f, .73f, .74f, .75f };

UINT m_hiddenSpriteIdx = 0;
constexpr UINT HIDDENSPRITECT = 5;

UINT8 tilesVisibleAroundAvatar[81] = { 0 };

void AutoMap::Initialize()
{
	bShowTransition = false;
	m_avatarPosition = XMUINT2(0, 0);
	m_currentMapRect = { 0,0,0,0 };
	m_currentMapUniqueName = "";
	CreateNewTileSpriteMap();
	// Set up the arrays for all backbuffers
	UINT bbCount = m_deviceResources->GetBackBufferCount();
	m_bbufFogOfWarTiles = std::vector(bbCount, std::vector<UINT8>(MAP_LENGTH, 0x00));		// states (seen, etc...)
	m_bbufCurrentMapTiles = std::vector(bbCount, std::vector<UINT8>(MAP_LENGTH, 0x00));		// tile id
}

void AutoMap::ClearMapArea()
{
	for (size_t i = 0; i < m_deviceResources->GetBackBufferCount(); i++)
	{
		std::fill(m_bbufCurrentMapTiles[i].begin(), m_bbufCurrentMapTiles[i].end(), 0x00);
		std::fill(m_bbufFogOfWarTiles[i].begin(), m_bbufFogOfWarTiles[i].end(), 0x00);
	}
}

void AutoMap::ForceRedrawMapArea()
{
	// TODO: unnecessary when we redraw the whole screen every time anyway
	for (size_t i = 0; i < m_deviceResources->GetBackBufferCount(); i++)
	{
		for (size_t mapPos = 0; mapPos < m_bbufCurrentMapTiles[i].size(); mapPos++)
		{
			m_bbufCurrentMapTiles[i][mapPos] = TILEID_REDRAW;
		}
	}
}

void AutoMap::SaveCurrentMapInfo()
{
	if (m_currentMapUniqueName == "")
		return;
	g_nonVolatile.fogOfWarMarkers[m_currentMapUniqueName] = m_bbufFogOfWarTiles[0];
	g_nonVolatile.SaveToDisk();
}


std::string AutoMap::GetCurrentMapUniqueName()
{
	auto memPtr = MemGetMainPtr(0);
	char _buf[60];
	if (memPtr[MAP_IS_OVERLAND] == 0x80)
	{
		sprintf_s(_buf, sizeof(_buf), "Overland_%.2d_%.2d",
			memPtr[MAP_OVERLAND_X],
			memPtr[MAP_OVERLAND_Y]);
	}
	else {
		sprintf_s(_buf, sizeof(_buf), "Map_%.3d_%.2d",
			memPtr[MAP_ID],
			memPtr[MAP_FLOOR]);
	}
	return std::string(_buf);
}

void AutoMap::InitializeCurrentMapInfo()
{
	if (m_currentMapUniqueName == "")
	{
		m_currentMapUniqueName = GetCurrentMapUniqueName();
		auto markers = g_nonVolatile.fogOfWarMarkers[m_currentMapUniqueName];
		if (markers.size() < MAP_LENGTH)
		{
			// Incorrect size, it was probably just initialized to a 0 size
			// reset it
			markers.resize(MAP_LENGTH, 0);
		}
		for (size_t i = 0; i < m_deviceResources->GetBackBufferCount(); i++)
			m_bbufFogOfWarTiles[i] = markers;
	}
}

void AutoMap::SetShowTransition(bool showTransition)
{
	if (showTransition == bShowTransition)
		return;
	bShowTransition = showTransition;
	if (bShowTransition == true)
	{
		//OutputDebugString(L"Started transition!\n");
		// Save the old map
		SaveCurrentMapInfo();
		// We'll set the new map name later when we draw it
		// We don't know yet if the memory is ready for that
		ClearMapArea();
		//ForceRedrawMapArea();
		m_currentMapUniqueName = "";
	}
	else
	{
		//OutputDebugString(L"       Ended transition!\n");
	}
}

#pragma warning(push)
#pragma warning(disable : 26451)
void AutoMap::UpdateAvatarPositionOnAutoMap(UINT x, UINT y)
{
	// Make sure we don't draw on the wrong map!
	if (m_currentMapUniqueName != GetCurrentMapUniqueName())
		InitializeCurrentMapInfo();

	UINT32 cleanX = (x < MAP_WIDTH  ? x : MAP_WIDTH - 1);
	UINT32 cleanY = (y < MAP_HEIGHT ? y : MAP_HEIGHT - 1);
	if ((m_avatarPosition.x == cleanX) && (m_avatarPosition.y == cleanY))
		return;

	/*
	char _buf[400];
	sprintf_s(_buf, 400, "X: %03d / %03d, Y: %03d / %03d, Map name: %s\n", 
		cleanX, m_avatarPosition.x, cleanY, m_avatarPosition.y, m_currentMapUniqueName.c_str());
	OutputDebugStringA(_buf);
	*/

	// We redraw all the tiles in the viewport later, so here just set the footsteps
	m_avatarPosition = { cleanX, cleanY };
	for (size_t i = 0; i < m_deviceResources->GetBackBufferCount(); i++)
	{
		m_bbufFogOfWarTiles[i][m_avatarPosition.x + m_avatarPosition.y * MAP_WIDTH] |= (1 << (UINT8)FogOfWarMarkers::Footstep);
	}

	/*
	char _buf[500];
	sprintf_s(_buf, 500, "Old Avatar Pos tileid has values %2d, %2d in vector\n",
		m_bbufCurrentMapTiles[0][xx_x + xx_y * MAP_WIDTH], m_bbufCurrentMapTiles[1][xx_x + xx_y * MAP_WIDTH]);
	OutputDebugStringA(_buf);
	*/

	// Now get the visible tiles to be analyzed when the state is safe
	// So ask MemoryTriggers to run AutoMap::AnalyzeVisibleTiles() on its next processing
	// during the safe period when the game is waiting on the player
	auto mT = MemoryTriggers::GetInstance();
	mT->DelayedTriggerInsert(DelayedTriggersFunction::VISIBLE_TILES, 0);

	// Automatically switch the visible quadrant if the map isn't visible in full
	AutoMapQuandrant _newQuadrant = AutoMapQuandrant::All;
	if (g_nonVolatile.mapQuadrant != AutoMapQuandrant::All)
	{
		if (m_avatarPosition.x < 0x20)
		{
			if (m_avatarPosition.y < 0x20)
				_newQuadrant = AutoMapQuandrant::TopLeft;
			else
				_newQuadrant = AutoMapQuandrant::BottomLeft;
		}
		else
		{
			if (m_avatarPosition.y < 0x20)
				_newQuadrant = AutoMapQuandrant::TopRight;
			else
				_newQuadrant = AutoMapQuandrant::BottomRight;
		}
		if (g_nonVolatile.mapQuadrant != _newQuadrant) {
			switch (_newQuadrant)
			{
			case AutoMapQuandrant::TopLeft:
				PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_AUTOMAP_DISPLAYTOPLEFTQUADRANT, 1);
				break;
			case AutoMapQuandrant::BottomLeft:
				PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_AUTOMAP_DISPLAYBOTTOMLEFTQUADRANT, 1);
				break;
			case AutoMapQuandrant::TopRight:
				PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_AUTOMAP_DISPLAYTOPRIGHTQUADRANT, 1);
				break;
			case AutoMapQuandrant::BottomRight:
				PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_AUTOMAP_DISPLAYBOTTOMRIGHTQUADRANT, 1);
				break;
			default:
				break;
			}
		}
	}
}

void AutoMap::AnalyzeVisibleTiles()
{

	// Now check the visible tiles around the avatar (tiles that aren't black)
	// Those will have their fog-of-war bit unset
	auto tileset = TilesetCreator::GetInstance();
	tileset->analyzeVisibleTiles(&tilesVisibleAroundAvatar[0]);
	UINT8 oldBufVal;
	for (UINT8 j = 0; j < 9; j++)	// rows
	{
		for (UINT8 i = 0; i < 9; i++)	// columns
		{
			// First calculate the position of the tile in the map
			UINT32 tilePosX = m_avatarPosition.x - 4 + i;
			UINT32 tilePosY = m_avatarPosition.y - 4 + j;
			if ((tilePosX >= MAP_WIDTH) || (tilePosY >= MAP_HEIGHT)) // outside the map (no need to check < 0, it'll roll over as a UINT32)
				continue;
			for (size_t ibb = 0; ibb < m_deviceResources->GetBackBufferCount(); ibb++)
			{
				// Move the visibility bit to the FogOfWar position, and OR it with the value in the vector.
				// If the user ever sees the tile, it stays "seen"
				oldBufVal = m_bbufFogOfWarTiles[ibb][tilePosX + tilePosY * MAP_WIDTH];
				m_bbufFogOfWarTiles[ibb][tilePosX + tilePosY * MAP_WIDTH] |=
					(tilesVisibleAroundAvatar[i + 9 * j] << (UINT8)FogOfWarMarkers::UnFogOfWar);

				// redraw all the tiles in the Deathlord viewport
				m_bbufCurrentMapTiles[ibb][tilePosX + tilePosY * MAP_WIDTH] = TILEID_REDRAW;
			}
		}
	}
}

#pragma warning(pop)

void AutoMap::CreateNewTileSpriteMap()
{
	// The tile spritemap will automatically update from memory onto the GPU texture, just like the AppleWin video buffer
	// Sprite sheet only exists when the player is in-game! Make sure this is called when player goes in game
	// And make sure the map only renders when in game.
	auto tileset = TilesetCreator::GetInstance();
	LoadTextureFromMemory((const unsigned char*)tileset->GetCurrentTilesetBuffer(),
		&m_autoMapTexture, DXGI_FORMAT_R8G8B8A8_UNORM, PNGBUFFERWIDTH, PNGBUFFERHEIGHT);
	//OutputDebugStringA("Loaded map into GPU\n");
}

void AutoMap::DrawAutoMap(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, RECT* mapRect)
{
	// Scale the viewport, then translate it in reverse to how much
	// the center of the mapRect was translated when scaled
	// In this way the map is always centered when scaled, within the mapRect requested
	// Also the center of the mapRect is shifted to one of the quadrants if a zoomed-in map is asked for


	auto commandList = m_deviceResources->GetCommandList();
	SimpleMath::Viewport mapViewport(m_deviceResources->GetScreenViewport());
	SimpleMath::Rectangle mapScissorRect(*mapRect);
	float _scale = (g_nonVolatile.mapQuadrant == AutoMapQuandrant::All ? 1.f : 2.f);
	Vector2 _mapCenter = mapScissorRect.Center();
	switch (g_nonVolatile.mapQuadrant)
	{
	case AutoMapQuandrant::TopLeft:
		_mapCenter.x -= mapScissorRect.width / 2.f;
		_mapCenter.y -= mapScissorRect.height / 2.f;
		break;
	case AutoMapQuandrant::TopRight:
		_mapCenter.x += mapScissorRect.width / 2.f;
		_mapCenter.y -= mapScissorRect.height / 2.f;
		break;
	case AutoMapQuandrant::BottomLeft:
		_mapCenter.x -= mapScissorRect.width / 2.f;
		_mapCenter.y += mapScissorRect.height / 2.f;
		break;
	case AutoMapQuandrant::BottomRight:
		_mapCenter.x += mapScissorRect.width / 2.f;
		_mapCenter.y += mapScissorRect.height / 2.f;
		break;
	}
	Vector2 _mapCenterScaled(_mapCenter * _scale);
	Vector2 _mapTranslation = _mapCenterScaled - _mapCenter;
	mapViewport.x -= _mapTranslation.x;
	mapViewport.y -= _mapTranslation.y;
	mapViewport.width *= _scale;
	mapViewport.height *= _scale;
	commandList->RSSetViewports(1, mapViewport.Get12());
	commandList->RSSetScissorRects(1, &(RECT)mapScissorRect);

	spriteBatch->Begin(commandList, DirectX::SpriteSortMode_Deferred);
	if (bShowTransition)
	{
		// Whatever happens we must show the transition right now
		auto mmBGTexSize = GetTextureSize(m_autoMapTextureBG.Get());
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapBackground), mmBGTexSize,
			m_currentMapRect, nullptr, Colors::White, 0.f, XMFLOAT2());
		spriteBatch->End();
		return;
	}
	// Drawing automap
	UINT currentBackBufferIdx = m_deviceResources->GetSwapChain()->GetCurrentBackBufferIndex();
	CopyRect(&m_currentMapRect, mapRect);
	auto tileset = TilesetCreator::GetInstance();

	// Now draw the automap tiles
	if (g_isInGameMap && m_autoMapTexture != NULL)
	{
		// Set the new map info if necessary
		if (m_currentMapUniqueName != GetCurrentMapUniqueName())
			InitializeCurrentMapInfo();

		float mapScale = (float)MAP_WIDTH_IN_VIEWPORT / (float)(MAP_WIDTH * PNGTW);
		// Use the tilemap texture
		commandList->SetGraphicsRootDescriptorTable(0, m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileSheet));
		auto mmTexSize = GetTextureSize(m_autoMapTexture.Get());
		XMUINT2 tileTexSize(PNGTW, PNGTH);
		// Loop through the in-memory map that has all the tile IDs for the current map
		LPBYTE mapMemPtr = GetCurrentGameMap();
		//OutputDebugStringA((std::to_string(currentBackBufferIdx)).append(std::string(" backbuffer being parsed\n")).c_str());
		for (UINT32 mapPos = 0; mapPos < MAP_LENGTH; mapPos++)
		{
			//OutputDebugStringA((std::to_string(mapPos)).append(std::string(" tile on screen\n")).c_str());

			// TODO: This is disabled as we redraw everything every frame
			// Only redraw the current backbuffer when a tile changes, as well as forbid ClearRenderTargetView
			// if (m_bbufCurrentMapTiles[currentBackBufferIdx][mapPos] == (UINT8)mapMemPtr[mapPos])    // it's been drawn
			//	continue;

			// Now check if the tile has been seen before. Otherwise set it to the black tile which is always
			// the first tile in the tileset

			RECT spriteRect;
			bool shouldDraw = true;
			if (((m_bbufFogOfWarTiles[currentBackBufferIdx][mapPos] & (0b1 << (UINT8)FogOfWarMarkers::UnFogOfWar)) > 0)
				|| (g_nonVolatile.showFog == false))
			{
				spriteRect = tileset->tileSpritePositions.at(mapMemPtr[mapPos] % 0x50);
			}
			else
			{
				// TODO: Since we're clearing the whole viewport every frame, no need to draw the black tile
				spriteRect = tileset->tileSpritePositions.at(0);
				shouldDraw = false;
			}

			XMFLOAT2 spriteOrigin(0, 0);
			UINT32 posX = mapPos % MAP_WIDTH;
			UINT32 posY = mapPos / MAP_WIDTH;
			XMFLOAT2 tilePosInMap(
				m_currentMapRect.left + (posX * PNGTW * mapScale),
				m_currentMapRect.top + (posY * PNGTH * mapScale)
			);
			if (shouldDraw)
			{
				spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileSheet), mmTexSize,
					tilePosInMap, &spriteRect, Colors::White, 0.f, spriteOrigin, mapScale);

				// Show a marker for hidden/unopened items
				// Anything above 0x4F is basically a "hidden" or special bit
				if (g_nonVolatile.showHidden)
				{
					int _hiddenIdx = ((m_hiddenSpriteIdx / STROBESLOWHIDDEN) + mapPos) % HIDDENSPRITECT;
					RECT _hiddenRect = { _hiddenIdx * FBTW, 0, (_hiddenIdx + 1) * FBTW, FBTH };
					if (mapMemPtr[mapPos] > 0x4F)
					{
						// start the strobe for each tile independently, so that it looks a bit better on the map
						spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileHidden), GetTextureSize(m_autoMapTileHidden.Get()),
							tilePosInMap, &_hiddenRect, Colors::White, 0.f, spriteOrigin, mapScale);
					}
//#ifdef _DEBUG
					char _tileHexVal[5];
					sprintf_s(_tileHexVal, 4, "%02x", mapMemPtr[mapPos]);
					// Display the tile ID to see the difference between regular and hidden walls, chutes, etc...
					auto gamePtr = GetGamePtr();
					(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), _tileHexVal,
						tilePosInMap + XMFLOAT2(.5f, .5f), Colors::Black, 0.f, spriteOrigin, .5f);
					(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), _tileHexVal,
						tilePosInMap, Colors::Cyan, 0.f, spriteOrigin, .5f);
//#endif
				}
			}
			m_bbufCurrentMapTiles[currentBackBufferIdx][mapPos] = (UINT8)mapMemPtr[mapPos];

			// now draw the avatar and footsteps
			auto _texSize = GetTextureSize(m_autoMapAvatar.Get());
			XMFLOAT2 _origin = { _texSize.x / 2.f, _texSize.y / 2.f };
			RECT avatarRect = { 0, 0, _texSize.x, _texSize.y };

			XMFLOAT2 overlayPosInMap(	// Display the avatar/footsteps in the center of the tile
				tilePosInMap.x + mapScale * FBTW / 2,
				tilePosInMap.y + mapScale * FBTH / 2
			);

			if (posX == m_avatarPosition.x && posY == m_avatarPosition.y)
			{

				spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapAvatar), _texSize, 
					overlayPosInMap, &avatarRect, Colors::White, 0.f, _origin, mapScale * AVATARSTROBE[m_avatarStrobeIdx / STROBESLOWAVATAR]);
				++m_avatarStrobeIdx;
				if (m_avatarStrobeIdx >= (AVATARSTROBECT * STROBESLOWAVATAR))
					m_avatarStrobeIdx = 0;
			}
			else // don't draw footsteps on the current tile but on anything else that has footsteps
			{
				if (g_nonVolatile.showFootsteps)
				{
					if ((m_bbufFogOfWarTiles[currentBackBufferIdx][mapPos] & (1 << (UINT8)FogOfWarMarkers::Footstep)) > 0)
					{
						spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapAvatar), _texSize,
							overlayPosInMap, &avatarRect, COLORTRANSLUCENTWHITE, 0.f, _origin, mapScale * 0.3f);
					}
				}
			}
		}
		++m_hiddenSpriteIdx;
		if (m_hiddenSpriteIdx >= (HIDDENSPRITECT * STROBESLOWHIDDEN))
			m_hiddenSpriteIdx = 0;

/*
		// Uncomment to debug and display the tilesheet
#ifdef _DEBUG
		XMFLOAT2 zeroOrigin(800, 200);
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileSheet), mmTexSize, zeroOrigin);
#endif
*/
	}
	else // draw the background if not in game
	{
		auto mmBGTexSize = GetTextureSize(m_autoMapTextureBG.Get());
		// nullptr here is the source rectangle. We're drawing the full background
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapBackground), mmBGTexSize,
			m_currentMapRect, nullptr, Colors::White, 0.f, XMFLOAT2());
		// write text on top of automap area
		Vector2 awaitTextPos(
			m_currentMapRect.left + (mapRect->right - mapRect->left) / 2 - 240.f,
			m_currentMapRect.top + (mapRect->bottom - mapRect->top) / 2 - 20.f);
		auto gamePtr = GetGamePtr();
		(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), "THE LANDS OF LORN AWAIT",
			awaitTextPos - Vector2(3.f, -2.f), Colors::White, 0.f, Vector2(0.f, 0.f), 3.f);
		(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), "THE LANDS OF LORN AWAIT",
			awaitTextPos, COLOR_APPLE2_VIOLET, 0.f, Vector2(0.f, 0.f), 3.f);
	}

	spriteBatch->End();
	D3D12_VIEWPORT viewports[1] = { m_deviceResources->GetScreenViewport() };
	D3D12_RECT scissorRects[1] = { m_deviceResources->GetScissorRect() };
	commandList->RSSetViewports(1, viewports);
	commandList->RSSetScissorRects(1, scissorRects);
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
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/PlayerSprite.png",
			m_autoMapAvatar.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_autoMapAvatar.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapAvatar));
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/TileHidden.png",
			m_autoMapTileHidden.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_autoMapTileHidden.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapTileHidden));
}

void AutoMap::OnDeviceLost()
{
	m_autoMapTexture.Reset();
	m_autoMapTextureBG.Reset();
	m_autoMapAvatar.Reset();
	m_autoMapTileHidden.Reset();
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
	DX::ThrowIfFailed(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture)));

	if (pTexture == NULL)
		return false;
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