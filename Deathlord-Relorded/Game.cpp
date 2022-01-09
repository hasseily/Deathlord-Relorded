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
#include "LogWindow.h"
#include "SpellWindow.h"
#include "TilesetCreator.h"
#include "AutoMap.h"
#include "InvOverlay.h"
#include "TextOutput.h"
#include "AppleWinDXVideo.h"
#include "MiniMap.h"
#include "Daytime.h"
#include "PartyLayout.h"
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


// Instance variables
HWND m_window;
static SidebarManager m_sbM;
static SidebarContent m_sbC;
Mouse::ButtonStateTracker moTracker;
Keyboard::KeyboardStateTracker kbTracker;

static std::shared_ptr<LogWindow>m_logWindow;
static std::shared_ptr<SpellWindow>m_spellWindow;
static std::shared_ptr<DeathlordHacks>m_dlHacks;
static InvOverlay* m_invOverlay;
static TextOutput* m_textOutput;
static AutoMap* m_autoMap;
static AppleWinDXVideo* m_a2Video;
static Daytime* m_daytime;
static MiniMap* m_minimap;
static PartyLayout* m_partyLayout;

AppMode_e m_previousAppMode = AppMode_e::MODE_UNKNOWN;

static std::wstring last_logged_line;
static UINT64 tickOfLastRender = 0;

static Vector2 m_vector2Zero = { 0.f, 0.f };

static MemoryTriggers* m_trigger;
UINT64	g_debug_video_field = 0;
UINT64	g_debug_video_data = 0;
NonVolatile g_nonVolatile;

bool g_isInGameMap = false;
bool g_hasBeenIdleOnce = false;
bool g_isInBattle = false;
bool g_wantsToSave = true;  // DISABLED. It can corrupt saved games
int g_debugLogInstructions = 0;    // Tapping "End" key logs the next 100,000 instructions
float Game::m_clientFrameScale = 1.f;

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
    if (g_isInGameMap)
        AutoMap::GetInstance()->SaveCurrentMapInfo();
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
    GameLink::Term();
}

// Initialize the resources required to run.
void Game::Initialize(HWND window)
{
    m_window = window;

	// Use a variable timestep to give as much time to the emulator as possible
// Then we control how often we render later in the Render() method
// m_timer.SetTargetElapsedSeconds(1.0 / MAX_RENDERED_FRAMES_PER_SECOND);
	m_timer.SetFixedTimeStep(false);

    m_gamePad = std::make_unique<GamePad>();
    m_keyboard = std::make_unique<Keyboard>();
	m_mouse = std::make_unique<Mouse>();

	m_mouse->SetWindow(window);

    g_nonVolatile.LoadFromDisk();

	// Initialize emulator
	EmulatorOneTimeInitialization(window);
	EmulatorRepeatInitialization();

	m_sbC.Initialize();

    shouldRender = true;
    m_clientFrameScale = 1.f;

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

	SimpleMath::Rectangle winrct = GetDrawRectangle();
	m_deviceResources->SetWindow(window, winrct.width, winrct.height);
    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

	m_trigger = MemoryTriggers::GetInstance(&m_timer);
	m_trigger->PollMapSetCurrentValues();
}

#pragma region Window texture and size

void Game::SetWindowSizeOnChangedProfile()
{
    // TODO: Remove this function

}
#pragma endregion

#pragma region Others

// Returns the rectangle in which to draw in the window.
// We never want to draw outside of it
// We use the main game texture as the size of the rectangle
// and it is centered in the window
SimpleMath::Rectangle Game::GetDrawRectangle()
{
    // The below would get the size of the monitor
/*
HMONITOR monitor = MonitorFromWindow(m_window, MONITOR_DEFAULTTONEAREST);
MONITORINFO info;
info.cbSize = sizeof(MONITORINFO);
GetMonitorInfo(monitor, &info);
width = info.rcMonitor.right - info.rcMonitor.left;
height = info.rcMonitor.bottom - info.rcMonitor.top;
*/
    if (!m_gameTextureBG)
        return SimpleMath::Rectangle::Rectangle();
    auto texSize = GetTextureSize(m_gameTextureBG.Get());
	RECT clientRect;
	bool res = GetClientRect(m_window, &clientRect);
    if (!res)
    {
        AlertIfError(m_window);
    }
    // GetClientRect always has its rect origin at 0,0.
    // Hence the right and bottom are the width and height
    int leftMargin, topMargin;
    if (clientRect.right > texSize.x)
        leftMargin = (clientRect.right - texSize.x) / 2;
    else
        leftMargin = 0;
    if (clientRect.bottom > texSize.y)
        topMargin = (clientRect.bottom - texSize.y) / 2;
    else
        topMargin = 0;
    SimpleMath::Rectangle r = SimpleMath::Rectangle(leftMargin, topMargin, texSize.x, texSize.y);
    return r;
}


