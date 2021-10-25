//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
#include "SidebarManager.h"
#include "SidebarContent.h"
#include "Sidebar.h"
#include "RemoteControl/Gamelink.h"
#include "Keyboard.h"
#include "Emulator/AppleWin.h"
#include "Emulator/Video.h"
#include "MemoryTriggers.h"
#include "TilesetCreator.h"
#include <vector>
#include <string>
#include <map>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;
using namespace HA;


// AppleWin video texture
D3D12_SUBRESOURCE_DATA g_textureData;
ComPtr<ID3D12Resource> g_textureUploadHeap;

D3D12_SUBRESOURCE_DATA g_textureDataMap;
ComPtr<ID3D12Resource> g_textureUploadHeapMap;

// Instance variables
HWND m_window;
static SidebarManager m_sbM;
static SidebarContent m_sbC;
// fonts and primitives from dxtoolkit12 to draw lines
static std::map<FontDescriptors, std::unique_ptr<SpriteFont>> m_spriteFonts;
static std::unique_ptr<PrimitiveBatch<VertexPositionColor>> m_primitiveBatch;
std::unique_ptr<BasicEffect> m_lineEffect;
AppMode_e m_previousAppMode = AppMode_e::MODE_UNKNOWN;

static std::wstring last_logged_line;
static UINT64 tickOfLastRender = 0;

static Vector2 m_vector2Zero = { 0.f, 0.f };
static RECT m_cachedClientRect;

static MemoryTriggers* m_trigger;
UINT64	g_debug_video_field = 0;
UINT64	g_debug_video_data = 0;
NonVolatile g_nonVolatile;

bool g_isInGameMap = false;
bool g_wantsToSave = true;  // DISABLED. It can corrupt saved games
float Game::m_clientFrameScale = 1.f;

static std::map<UINT32, UINT8> currentMapTiles;   // All mapPos -> tileid for the whole map
                                                   // to ensure we don't redraw what's already there

Game::Game() noexcept(false)
{
    g_nonVolatile = NonVolatile();
    g_textureData = {};
    GameLink::Init(false);

    // TODO: give option to un-vsync?
    // TODO: give option to show FPS?
	m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0, DX::DeviceResources::c_AllowTearing);
	//m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0);
    m_deviceResources->RegisterDeviceNotify(this);

    // Any time the layouts differ, a recreation of the vertex buffer is triggered
    m_previousLayout = EmulatorLayout::NONE;
    m_currentLayout = EmulatorLayout::NORMAL;
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
    GameLink::Term();
}

// Initialize the resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_window = window;

    m_gamePad = std::make_unique<GamePad>();
    m_keyboard = std::make_unique<Keyboard>();

    g_nonVolatile.LoadFromDisk();

	// Initialize emulator
	EmulatorOneTimeInitialization(window);
	EmulatorRepeatInitialization();

    wchar_t buff[MAX_PATH];
    DX::FindMediaFile(buff, MAX_PATH, L"Background.jpg");
    m_bgImage = HA::LoadBGRAImage(buff, m_bgImageWidth, m_bgImageHeight);

	m_sbC.Initialize();
	m_tileset = GetTilesetCreator();

    shouldRender = true;

    GetClientRect(window, &m_cachedClientRect);
    m_clientFrameScale = 1.f;
    m_deviceResources->SetWindow(window, width, height, (float)GetFrameBufferWidth(), (float)GetFrameBufferHeight());

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // Use a variable timestep to give as much time to the emulator as possible
    // Then we control how often we render later in the Render() method
	// m_timer.SetTargetElapsedSeconds(1.0 / MAX_RENDERED_FRAMES_PER_SECOND);
	m_timer.SetFixedTimeStep(false);
    
    CreateNewTileSpriteMap();
	m_trigger = MemoryTriggers::GetInstance(&m_timer);
	m_trigger->PollMapSetCurrentValues();
}

#pragma region Window texture and size

// Select which texture to show: The default background or the gamelink data
D3D12_RESOURCE_DESC Game::ChooseTexture()
{
    D3D12_RESOURCE_DESC txtDesc = {};
    txtDesc.MipLevels = txtDesc.DepthOrArraySize = 1;
    txtDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    txtDesc.SampleDesc.Count = 1;
    txtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	switch (g_nAppMode)
	{
	case AppMode_e::MODE_LOGO:  // obsolete
	case AppMode_e::MODE_PAUSED:
	{
		txtDesc.Width = m_bgImageWidth;
		txtDesc.Height = m_bgImageHeight;
		g_textureData.pData = m_bgImage.data();
		g_textureData.SlicePitch = m_bgImage.size();
		SetVideoLayout(EmulatorLayout::NORMAL);
		break;
	}
	/*
	#ifdef _DEBUG
		// In debug mode when pause we display the current tilemap
		// Enable to debug the tilemap
		// TODO: Write a PAUSE overlay text in render() when paused
		case AppMode_e::MODE_PAUSED:
		{
			m_tileset = GetTilesetCreator();
			UINT64 pngW = PNGBUFFERWIDTHB / sizeof(UINT32);
			UINT64 pngH = PNGBUFFERHEIGHT;
			LPBYTE tsB = m_tileset->GetCurrentTilesetBuffer();
			auto fbI = g_pFramebufferinfo->bmiHeader;
			UINT64 lineByteWidth = MIN(pngW, fbI.biWidth) * sizeof(UINT32);
			for (size_t h = 0; h < MIN(PNGBUFFERHEIGHT, fbI.biHeight) ; h++)
			{
				memcpy_s(g_pFramebufferbits + (h * fbI.biWidth * sizeof(UINT32)), lineByteWidth,
					tsB + (h * PNGBUFFERWIDTHB), lineByteWidth);
			}
			SetVideoLayout(EmulatorLayout::NORMAL);
		}
	#endif
	*/
	default:
	{
		auto fbI = g_pFramebufferinfo->bmiHeader;
		txtDesc.Width = fbI.biWidth;
		txtDesc.Height = fbI.biHeight;
		g_textureData.pData = g_pFramebufferbits;
		g_textureData.SlicePitch = ((long long)GetFrameBufferWidth()) * ((long long)GetFrameBufferHeight()) * sizeof(bgra_t);
		SetVideoLayout(EmulatorLayout::NORMAL);
		break;
	}
	}
    g_textureData.RowPitch = static_cast<LONG_PTR>(txtDesc.Width * sizeof(uint32_t));
    return txtDesc;
}

void Game::SetVideoLayout(EmulatorLayout layout)
{
    m_previousLayout = m_currentLayout;
    m_currentLayout = layout;
}

void Game::SetWindowSizeOnChangedProfile()
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(m_window, &wp);
    // Update the window position to display the sidebars
    int w, h;
    m_clientFrameScale = 1.f;
    GetBaseSize(w, h);
    RECT wR = { 0,0,w,h };
    SendMessage(m_window, WM_SIZING, WMSZ_BOTTOMRIGHT, (LPARAM)&wR);
    wp.rcNormalPosition.right = wp.rcNormalPosition.left + wR.right;
    wp.rcNormalPosition.bottom = wp.rcNormalPosition.top + wR.bottom;
    SetWindowPlacement(m_window, &wp);
    SendMessage(m_window, WM_SIZE, SIZE_RESTORED, w & 0x0000FFFF | h << 16);
}
#pragma endregion

#pragma region Others

// Base size adds the automap after getting the size with the sidebars
void Game::GetBaseSize(__out int& width, __out int& height) noexcept
{
	m_sbM.GetBaseSize(width, height);
	width = width + MAP_WIDTH_IN_VIEWPORT;
    UINT32 mapHeight = MAP_WIDTH_IN_VIEWPORT * PNGTH / PNGTW;
	if (height < mapHeight)
		height = mapHeight;
	return;
}