DirectX::SpriteFont* Game::GetSpriteFontAtIndex(FontDescriptors fontIndex)
{
    return m_spriteFonts.at(fontIndex).get();
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

	UINT8* memPtr = MemGetMainPtr(0);
	g_isInGameMap = (memPtr[MAP_IS_IN_GAME_MAP] == 0xE5);	// when not in game map, that area is all zeros
    g_isInBattle = (memPtr[MEM_MODULE_STATE] == (int)ModuleStates::Combat);
    if (g_isInGameMap)
    {    // Always set speed to SPEED_NORMAL when playing, otherwise things are too fast
		if (EmulatorGetSpeed() != 1)
			EmulatorSetSpeed(1);    // SPEED_NORMAL
    }
    else
    {
		if (EmulatorGetSpeed() != g_nonVolatile.speed)
			EmulatorSetSpeed(g_nonVolatile.speed);
    }

    // Now parse input
    // Pad is unused, just kept here for reference
	auto pad = m_gamePad->GetState(0);
	if (pad.IsConnected())
	{
		if (pad.IsViewPressed())
		{
			// Do something when gamepad keys are pressed
		}
	}

    // Modern keyboard handling
    // Keystrokes are passed to AppleWin in Main.cpp before getting here
    // So the only thing one should do here is handle special keys that
    // we know don't do anything under the emulated Deathlord
	auto kb = m_keyboard->GetState();
	kbTracker.Update(kb);
	if (g_isInGameMap)
	{
        if (kbTracker.pressed.Insert)
            m_invOverlay->ToggleInvOverlay();
        if (kbTracker.pressed.F11)
            m_a2Video->ToggleApple2Video();
	}
#ifdef _DEBUG
    // Poor man's 6502 instructions history
    // Press the END key to log the next 100k instructions
    if (kbTracker.pressed.End)
    {
        g_debugLogInstructions = 100000;
    }
#endif

    // Modern mouse handling
	using ButtonState = Mouse::ButtonStateTracker::ButtonState;
	auto mo = m_mouse->GetState();
	moTracker.Update(mo);
    if (m_invOverlay->IsInvOverlayDisplayed())
    {
        m_invOverlay->UpdateState();
        m_invOverlay->MousePosInPixels(mo.x, mo.y);
		if (moTracker.leftButton == ButtonState::PRESSED)
		{
            m_invOverlay->LeftMouseButtonClicked(moTracker.GetLastState().x, moTracker.GetLastState().y);
		}
    }

    // Should we pause the emulator?
    // Not pausing it allows us to get automatic feedback of inventory changes
    EmulatorMessageLoopProcessing();

    auto autoMap = AutoMap::GetInstance();
    auto memTriggers = MemoryTriggers::GetInstance();
    if (g_isInGameMap && (autoMap != NULL))
    {
		memTriggers->PollKeyMemoryLocations();  // But not the avatar XY
    }
    m_minimap->Update(memPtr[MAP_OVERLAND_X], memPtr[MAP_OVERLAND_Y]);
    m_partyLayout->Update(MemGetMainPtr(PARTY_CHAR_LEADER)[0]);

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
		OnWindowMoved();
	}

    // change in video layout Normal/Flipped
    if (m_previousLayout != m_currentLayout)
    {
        OnWindowMoved();
    }

    // Only render 30 times a second. No need to render more often.
    // The tick loop is running as fast as possible so that the emulator runs as fast as possible
    // Here we'll just decide whether to render or not

    UINT64 ticksSinceLastRender = m_timer.GetTotalTicks() - tickOfLastRender;
    if (ticksSinceLastRender > m_timer.TicksPerSecond / MAX_RENDERED_FRAMES_PER_SECOND)
    {
        tickOfLastRender = m_timer.GetTotalTicks();
        SimpleMath::Rectangle r = GetDrawRectangle();   // TODO: Cache this?

        // First update the sidebar, it doesn't need to be updated until right before the render
        // Only allow x microseconds for updates of sidebar every render
        UINT64 sbTimeSpent = m_sbC.UpdateAllSidebarText(&m_sbM, false, 500);

        // Prepare the command list to render a new frame.
        m_deviceResources->Prepare();
        Clear();

        auto commandList = m_deviceResources->GetCommandList();
        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

		// Now set the command list data
		ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
		commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

		// Get the window size
        // We'll need it for the effects projection matrix
		RECT clientRect;
		GetClientRect(m_window, &clientRect);

        // Now render
	    if (!g_isInGameMap)
		{
            // Only run the original game if not in the game map already
			m_a2Video->Render(SimpleMath::Rectangle(clientRect), m_uploadBatch.get());
        }
        else
        {
			m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
			m_spriteBatch->Begin(commandList, m_states->LinearClamp(), SpriteSortMode_Deferred);

			// Draw the game background
			auto mmBGTexSize = GetTextureSize(m_gameTextureBG.Get());
			m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::MainBackground), mmBGTexSize,
				r, nullptr, Colors::White, 0.f, XMFLOAT2());
			// End drawing the game background
            