#pragma endregion

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

	EmulatorMessageLoopProcessing();

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        if (pad.IsViewPressed())
        {
            // Do something when gamepad keys are pressed
        }
    }
    auto kb = m_keyboard->GetState();
    // TODO: Should we pass the keystrokes back to AppleWin here?
    if (kb.Escape)
    {
        // Do something when escape or other keys pressed
    }

    // poll memory and fire triggers at specific intervals
    m_trigger->Process();

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // use shouldRender to pause/activate rendering
    if (!shouldRender)
        return;
    // Don't try to render anything before the first Update.
	uint32_t currFrameCount = m_timer.GetFrameCount();
    if (currFrameCount == 0)
    {
        return;
    }

    // Change in mode
	if (m_previousAppMode != g_nAppMode)
	{
        m_previousAppMode = g_nAppMode;
		OnWindowChanged();
	}

    // change in video layout Normal/Flipped
    if (m_previousLayout != m_currentLayout)
    {
        OnWindowChanged();
    }

    // Only render 30 times a second. No need to render more often.
    // The tick loop is running as fast as possible so that the emulator runs as fast as possible
    // Here we'll just decide whether to render or not

    UINT64 ticksSinceLastRender = m_timer.GetTotalTicks() - tickOfLastRender;
    if (ticksSinceLastRender > m_timer.TicksPerSecond / MAX_RENDERED_FRAMES_PER_SECOND)
    {
        tickOfLastRender = m_timer.GetTotalTicks();
        GetClientRect(m_window, &m_cachedClientRect);

        // First update the sidebar, it doesn't need to be updated until right before the render
        // Only allow x microseconds for updates of sidebar every render
        UINT64 sbTimeSpent = m_sbC.UpdateAllSidebarText(&m_sbM, false, 500);

        // Prepare the command list to render a new frame.
        m_deviceResources->Prepare();
        Clear();

        auto commandList = m_deviceResources->GetCommandList();
        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

        // Drawing video texture
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &barrier);
        UpdateSubresources(commandList, m_texture.Get(), g_textureUploadHeap.Get(), 0, 0, 1, &g_textureData);
        barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);

        commandList->SetGraphicsRootSignature(m_rootSignature.Get());
        commandList->SetPipelineState(m_pipelineState.Get());

        // One unified heap for all resources
		ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
		commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

        commandList->SetGraphicsRootDescriptorTable(0, m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::Apple2Video));

        // Set necessary state.
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        commandList->IASetIndexBuffer(&m_indexBufferView);

        // Draw quad.
        commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
        // End drawing video texture

		// Drawing automap
        // TODO: Do most of this outside of render, much of it is fixed between renders
		int origW, origH;
		GetBaseSize(origW, origH);
		auto mmOrigin = Vector2(m_cachedClientRect.right - MAP_WIDTH_IN_VIEWPORT, 0.f);
        RECT mapRectInViewport = {
            mmOrigin.x,
            mmOrigin.y,
            mmOrigin.x + MAP_WIDTH_IN_VIEWPORT,
            mmOrigin.y + MAP_WIDTH_IN_VIEWPORT * PNGTH / PNGTW
        };
		m_spriteBatch->Begin(commandList, DirectX::SpriteSortMode_Deferred);

		// Now draw the automap tiles
        if (g_isInGameMap && m_autoMapTexture != NULL)
        {
            float mapScale = (float)MAP_WIDTH_IN_VIEWPORT / (float)(MAP_WIDTH * PNGTW);
			// Use the tilemap texture
			commandList->SetGraphicsRootDescriptorTable(0, m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileSheet));
			auto mmTexSize = GetTextureSize(m_autoMapTexture.Get());
            XMUINT2 tileTexSize(PNGTW, PNGTH);
            // Loop through the in-memory map that has all the tile IDs for the current map
			LPBYTE mapMemPtr = m_tileset->GetCurrentGameMap();
            for (size_t mapPos = 0; mapPos < MAP_LENGTH; mapPos++)
            {
				//OutputDebugStringA((std::to_string(mapPos)).append(std::string(" tile on screen\n")).c_str());
                // TODO: we now redraw every frame. Not efficient!
				//if (currentMapTiles[mapPos] == (UINT8)mapMemPtr[mapPos])    // it's been drawn
					//continue;
				RECT spriteRect = m_tileset->tileSpritePositions.at(mapMemPtr[mapPos]);
				XMFLOAT2 spriteOrigin(0, 0);
				int posX = mapPos % MAP_WIDTH;
				int posY = mapPos / MAP_WIDTH;
				XMFLOAT2 tilePosInMap(
                    mapRectInViewport.left + (posX * PNGTW * mapScale), 
                    mapRectInViewport.top + (posY * PNGTH * mapScale)
                );
				m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileSheet), mmTexSize,
                    tilePosInMap, &spriteRect, Colors::White, 0.f, spriteOrigin, mapScale);
				currentMapTiles[mapPos] = (UINT8)mapMemPtr[mapPos];
				//OutputDebugStringA((std::to_string(mapPos)).append(std::string(" tile DRAWN on screen\n")).c_str());
              
			}
		}
        else // draw the background if not in game
        {
			auto mmBGTexSize = GetTextureSize(m_autoMapTextureBG.Get());
			// nullptr here is the source rectangle. We're drawing the full background
			m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapBackground), mmBGTexSize,
				mapRectInViewport, nullptr, Colors::White, 0.f, XMFLOAT2());
			// write text on top of automap area
			Vector2 awaitTextPos(
				mapRectInViewport.left + (mapRectInViewport.right - mapRectInViewport.left) / 2 - 200.f,
				mapRectInViewport.top + (mapRectInViewport.bottom - mapRectInViewport.top) / 2 - 20.f);
			m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), "Awaiting Masochists...",
				awaitTextPos - Vector2(2.f, 2.f), Colors::White, 0.f, Vector2(0.f, 0.f), m_clientFrameScale * 3.f);
			m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), "Awaiting Masochists...",
				awaitTextPos, COLOR_APPLE2_VIOLET, 0.f, Vector2(0.f, 0.f), m_clientFrameScale * 3.f);
        }
        
        /*
		// TODO: delete, this is a test
		RECT sTestRect = { 0, 0, 448, 512 };
		RECT dTestRect = { 500, 200, 948, 712 };
		m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapTileSheet),
            GetTextureSize(m_autoMapTexture.Get()), dTestRect, &sTestRect);
        */

		m_spriteBatch->End();
        // End drawing automap

        // Drawing text
        m_spriteBatch->Begin(commandList);

		m_lineEffect->SetProjection(XMMatrixOrthographicOffCenterRH(0, m_cachedClientRect.right - m_cachedClientRect.left, 
            m_cachedClientRect.bottom - m_cachedClientRect.top, 0, 0, 1));
        m_lineEffect->Apply(commandList);
        m_primitiveBatch->Begin(commandList);
        for each (auto sb in m_sbM.sidebars)
        {
            // shifted sidebar position if the window is different size than original size
			XMFLOAT2 shiftedPosition = sb.position;
			switch (sb.type)
			{
            case SidebarTypes::Right:
				shiftedPosition.x += m_cachedClientRect.right - m_cachedClientRect.left - origW;
                break;
            case SidebarTypes::Bottom:
				shiftedPosition.y += m_cachedClientRect.bottom - m_cachedClientRect.top - origH;
                break;
			default:
				break;
			}

            // Draw each block's text
            // shift it by the amount the sidebar was shifted
            XMFLOAT2 sblockPosition;    // shifted block position
            for each (auto b in sb.blocks)
            {
                sblockPosition = b->position;
                sblockPosition.x += shiftedPosition.x - sb.position.x;
                sblockPosition.y += shiftedPosition.y - sb.position.y;
                m_spriteFonts.at(b->fontId)->DrawString(m_spriteBatch.get(), b->text.c_str(),
                    sblockPosition, b->color, 0.f, m_vector2Zero);
            }

            // Now draw a delimiter line for the sidebar
            // TODO: not sure if we want those delimiters
            XMFLOAT3 lstart = XMFLOAT3(shiftedPosition.x, shiftedPosition.y, 0);
            XMFLOAT3 lend = lstart;
            switch (sb.type)
            {
            case SidebarTypes::Right:
                lend.y = lstart.y + m_cachedClientRect.bottom - m_cachedClientRect.top;
                break;
            case SidebarTypes::Bottom:
				lend.x = lstart.x + GetFrameBufferWidth();
                break;
            default:
                break;
            }
            m_primitiveBatch->DrawLine(
                VertexPositionColor(lstart, static_cast<XMFLOAT4>(Colors::DimGray)),
                VertexPositionColor(lend, static_cast<XMFLOAT4>(Colors::Black))
            );
        }
		m_primitiveBatch->End();


#ifdef _DEBUG
		char pcbuf[4000];
		//    snprintf(pcbuf, sizeof(pcbuf), "DEBUG: %I64x : %I64x", g_debug_video_field, g_debug_video_data);
		snprintf(pcbuf, sizeof(pcbuf), "%6.0f usec/frame - Time: %6.2f - Sidebar Time: %6lld\n", 1000000.f / m_timer.GetFramesPerSecond(), m_timer.GetTotalSeconds(), sbTimeSpent);
		m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), pcbuf,
			{ 11.f, 11.f }, Colors::Black, 0.f, m_vector2Zero, 1.f);
		m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), pcbuf,
			{ 10.f, 10.f }, Colors::OrangeRed, 0.f, m_vector2Zero, 1.f);

#endif // _DEBUG
        m_spriteBatch->End();
        // End drawing text

		PIXEndEvent(commandList);

		// Show the new frame.
		PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
		m_deviceResources->Present();
		m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
		PIXEndEvent();
    }
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::Black, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewports and scissor rects.
    // Set the Gamelink viewport as the first default viewport for the geometry shaders to use
    // So we don't have to specifiy the viewport in the shader
    // TODO: figure out how that's done
    // D3D12_VIEWPORT viewports[2] = { m_deviceResources->GetGamelinkViewport(), m_deviceResources->GetScreenViewport() };
    // D3D12_RECT scissorRects[2] = { m_deviceResources->GetScissorRect(), m_deviceResources->GetScissorRect() };
    D3D12_VIEWPORT viewports[1] = { m_deviceResources->GetScreenViewport() };
    D3D12_RECT scissorRects[1] = { m_deviceResources->GetScissorRect() };
    commandList->RSSetViewports(1, viewports);
    commandList->RSSetScissorRects(1, scissorRects);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    shouldRender = false;
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();
    shouldRender = true;
}