#if 0   // No more sidebars
			// Now time to draw the text and lines
			m_dxtEffectLines->SetProjection(XMMatrixOrthographicOffCenterRH(clientRect.left, clientRect.right, clientRect.bottom, clientRect.top, 0, 1));
			m_dxtEffectLines->Apply(commandList);
			m_primitiveBatchLines->Begin(commandList);
			for each (auto sb in m_sbM.sidebars)
			{

				// shifted sidebar position if the window is different size than original size
				XMFLOAT2 shiftedPosition = sb.position;
				switch (sb.type)
				{
				case SidebarTypes::Right:
					shiftedPosition.x += r.x;
					break;
				case SidebarTypes::Bottom:
					shiftedPosition.y += r.x;
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
					lend.y = lstart.y + r.height;
					break;
				case SidebarTypes::Bottom:
					lend.x = lstart.x + GetFrameBufferWidth();
					break;
				default:
					break;
				}
				m_primitiveBatchLines->DrawLine(
					VertexPositionColor(lstart, static_cast<XMFLOAT4>(Colors::DimGray)),
					VertexPositionColor(lend, static_cast<XMFLOAT4>(Colors::Black))
				);
			}
			m_primitiveBatchLines->End();
#endif

#if 1
			char pcbuf[4000];
			//    snprintf(pcbuf, sizeof(pcbuf), "DEBUG: %I64x : %I64x", g_debug_video_field, g_debug_video_data);
			snprintf(pcbuf, sizeof(pcbuf), "%.2d FPS , %6.0f usec/frame - Time: %6.2f - Sidebar Time: %6lld\n", 
                m_timer.GetFramesPerSecond(), 1000000.f / m_timer.GetFramesPerSecond(), m_timer.GetTotalSeconds(), sbTimeSpent);
			m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), pcbuf,
				{ 11.f, 11.f }, Colors::Black, 0.f, m_vector2Zero, 1.f);
			m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), pcbuf,
				{ 10.f, 10.f }, Colors::OrangeRed, 0.f, m_vector2Zero, 1.f);
#endif // _DEBUG
			m_spriteBatch->End();

			if (g_nonVolatile.showMap)  // TODO: Always show the map
			{
				// Now draw autoMap
				auto mmOrigin = Vector2(r.x + 361.f, r.y + 10.f);
				RECT mapRectInViewport = {
					mmOrigin.x,
					mmOrigin.y,
					mmOrigin.x + MAP_WIDTH_IN_VIEWPORT,
					mmOrigin.y + MAP_WIDTH_IN_VIEWPORT * PNGTH / PNGTW
				};
				m_autoMap->DrawAutoMap(m_spriteBatch, m_states.get(), &mapRectInViewport);
				// End drawing autoMap

				// now draw the hidden layer around the player if he's allowed to see it
				// inside the original Deathlord viewport
				m_autoMap->ConditionallyDisplayHiddenLayerAroundPlayer(m_spriteBatch, m_states.get());
			}


            // Now draw everything else that's in the main viewport
            // We use the same vp, primitivebatch and spriteBatch for all.
            // Only the main automap area was different due to its special handling of vp and scissors
			D3D12_VIEWPORT vp = GetCurrentViewport();
			m_dxtEffectTriangles->SetProjection(XMMatrixOrthographicOffCenterRH(clientRect.left, clientRect.right, clientRect.bottom, clientRect.top, 0, 1));
			m_dxtEffectTriangles->Apply(commandList);
			m_primitiveBatchTriangles->Begin(commandList);
			m_spriteBatch->Begin(commandList, SpriteSortMode_Deferred);

			m_partyLayout->Render(r, m_spriteBatch.get());
            m_minimap->Render(r, m_spriteBatch.get());
            m_daytime->Render(r, m_spriteBatch.get());
			m_textOutput->Render(r, m_spriteBatch.get());
			m_primitiveBatchTriangles->End();
			m_spriteBatch->End();
			// End drawing everything else that's in the main viewport

            // Draw now the overlays, each is independent
            // The inventory overlay
			if (m_invOverlay->IsInvOverlayDisplayed())
				m_invOverlay->Render(SimpleMath::Rectangle(clientRect));

            // The apple2 video is unique and independent
            // It should be displayed at the top if requested
			if (m_a2Video->IsApple2VideoDisplayed())
				m_a2Video->Render(SimpleMath::Rectangle(clientRect), m_uploadBatch.get());

        }   // end if !g_isInGameMap

        // Now check if the game is paused and display an overlay
        if (g_nAppMode == AppMode_e::MODE_PAUSED)
        {
            std::wstring pauseWStr = L"GAME PAUSED";
            float pauseScale = 5.f;
            auto pauseOrigin = r.Center(); // start with dead center
            pauseOrigin.x -= (pauseWStr.length() * pauseScale * 5)/2;
            pauseOrigin.y -= pauseScale * 14 / 2;
			m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
			m_spriteBatch->Begin(commandList, m_states->LinearClamp(), SpriteSortMode_Deferred);
			m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), pauseWStr.c_str(),
                pauseOrigin - Vector2(pauseScale/2, pauseScale/2), Colors::Black, 0.f, m_vector2Zero, pauseScale);
			m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), pauseWStr.c_str(),
                pauseOrigin, Colors::OrangeRed, 0.f, m_vector2Zero, pauseScale);
            m_spriteBatch->End();
        }

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
    // TODO: we do not manually overdraw what has been modified, we redraw every frame completely every time
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::Black, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewports and scissor rects.
    // Those get modified when displaying the automap, but get reverted to the below after that
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
    moTracker.Reset();
	kbTracker.Reset();
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();
	moTracker.Reset();
	kbTracker.Reset();
    shouldRender = true;
}

void Game::OnWindowMoved()
{
	// TODO: Get rid of this method?
	auto r = m_deviceResources->GetOutputSize();
	m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(LONG width, LONG height)
{
	if (!m_deviceResources->WindowSizeChanged(width, height))
		return;

	CreateWindowSizeDependentResources();
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

void Game::MenuShowSpellWindow()
{
    m_spellWindow = GetSpellWindow();
    m_spellWindow->ShowSpellWindow();
	g_nonVolatile.showSpells = true;
	g_nonVolatile.SaveToDisk();
}

void Game::MenuToggleLogWindow()
{
	m_logWindow = GetLogWindow();
    if (m_logWindow->IsLogWindowDisplayed())
        m_logWindow->HideLogWindow();
    else
        m_logWindow->ShowLogWindow();
}

void Game::MenuToggleSpellWindow()
{
	m_spellWindow = GetSpellWindow();
	if (m_spellWindow->IsSpellWindowDisplayed())
        m_spellWindow->HideSpellWindow();
	else
        m_spellWindow->ShowSpellWindow();
	g_nonVolatile.showSpells = m_spellWindow->IsSpellWindowDisplayed();
	g_nonVolatile.SaveToDisk();
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

// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto command_queue = m_deviceResources->GetCommandQueue();
    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);
    m_uploadBatch = std::make_shared<ResourceUploadBatch>(device);

    /// <summary>
    /// Start of resource uploading to GPU
    /// </summary>
	m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, (int)TextureDescriptors::Count);

    m_uploadBatch->Begin();

    // Upload the background of the main window
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *m_uploadBatch, L"Assets/Background_Relorded.png",
            m_gameTextureBG.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_gameTextureBG.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::MainBackground));

    // Create the sprite fonts based on FontsAvailable
    m_spriteFonts.clear();
    wchar_t buff[MAX_PATH];
	map<FontDescriptors, wstring> fontsAvailable;
	fontsAvailable[FontDescriptors::FontA2Regular] = L"a2-12pt.spritefont";
	fontsAvailable[FontDescriptors::FontA2Bold] = L"a2-12pt-bold.spritefont";
	fontsAvailable[FontDescriptors::FontA2Italic] = L"a2-12pt-italic.spritefont";
	fontsAvailable[FontDescriptors::FontA2BoldItalic] = L"a2-12pt-bolditalic.spritefont";
	fontsAvailable[FontDescriptors::FontPR3Regular] = L"pr3-dlcharset-12pt.spritefont";
	fontsAvailable[FontDescriptors::FontPR3Inverse] = L"pr3-dlcharset-12pt-inverse.spritefont";
	fontsAvailable[FontDescriptors::FontDLRegular] = L"dlfont-12pt.spritefont";
	fontsAvailable[FontDescriptors::FontDLInverse] = L"dlfont-12pt-inverse.spritefont";
    for each (auto aFont in fontsAvailable)
    {
        DX::FindMediaFile(buff, MAX_PATH, aFont.second.c_str());
		m_spriteFonts[aFont.first] = std::make_unique<SpriteFont>(device, *m_uploadBatch, buff,
				m_resourceDescriptors->GetCpuHandle((int)aFont.first),
				m_resourceDescriptors->GetGpuHandle((int)aFont.first));
		m_spriteFonts.at(aFont.first)->SetDefaultCharacter('.');
    }

    // Now initialize the pieces of the UI, and create the resources
	m_states = std::make_unique<CommonStates>(device);

    m_autoMap = AutoMap::GetInstance(m_deviceResources, m_resourceDescriptors);
    m_autoMap->CreateDeviceDependentResources(m_uploadBatch.get());
    m_textOutput = TextOutput::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_invOverlay = InvOverlay::GetInstance(m_deviceResources, m_resourceDescriptors);
    m_invOverlay->CreateDeviceDependentResources(m_uploadBatch.get(), m_states.get());
	m_a2Video = AppleWinDXVideo::GetInstance(m_deviceResources, m_resourceDescriptors);
    m_a2Video->CreateDeviceDependentResources(m_uploadBatch.get(), m_states.get());
    m_minimap = MiniMap::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_minimap->CreateDeviceDependentResources(m_uploadBatch.get());
	m_daytime = Daytime::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_daytime->CreateDeviceDependentResources(m_uploadBatch.get());
	m_partyLayout = PartyLayout::GetInstance(m_deviceResources, m_resourceDescriptors);
    m_partyLayout->CreateDeviceDependentResources(m_uploadBatch.get());

    // Do the sprite batches.
	auto sampler = m_states->LinearWrap();
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
    // TODO: Right now spritesheets are NonPremultiplied
    // Optimize to AlphaBlend?
	SpriteBatchPipelineStateDescription spd(rtState, &CommonStates::NonPremultiplied, nullptr, nullptr, &sampler);
	m_spriteBatch = std::make_unique<SpriteBatch>(device, *m_uploadBatch.get(), spd);


    auto uploadResourcesFinished = m_uploadBatch->End(command_queue);
    uploadResourcesFinished.wait();


    /// <summary>
    /// Set up PrimitiveBatch to draw the lines and triangles for sidebars and inventory
    /// https://github.com/microsoft/DirectXTK12/wiki/PrimitiveBatch
    /// </summary>
    /// 
    EffectPipelineStateDescription epdLines(
        &VertexPositionColor::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullCounterClockwise,
        rtState,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
    m_dxtEffectLines = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, epdLines);
    m_dxtEffectLines->SetProjection(XMMatrixOrthographicOffCenterRH(0, GetFrameBufferWidth(), GetFrameBufferHeight(), 0, 0, 1));
	m_primitiveBatchLines = std::make_shared<PrimitiveBatch<VertexPositionColor>>(device);
	EffectPipelineStateDescription epdTriangles(
		&VertexPositionColor::InputLayout,
		CommonStates::Opaque,
		CommonStates::DepthDefault,
		CommonStates::CullCounterClockwise,
		rtState,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_dxtEffectTriangles = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, epdTriangles);
    m_dxtEffectTriangles->SetProjection(XMMatrixOrthographicOffCenterRH(0, GetFrameBufferWidth(), GetFrameBufferHeight(), 0, 0, 1));
	m_primitiveBatchTriangles = std::make_shared<PrimitiveBatch<VertexPositionColor>>(device);

    /// <summary>
    /// Finish
    /// </summary>

    // Wait until assets have been uploaded to the GPU.
    m_deviceResources->WaitForGpu();

}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    m_spriteBatch->SetViewport(GetCurrentViewport());
}

void Game::OnDeviceLost()
{
    // Reset fonts
    for (auto it = m_spriteFonts.begin(); it != m_spriteFonts.end(); it++)
    {
        it->second.reset();
    }
    m_autoMap->OnDeviceLost();
	m_invOverlay->OnDeviceLost();
	m_a2Video->OnDeviceLost();
	m_minimap->OnDeviceLost();
	m_daytime->OnDeviceLost();
	m_partyLayout->OnDeviceLost();

	m_gameTextureBG.Reset();
    m_states.reset();
    m_spriteBatch.reset();
    m_graphicsMemory.reset();
	m_primitiveBatchLines.reset();
	m_primitiveBatchTriangles.reset();
	m_dxtEffectLines.reset();
	m_dxtEffectTriangles.reset();
	m_uploadBatch.reset();
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