void Game::OnWindowChanged()
{
	// Something's changed, let's update pos and size
    RECT cR;
    GetClientRect(m_window, &cR);
    OnWindowMoved(cR.left, cR.top);
	OnWindowSizeChanged(cR.right-cR.left, cR.bottom-cR.top);
}

void Game::OnWindowMoved(LONG x, LONG y)
{
    LONG width = m_cachedClientRect.right - m_cachedClientRect.left;
    LONG height = m_cachedClientRect.bottom - m_cachedClientRect.top;
    m_cachedClientRect.left = x;
    m_cachedClientRect.right = x + width;
	m_cachedClientRect.top = y;
	m_cachedClientRect.bottom = y + height;
}

void Game::OnWindowSizeChanged(LONG width, LONG height)
{
    m_cachedClientRect.right = m_cachedClientRect.left + width;
	m_cachedClientRect.bottom = m_cachedClientRect.top + height;
    RECT outSize;
    int origW, origH;
    GetBaseSize(origW, origH);
    float scaleW = (float)(width) / (float)origW;
    float scaleH = (float)(height) / (float)origH;
    
//     char buf[300];
//     snprintf(buf, 300, "Scales are; %.2f , %.2f\n", scaleW, scaleH);
//     OutputDebugStringA(buf);
    
    float gamelinkWidth, gamelinkHeight;
    if (scaleW > scaleH)
        scaleW = scaleH;
    else
        scaleH = scaleW;
	gamelinkWidth = scaleW * (float)GetFrameBufferWidth();
	gamelinkHeight = scaleH * (float)GetFrameBufferHeight();

    // don't check return status of the below because we still want to recreate the vertex data
    if (m_deviceResources->WindowSizeChanged(&outSize, &m_cachedClientRect, gamelinkWidth, gamelinkHeight))
    {
        CreateWindowSizeDependentResources();
    }
    UpdateGamelinkVertexData(gamelinkWidth, gamelinkHeight, 
	(float)GetFrameBufferWidth() / (float)origW, (float)GetFrameBufferHeight() / (float)origH);
}

#pragma endregion

#pragma region Menu commands

void Game::MenuActivateProfile()
{
    m_sbC.LoadProfileUsingDialog(&m_sbM);
    SetWindowSizeOnChangedProfile();
	g_nonVolatile.SaveToDisk();
}

void Game::MenuDeactivateProfile()
{
    m_sbC.ClearActiveProfile(&m_sbM);
    SetWindowSizeOnChangedProfile();
	g_nonVolatile.SaveToDisk();
}

void Game::MenuShowLogWindow()
{
    m_logWindow = GetLogWindow();
    m_logWindow->ShowLogWindow();
}

void Game::MenuToggleLogWindow()
{
	m_logWindow = GetLogWindow();
    if (m_logWindow->IsLogWindowDisplayed())
        m_logWindow->HideLogWindow();
    else
        m_logWindow->ShowLogWindow();
}

void Game::MenuToggleHacksWindow()
{
	m_dlHacks = GetDeathlordHacks();
	if (m_dlHacks->IsHacksWindowDisplayed())
        m_dlHacks->HideHacksWindow();
	else
        m_dlHacks->ShowHacksWindow();
}
#pragma endregion

#pragma region Direct3D Resources

// The tile spritemap needs to update at every level change
void Game::CreateNewTileSpriteMap()
{
// Sprite sheet only exists when the player is in-game! Make sure this is called when player goes in game
// And make sure the map only renders when in game.

	m_tileset = GetTilesetCreator();

	auto device = m_deviceResources->GetD3DDevice();
    LoadTextureFromMemory((const unsigned char*)m_tileset->GetCurrentTilesetBuffer(),
        device, &m_autoMapTexture, DXGI_FORMAT_R8G8B8A8_UNORM, PNGBUFFERWIDTH, PNGBUFFERHEIGHT);
    //OutputDebugStringA("Loaded map into GPU\n");
}

// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto command_queue = m_deviceResources->GetCommandQueue();
    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    /// <summary>
    /// Start of resource uploading to GPU
    /// </summary>
	m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, (int)TextureDescriptors::Count);

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    // Create the sprite fonts based on FontsAvailable
    m_spriteFonts.clear();
    wchar_t buff[MAX_PATH];
    for each (auto aFont in m_sbM.fontsAvailable)
    {
        DX::FindMediaFile(buff, MAX_PATH, aFont.second.c_str());
		m_spriteFonts[aFont.first] = std::make_unique<SpriteFont>(device, resourceUpload, buff,
				m_resourceDescriptors->GetCpuHandle((int)aFont.first),
				m_resourceDescriptors->GetGpuHandle((int)aFont.first));
		m_spriteFonts.at(aFont.first)->SetDefaultCharacter('.');
    }

    // Now create the automap resources
    // First the background
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, resourceUpload, L"Assets/Background_NoMap.png",
            m_autoMapTextureBG.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_autoMapTextureBG.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapBackground));

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
	SpriteBatchPipelineStateDescription spd(rtState);
	m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, spd);

    auto uploadResourcesFinished = resourceUpload.End(command_queue);
    uploadResourcesFinished.wait();

    //////////////////////////////////////////////////

    /// <summary>
    /// Start of AppleWin texture info uploading to GPU
    /// </summary>

    // Create a root signature with one sampler and one texture
    {
        CD3DX12_DESCRIPTOR_RANGE descRange = {};
        descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

        CD3DX12_ROOT_PARAMETER rp = {};
        rp.InitAsDescriptorTable(1, &descRange, D3D12_SHADER_VISIBILITY_PIXEL);

        // Use a static sampler that matches the defaults
        // https://msdn.microsoft.com/en-us/library/windows/desktop/dn913202(v=vs.85).aspx#static_sampler
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Init(1, &rp, 1, &samplerDesc,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (FAILED(hr))
        {
            if (error)
            {
                OutputDebugStringA(reinterpret_cast<const char*>(error->GetBufferPointer()));
            }
            throw DX::com_exception(hr);
        }

        DX::ThrowIfFailed(
            device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));
    }

    // Create the pipeline state, which includes loading shaders.
    auto vertexShaderBlob = DX::ReadData(L"VertexShader.cso");

    auto pixelShaderBlob = DX::ReadData(L"PixelShader.cso");

    static const D3D12_INPUT_ELEMENT_DESC s_inputElementDesc[2] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,  0 },
        { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,  0 },
    };

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { s_inputElementDesc, _countof(s_inputElementDesc) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { vertexShaderBlob.data(), vertexShaderBlob.size() };
    psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DSVFormat = m_deviceResources->GetDepthBufferFormat();
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
    psoDesc.SampleDesc.Count = 1;
    DX::ThrowIfFailed(
        device->CreateGraphicsPipelineState(&psoDesc,
            IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())));

    CD3DX12_HEAP_PROPERTIES heapUpload(D3D12_HEAP_TYPE_UPLOAD);

    // Create vertex buffer.

    {
        Vertex s_vertexData[4];
        SetVertexData(s_vertexData, 1.f, 1.f, m_currentLayout);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(s_vertexData));

        DX::ThrowIfFailed(
            device->CreateCommittedResource(&heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf())));

        // Copy the quad data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(
            m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, s_vertexData, sizeof(s_vertexData));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = sizeof(s_vertexData);
    }

    // Create index buffer.
    {
        static const uint16_t s_indexData[6] =
        {
            3,1,0,
            2,1,3,
        };

        // See note above
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(s_indexData));

        DX::ThrowIfFailed(
            device->CreateCommittedResource(&heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_indexBuffer.ReleaseAndGetAddressOf())));

        // Copy the data to the index buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(
            m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, s_indexData, sizeof(s_indexData));
        m_indexBuffer->Unmap(0, nullptr);

        // Initialize the index buffer view.
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        m_indexBufferView.SizeInBytes = sizeof(s_indexData);
    }

    // Create texture.
    auto commandList = m_deviceResources->GetCommandList();
    commandList->Reset(m_deviceResources->GetCommandAllocator(), nullptr);

    {
        D3D12_RESOURCE_DESC txtDesc = ChooseTexture();

        CD3DX12_HEAP_PROPERTIES heapDefault(D3D12_HEAP_TYPE_DEFAULT);

        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &heapDefault,
                D3D12_HEAP_FLAG_NONE,
                &txtDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(m_texture.ReleaseAndGetAddressOf())));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

        // Create the GPU upload buffer.
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(g_textureUploadHeap.GetAddressOf())));

        UpdateSubresources(commandList, m_texture.Get(), g_textureUploadHeap.Get(), 0, 0, 1, &g_textureData);

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = txtDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(m_texture.Get(), &srvDesc, 
            m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::Apple2Video));
    }

    /// <summary>
    /// Set up PrimitiveBatch to draw the lines that will delimit the sidebars
    /// https://github.com/microsoft/DirectXTK12/wiki/PrimitiveBatch
    /// </summary>

    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(device);

    EffectPipelineStateDescription epd(
        &VertexPositionColor::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        rtState,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

    m_lineEffect = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, epd);

    m_lineEffect->SetProjection(XMMatrixOrthographicOffCenterRH(0, GetFrameBufferWidth(), GetFrameBufferHeight(), 0, 0, 1));

    /// <summary>
    /// Finish
    /// </summary>

    DX::ThrowIfFailed(commandList->Close());
    m_deviceResources->GetCommandQueue()->ExecuteCommandLists(1, CommandListCast(&commandList));

    // Wait until assets have been uploaded to the GPU.
    m_deviceResources->WaitForGpu();

}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);
}

void Game::SetVertexData(Vertex* v, float wRatio, float hRatio, EmulatorLayout layout)
{
	float wR = 1.f - (1.f - wRatio) * 2.f;
	float hR = -1.f + (1.f - hRatio) * 2.f;

    v[0] = { { -1.0f, hR, 0.5f, 1.0f },    { 0.f, 1.f } };
    v[1] = { { wR, hR, 0.5f, 1.0f },       { 1.f, 1.f } };
    v[2] = { { wR,  1.0f, 0.5f, 1.0f },    { 1.f, 0.f } };
    v[3] = { { -1.0f,  1.0f, 0.5f, 1.0f }, { 0.f, 0.f } };

	switch (layout)
	{
	case EmulatorLayout::FLIPPED_X:
        v[0].texcoord = { 1.f, 0.f };
        v[1].texcoord = { 0.f, 0.f };
        v[2].texcoord = { 0.f, 1.f };
        v[3].texcoord = { 1.f, 1.f };
		break;
	case EmulatorLayout::FLIPPED_Y:
        v[0].texcoord = { 0.f, 0.f };
        v[1].texcoord = { 1.f, 0.f };
        v[2].texcoord = { 1.f, 1.f };
        v[3].texcoord = { 0.f, 1.f };
		break;
	case EmulatorLayout::FLIPPED_XY:
        v[0].texcoord = { 1.f, 1.f };
        v[1].texcoord = { 0.f, 1.f };
        v[2].texcoord = { 0.f, 0.f };
        v[3].texcoord = { 1.f, 0.f };
		break;
	default:
		break;
	}
}

// This method must be called when sidebars are added or deleted
// so the relative vertex boundaries can be updated to stay within
// the aspect ratio of the gamelink texture. Pass in width and height ratios
// (i.e. how much of the width|height the vertex should fill, 1.f being full width|height)
void Game::UpdateGamelinkVertexData(int width, int height, float wRatio, float hRatio)
{
    {
        Vertex s_vertexData[4];
        SetVertexData(s_vertexData, wRatio, hRatio, m_currentLayout);

        // Copy the quad data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(
            m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, s_vertexData, sizeof(s_vertexData));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = sizeof(s_vertexData);
    }

    // Update index buffer.
    {
        static const uint16_t s_indexData[6] =
        {
            3,1,0,
            2,1,3,
        };

        // Copy the data to the index buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(
            m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, s_indexData, sizeof(s_indexData));
        m_indexBuffer->Unmap(0, nullptr);

        // Initialize the index buffer view.
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        m_indexBufferView.SizeInBytes = sizeof(s_indexData);
    }

    // And update the projection for line drawing
    m_lineEffect->SetProjection(XMMatrixOrthographicOffCenterRH(0, (float)width, (float)height, 0, 0, 1));

}

void Game::OnDeviceLost()
{
    // Reset fonts
    for (auto it = m_spriteFonts.begin(); it != m_spriteFonts.end(); it++)
    {
        it->second.reset();
    }
	m_texture.Reset();
	m_autoMapTexture.Reset();
	m_autoMapTextureBG.Reset();
    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();
    m_pipelineState.Reset();
    m_rootSignature.Reset();
	m_resourceDescriptors.reset();
    m_spriteBatch.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

#pragma region Utilities

void Game::ActivateLastUsedProfile()
{
	// Autoload the last used profile
	std::string profileName = m_sbC.OpenProfile(g_nonVolatile.profilePath);
	m_sbC.setActiveProfile(&m_sbM, &profileName);
    SetWindowSizeOnChangedProfile();
}


#pragma endregion

#pragma region DX Helpers

// Simple helper function to load an image as a R8B8G8 memory buffer into a DX12 texture with common settings
// Returns true on success, with the SRV CPU handle having an SRV for the newly-created texture placed in it
// (srv_cpu_handle must be a handle in a valid descriptor heap)
bool Game::LoadTextureFromMemory (const unsigned char* image_data,
    ID3D12Device* d3d_device, Microsoft::WRL::ComPtr <ID3D12Resource>* out_tex_resource,
    DXGI_FORMAT tex_format, int width, int height)
{
	int image_width = width;
	int image_height = height;
	if (image_data == NULL)
		return false;

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
	d3d_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(pTexture, 0, 1);

	props.Type = D3D12_HEAP_TYPE_UPLOAD;
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// Create the GPU upload buffer.
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

	DX::ThrowIfFailed(
        d3d_device->CreateCommittedResource(
			&props,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(g_textureUploadHeapMap.GetAddressOf())));

    g_textureDataMap.pData = image_data;
    g_textureDataMap.SlicePitch = static_cast<UINT64>(width) * height * sizeof(bgra_t);
    g_textureDataMap.RowPitch = static_cast<LONG_PTR>(desc.Width * sizeof(uint32_t));

	auto commandList = m_deviceResources->GetCommandList();
	commandList->Reset(m_deviceResources->GetCommandAllocator(), nullptr);
	

    UpdateSubresources(commandList, pTexture, g_textureUploadHeapMap.Get(), 0, 0, 1, &g_textureDataMap);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	// Describe and create a SRV for the texture.
    // Using CreateShaderResourceView removes a ton of boilerplate
	CreateShaderResourceView(d3d_device, pTexture,
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
