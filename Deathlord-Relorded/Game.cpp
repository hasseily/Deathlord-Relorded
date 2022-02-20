//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
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
#include "BattleOverlay.h"
#include "GameOverOverlay.h"
#include "GameLoadingOverlay.h"
#include "TextOutput.h"
#include "AppleWinDXVideo.h"
#include "MiniMap.h"
#include "Daytime.h"
#include "PartyLayout.h"
#include "AnimTextManager.h"
#include "AnimSpriteManager.h"
#include <vector>
#include <string>
#include <map>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;
using namespace HA;

extern void SetSendKeystrokesToAppleWin(bool shouldSend);
extern int rerollCount;

// AppleWin video texture
D3D12_SUBRESOURCE_DATA g_textureData;
ComPtr<ID3D12Resource> g_textureUploadHeap;


// Instance variables
HWND m_window;
Mouse::ButtonStateTracker moTracker;
Keyboard::KeyboardStateTracker kbTracker;

static std::shared_ptr<LogWindow>m_logWindow;
static std::shared_ptr<SpellWindow>m_spellWindow;
static std::shared_ptr<DeathlordHacks>m_dlHacks;
static InvOverlay* m_invOverlay;
static BattleOverlay* m_battleOverlay;
static GameOverOverlay* m_gameOverOverlay;
static GameLoadingOverlay* m_gameLoadingOverlay;
static TextOutput* m_textOutput;
static AutoMap* m_autoMap;
static AppleWinDXVideo* m_a2Video;
static Daytime* m_daytime;
static MiniMap* m_minimap;
static PartyLayout* m_partyLayout;
static AnimTextManager* m_animTextManager;
static AnimSpriteManager* m_animSpriteManager;

AppMode_e m_previousAppMode = AppMode_e::MODE_UNKNOWN;

static std::wstring last_logged_line;
static UINT64 tickOfLastRender = 0;

static std::wstring s_hourglass = std::wstring(1, '\u005B'); // horizontal bar that will spin

static Vector2 m_vector2Zero = { 0.f, 0.f };

static MemoryTriggers* m_trigger;
UINT64	g_debug_video_field = 0;
UINT64	g_debug_video_data = 0;
NonVolatile g_nonVolatile;

bool g_isInGameMap = false;
StartMenuState g_startMenuState = StartMenuState::Booting;
int g_rerollCount = 0;  // Number of rerolls in char attributes creation
bool g_hasBeenIdleOnce = false;
bool g_isInGameTransition = false;  // Before being in game map
bool g_isInBattle = false;
bool g_isDead = false;
bool g_wantsToSave = true;  // DISABLED. It can corrupt saved games
int g_debugLogInstructions = 0;    // Tapping "End" key logs the next 100,000 instructions

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

    // Offscreen rendering
	const auto format = m_deviceResources->GetBackBufferFormat();
	m_offscreenTexture1 = std::make_unique<DX::RenderTexture>(format);
    m_offscreenTexture1->SetClearColor(Colors::Black);
	m_offscreenTexture2 = std::make_unique<DX::RenderTexture>(format);
	m_offscreenTexture2->SetClearColor(Colors::Black);
	m_offscreenTexture3 = std::make_unique<DX::RenderTexture>(format);
	m_offscreenTexture3->SetClearColor(Colors::Black);

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

    shouldRender = true;

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

	SimpleMath::Rectangle winrct = GetDrawRectangle();
	m_deviceResources->SetWindow(window, winrct.width, winrct.height);
    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_animTextManager = AnimTextManager::GetInstance(m_deviceResources.get(), m_resourceDescriptors.get());
	m_animSpriteManager = AnimSpriteManager::GetInstance(m_deviceResources.get(), m_resourceDescriptors.get());
	m_trigger = MemoryTriggers::GetInstance(&m_timer);
	m_trigger->PollMapSetCurrentValues();
}

UINT64 Game::GetTotalTicks()
{
    return m_timer.GetTotalTicks();
}

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

	EmulatorMessageLoopProcessing();

	// Modern keyboard handling
    // Keystrokes are passed to AppleWin in Main.cpp before getting here
    // So the only thing one should do here is handle special keys that
    // we know don't do anything under the emulated Deathlord
    // Unless SetSendKeystrokesToAppleWin(false), in which case we take care
    // of the whole keyboard state here

	auto kb = m_keyboard->GetState();
	kbTracker.Update(kb);
	// Pad is unused, just kept here for reference
	auto pad = m_gamePad->GetState(0);
	if (pad.IsConnected())
	{
		if (pad.IsViewPressed())
		{
			// Do something when gamepad keys are pressed
		}
	}

	UINT8* memPtr = MemGetMainPtr(0);
	g_isInGameMap = (memPtr[MAP_IS_IN_GAME_MAP] == 0xE5);	// when not in game map, that area is all zeros

    if (g_isInGameTransition)
    {
		// If in transition from pre-game to game, only show the transition screen
		if (g_isInGameMap && kbTracker.pressed.Space)
		{
			m_gameLoadingOverlay->HideOverlay();
			g_isInGameTransition = false;
			g_nAppMode = AppMode_e::MODE_RUNNING;
		}
		else
		{
			m_gameLoadingOverlay->ShowOverlay();
			m_gameLoadingOverlay->Update();
			return;
		}
    }
	else
	{
		SetSendKeystrokesToAppleWin(true);
	}

	if (g_isInGameMap)
	{

#ifdef _DEBUG
		if (kbTracker.pressed.F8)  // TODO: REMOVE
			m_gameOverOverlay->ToggleOverlay();
#endif
		if (g_isDead)
			m_gameOverOverlay->ShowOverlay();
		
        if (m_gameOverOverlay->ShouldRenderOverlay())
        {
            m_gameOverOverlay->Update();
            return;
        }

        if (g_isInBattle && (!m_battleOverlay->IsOverlayDisplayed()))
            m_battleOverlay->ShowOverlay();
        if ((!g_isInBattle) && m_battleOverlay->IsOverlayDisplayed())
			m_battleOverlay->HideOverlay();

		if (kbTracker.pressed.Insert)
			m_invOverlay->ToggleOverlay();

        if (kbTracker.pressed.F11)
            m_a2Video->ToggleApple2Video();
	}
#ifdef _DEBUGXXX
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
    if (m_invOverlay->IsOverlayDisplayed())
    {
		if (kbTracker.pressed.Escape)       // Escape used to close the overlay
			m_invOverlay->HideOverlay();
        m_invOverlay->Update();
        m_invOverlay->MousePosInPixels(mo.x, mo.y);
		if (moTracker.leftButton == ButtonState::PRESSED)
		{
            m_invOverlay->LeftMouseButtonClicked(moTracker.GetLastState().x, moTracker.GetLastState().y);
		}
    }

    // Let the battle window update itself
    if (m_battleOverlay->IsOverlayDisplayed())
    {
        m_battleOverlay->Update();
    }

    auto autoMap = AutoMap::GetInstance();
    auto memTriggers = MemoryTriggers::GetInstance();
    if (g_isInGameMap && (autoMap != NULL))
    {
		memTriggers->PollKeyMemoryLocations();  // But not the avatar XY
        autoMap->CalcTileVisibility();          // Won't trigger unless the game cpu65C02.h has requested it
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

        // Prepare the command list to render a new frame.
        m_deviceResources->Prepare();
        Clear();

        auto commandList = m_deviceResources->GetCommandList();
        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

		// Now set the command list data
		ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
		commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

        // Always draw to offscreen texture
        m_offscreenTexture1->BeginScene(commandList);
		auto rtvDescriptor = m_renderDescriptors->GetCpuHandle((size_t)RTDescriptors::Offscreen1);
		commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &m_deviceResources->GetDepthStencilView());

		// Get the window size
        // We'll need it for the effects projection matrix
		RECT clientRect;
		GetClientRect(m_window, &clientRect);
		auto _scRect = SimpleMath::Rectangle(clientRect);

        // Now render
        if (g_isInGameTransition)
        {
            // In between pre-game and game
			m_gameLoadingOverlay->Render(_scRect);
        }
	    else if (!g_isInGameMap)
		{
            // Start Menu Loading and utilities, character creation
            m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
			m_spriteBatch->Begin(commandList, m_states->LinearClamp(), SpriteSortMode_Deferred);
			m_a2Video->Render(_scRect, m_uploadBatch.get());
            // Show extra info for the modern gamer who doesn't expect to tap a key unless explicitly told
            std::wstring _sMenu;
            switch (g_startMenuState)
            {
            case StartMenuState::Booting:
                break;
            case StartMenuState::Title:
				_sMenu = L"Press any key";
                break;
            case StartMenuState::Menu:
                _sMenu = L"Choose 'U', 'C', or 'P' to continue";
                break;
            case StartMenuState::AttributesRerollPropose:
                _sMenu = L"Press 'A' to autoroll until STR/CON/INT/DEX are all at least 1 below the maximum for the chosen race";
                break;
            case StartMenuState::AttributesRerolling:
                _sMenu = L"Rolling attributes for STR/CON/INT/DEX >= (max-1), press almost any key to cancel";
                break;
			case StartMenuState::AttributesRerollCancelled:
				_sMenu = L"";
				break;
			case StartMenuState::AttributesRerollDone:
				_sMenu = L"Saved you rolling " + std::to_wstring(g_rerollCount) + L" times to get to this set of attributes";
				break;
            case StartMenuState::PromptScenarios:
                break;
            case StartMenuState::LoadingGame:
                break;
            case StartMenuState::Other:
                break;
            default:
                break;
            }
            auto _sSize = m_spriteFonts.at(FontDescriptors::FontDLRegular)->MeasureString(_sMenu.c_str(), false);
            auto _a2VideoSize = m_a2Video->GetSize();
            float _sX = _scRect.Center().x - (XMVectorGetX(_sSize) / 2.f);
            float _sY = _scRect.Center().y + (_a2VideoSize.y / 2) + 50;
			m_spriteFonts.at(FontDescriptors::FontDLRegular)->DrawString(m_spriteBatch.get(),
                _sMenu.c_str(), { _sX, _sY }, Colors::AntiqueWhite, 0.f, Vector2(), 1.f);
			// Display loading/writing status
			if (DiskActivity())
			{
				m_spriteFonts.at(FontDescriptors::FontDLRegular)->DrawString(m_spriteBatch.get(),
					s_hourglass.c_str(), { _scRect.Center().x + (_a2VideoSize.x / 2) + 50, _sY }, Colors::AntiqueWhite, ((tickOfLastRender / 100000) % 32) / 10.f,
					Vector2(7, 8), 1.f);
			}
			m_spriteBatch->End();
        }
        else
        {
			// Only redraw the map if not in battle and not paused. Since the map is the only thing drawn in m_offscreenTexture2,
			// we can just copy over the existing texture inside PostProcessMap and it'll be the last drawn map we had.
			if ((!m_battleOverlay->IsOverlayDisplayed()) && (g_nAppMode != AppMode_e::MODE_PAUSED))
			{
				// Draw autoMap using the second offscreen render target
				m_offscreenTexture2->BeginScene(commandList);
				auto rtvDescriptor = m_renderDescriptors->GetCpuHandle((size_t)RTDescriptors::Offscreen2);
				commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &m_deviceResources->GetDepthStencilView());
				commandList->ClearRenderTargetView(rtvDescriptor, Colors::Black, 0, nullptr);

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

			// and postprocess the map, sending it into m_offscreenTexture1
			// At this point we're working on m_offscreenTexture1
			PostProcessMap(commandList);

			// Now draw everything else that's in the main viewport
			// We use the same vp, primitivebatch and spriteBatch for all.
			// Only the main automap area was different due to its special handling of vp and scissors

			m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
			m_dxtEffectTriangles->SetProjection(XMMatrixOrthographicOffCenterRH(clientRect.left, clientRect.right, clientRect.bottom, clientRect.top, 0, 1));
			m_dxtEffectTriangles->Apply(commandList);
			m_primitiveBatchTriangles->Begin(commandList);
			m_spriteBatch->Begin(commandList, SpriteSortMode_Deferred);

			// Draw the game background
			auto mmBGTexSize = GetTextureSize(m_gameTextureBG.Get());
			m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::MainBackground), mmBGTexSize,
				r, nullptr, Colors::White, 0.f, XMFLOAT2());
			// End drawing the game background

#if 1
			char pcbuf[4000];
			snprintf(pcbuf, sizeof(pcbuf), "PRERELEASE: %.2d FPS , %6.0f usec/frame - Time: %6.2f\n",
				m_timer.GetFramesPerSecond(), 1000000.f / m_timer.GetFramesPerSecond(), m_timer.GetTotalSeconds());
			m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), pcbuf,
				{ 11.f, 11.f }, Colors::Black, 0.f, m_vector2Zero, 1.f);
			m_spriteFonts.at(FontDescriptors::FontA2Regular)->DrawString(m_spriteBatch.get(), pcbuf,
				{ 10.f, 10.f }, Colors::OrangeRed, 0.f, m_vector2Zero, 1.f);
#endif // _DEBUG

			m_partyLayout->Render(r, m_spriteBatch.get());
            m_minimap->Render(r, m_spriteBatch.get());
            m_daytime->Render(r, m_spriteBatch.get());
			m_textOutput->Render(r, m_spriteBatch.get());
			m_primitiveBatchTriangles->End();
			m_spriteBatch->End();
			// End drawing everything else that's in the main viewport

            // Draw now the overlays, each is independent
			// The battle overlay
			m_battleOverlay->Render(SimpleMath::Rectangle(clientRect));
			// The inventory overlay
			m_invOverlay->Render(SimpleMath::Rectangle(clientRect));

            // and the animations
            // TODO: Really have to figure out how to reduce the sprite batch begin/end calls
			m_spriteBatch->Begin(commandList, SpriteSortMode_Deferred);
            m_animSpriteManager->RenderAnimations(m_spriteBatch.get());
			m_animTextManager->RenderAnimations(m_spriteBatch.get());
			m_spriteBatch->End();

            // The apple2 video is unique and independent
            // It should be displayed at the top if requested
			if (m_a2Video->IsApple2VideoDisplayed())
				m_a2Video->Render(SimpleMath::Rectangle(clientRect), m_uploadBatch.get());

			// And the game over overlay is even more unique
			// We'll make it render with a special spritebatch
			// and animate its appearance via shaders
			m_gameOverOverlay->Render(SimpleMath::Rectangle(clientRect));

        }   // end if !g_isInGameMap

        // Now check if the game is paused and display an overlay
		// Unless we're in the game loading screen
        if (g_nAppMode == AppMode_e::MODE_PAUSED && (!g_isInGameTransition))
        {
            std::wstring pauseWStr = L"GAME PAUSED";
            float pauseScale = 5.f;
            auto pauseOrigin = r.Center(); // start with dead center
			auto _sSizeX = XMVectorGetX(m_spriteFonts.at(FontDescriptors::FontDLRegular)->MeasureString(pauseWStr.c_str(), false)) * pauseScale;
            pauseOrigin.x -= _sSizeX /2;
            pauseOrigin.y -= pauseScale * 14 / 2;
			m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
			m_spriteBatch->Begin(commandList, m_states->LinearClamp(), SpriteSortMode_Deferred);
			m_spriteFonts.at(FontDescriptors::FontDLRegular)->DrawString(m_spriteBatch.get(), pauseWStr.c_str(),
                pauseOrigin - Vector2(pauseScale/2, pauseScale/2), Colors::Black, 0.f, m_vector2Zero, pauseScale);
			m_spriteFonts.at(FontDescriptors::FontDLRegular)->DrawString(m_spriteBatch.get(), pauseWStr.c_str(),
                pauseOrigin, Colors::OrangeRed, 0.f, m_vector2Zero, pauseScale);
            m_spriteBatch->End();
        }

		PIXEndEvent(commandList);

        // Offscreen postprocessing
		if (m_offscreenTexture1->GetCurrentState() == D3D12_RESOURCE_STATE_RENDER_TARGET)
			m_offscreenTexture1->EndScene(commandList);

        PostProcess(commandList);

		// Show the new frame.
		PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
		m_deviceResources->Present();
		m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
		PIXEndEvent();
    }
}

void Game::PostProcessMap(ID3D12GraphicsCommandList* commandList)
{
	if (!g_isInGameMap)
		return;

	auto vp = m_deviceResources->GetScreenViewport();
	{
		// apply postprocessing on the map
		// OffscreenTexture2 -> OffscreenTexture1
		auto rtvDescriptor = m_renderDescriptors->GetCpuHandle((size_t)RTDescriptors::Offscreen1);
		commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &m_deviceResources->GetDepthStencilView());
		commandList->RSSetViewports(1, &vp);
		// Determine if the leader character has a certain status

		UINT8 _mCurrentChar = MemGetMainPtr(PARTY_CURRENT_CHAR_POS)[0];
		// If _mCurrentChar == 0xFF it could be after casting a spell in battle
		if (_mCurrentChar >= DEATHLORD_PARTY_SIZE)
			goto NOPOSTPROCESSING;

		UINT8 _mStatus = MemGetMainPtr(PARTY_STATUS_START)[_mCurrentChar];
		if (_mStatus & (UINT8)DeathlordCharStatus::ILL)
		{	// blur if ILL
			m_postProcessBlur->SetSourceTexture(
				m_resourceDescriptors->GetGpuHandle((size_t)TextureDescriptors::OffscreenTexture2),
				m_offscreenTexture2->GetResource());
			m_postProcessBlur->SetBloomBlurParameters(true, abs(sin((m_timer.GetTotalTicks() << 10) >> 8)) + 3.f, 1.f);
			m_postProcessBlur->Process(commandList);
		}
		else
		{	// do nothing
NOPOSTPROCESSING:
			m_postProcessCopy->SetSourceTexture(
				m_resourceDescriptors->GetGpuHandle((size_t)TextureDescriptors::OffscreenTexture2),
				m_offscreenTexture2->GetResource());
			m_postProcessCopy->Process(commandList);
			// another way to do the above (no speed difference):
//			auto offscreenTarget = m_offscreenTexture2->GetResource();
//			auto renderTarget = m_offscreenTexture1->GetResource();
//			m_offscreenTexture2->TransitionTo(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
//			m_offscreenTexture1->TransitionTo(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
//			commandList->CopyResource(renderTarget, offscreenTarget);
//			m_offscreenTexture2->TransitionTo(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
//			m_offscreenTexture1->TransitionTo(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		if (m_offscreenTexture2->GetCurrentState() == D3D12_RESOURCE_STATE_RENDER_TARGET)
			m_offscreenTexture2->EndScene(commandList);
	}
}

void Game::PostProcess(ID3D12GraphicsCommandList* commandList)
{
	auto vp = m_deviceResources->GetScreenViewport();
    if (g_isInGameMap)
	{
		// OffscreenTexture1 -> framebuffer
		PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"In-Game PostProcess");
		auto renderTarget = m_deviceResources->GetRenderTarget();
		auto offscreenTarget = m_offscreenTexture1->GetResource();
		auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
		commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);
		commandList->RSSetViewports(1, &vp);
		{
			ScopedBarrier barriers(commandList,
				{
					CD3DX12_RESOURCE_BARRIER::Transition(renderTarget,
						D3D12_RESOURCE_STATE_RENDER_TARGET,
						D3D12_RESOURCE_STATE_COPY_DEST, 0),
					CD3DX12_RESOURCE_BARRIER::Transition(offscreenTarget,
						D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
						D3D12_RESOURCE_STATE_COPY_SOURCE, 0)
				});
			commandList->CopyResource(renderTarget, offscreenTarget);
		}
		PIXEndEvent(commandList);
	}
    else
    {   // Not in game map
        // AppleWin already draws to the framebuffer
        // OffscreenTexture1 -> framebuffer

        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"PreGame PostProcess");
		auto renderTarget = m_deviceResources->GetRenderTarget();
		auto offscreenTarget = m_offscreenTexture1->GetResource();
		auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
		commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);
		commandList->RSSetViewports(1, &vp);
        {
			ScopedBarrier barriers(commandList,
				{
					CD3DX12_RESOURCE_BARRIER::Transition(renderTarget,
						D3D12_RESOURCE_STATE_RENDER_TARGET,
						D3D12_RESOURCE_STATE_COPY_DEST, 0),
					CD3DX12_RESOURCE_BARRIER::Transition(offscreenTarget,
						D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
						D3D12_RESOURCE_STATE_COPY_SOURCE, 0)
				});
			commandList->CopyResource(renderTarget, offscreenTarget);
        }
		PIXEndEvent(commandList);
    }
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
	// we redraw every frame completely every time
	// The offscreen tex 2 is cleared right before drawing the map
	// The offscreen tex 3 is never cleared because
	//	1. it's not used right now
	//  2. it'll be used as a render target that will be filled
	auto dsvDescriptor = m_deviceResources->GetDepthStencilView();
    //auto rtvDescriptor = m_deviceResources->GetRenderTargetView();	// using offscreen textures instead
	auto rtvDescriptor = m_renderDescriptors->GetCpuHandle((size_t)RTDescriptors::Offscreen1);

	m_offscreenTexture1->TransitionTo(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_offscreenTexture2->TransitionTo(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_offscreenTexture3->TransitionTo(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);

	m_offscreenTexture1->TransitionTo(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
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

    // Do the sprite batches.
	auto sampler = m_states->LinearWrap();
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
    // TODO: Right now spritesheets are NonPremultiplied
    // Optimize to AlphaBlend?
	SpriteBatchPipelineStateDescription spd(rtState, &CommonStates::NonPremultiplied, nullptr, nullptr, &sampler);
	m_spriteBatch = std::make_unique<SpriteBatch>(device, *m_uploadBatch.get(), spd);

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

    // offscreen rendering and postprocessing
	m_renderDescriptors = std::make_unique<DescriptorHeap>(device,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		(size_t)RTDescriptors::Count);

	m_offscreenTexture1->SetDevice(device,
		m_resourceDescriptors->GetCpuHandle((size_t)TextureDescriptors::OffscreenTexture1),
		m_renderDescriptors->GetCpuHandle((size_t)RTDescriptors::Offscreen1));
	m_offscreenTexture2->SetDevice(device,
		m_resourceDescriptors->GetCpuHandle((size_t)TextureDescriptors::OffscreenTexture2),
		m_renderDescriptors->GetCpuHandle((size_t)RTDescriptors::Offscreen2));
	m_offscreenTexture3->SetDevice(device,
		m_resourceDescriptors->GetCpuHandle((size_t)TextureDescriptors::OffscreenTexture3),
		m_renderDescriptors->GetCpuHandle((size_t)RTDescriptors::Offscreen3));

	m_postProcessBlur = std::make_unique<BasicPostProcess>(device, rtState, BasicPostProcess::BloomBlur);
	m_postProcessCopy = std::make_unique<BasicPostProcess>(device, rtState, BasicPostProcess::Copy);
	m_postProcessMerge = std::make_unique<DualPostProcess>(device, rtState, DualPostProcess::Merge);

	m_autoMap = AutoMap::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_autoMap->CreateDeviceDependentResources(m_uploadBatch.get());
	m_textOutput = TextOutput::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_invOverlay = InvOverlay::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_invOverlay->CreateDeviceDependentResources(m_uploadBatch.get(), m_states.get());
	m_battleOverlay = BattleOverlay::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_battleOverlay->CreateDeviceDependentResources(m_uploadBatch.get(), m_states.get());
	m_gameOverOverlay = GameOverOverlay::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_gameOverOverlay->CreateDeviceDependentResources(m_uploadBatch.get(), m_states.get());
	m_gameLoadingOverlay = GameLoadingOverlay::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_gameLoadingOverlay->CreateDeviceDependentResources(m_uploadBatch.get(), m_states.get(), L"InterferencePS.cso");
	m_a2Video = AppleWinDXVideo::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_a2Video->CreateDeviceDependentResources(m_uploadBatch.get(), m_states.get());
	m_minimap = MiniMap::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_minimap->CreateDeviceDependentResources(m_uploadBatch.get());
	m_daytime = Daytime::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_daytime->CreateDeviceDependentResources(m_uploadBatch.get());
	m_partyLayout = PartyLayout::GetInstance(m_deviceResources, m_resourceDescriptors);
	m_partyLayout->CreateDeviceDependentResources(m_uploadBatch.get());

    auto uploadResourcesFinished = m_uploadBatch->End(command_queue);
    uploadResourcesFinished.wait();

    // Wait until assets have been uploaded to the GPU.
    m_deviceResources->WaitForGpu();

}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    m_spriteBatch->SetViewport(GetCurrentViewport());
	auto size = m_deviceResources->GetOutputSize();
	// offscreen rendering and postprocessing
	m_offscreenTexture1->SetWindow(size);
	m_offscreenTexture2->SetWindow(size);
	m_offscreenTexture3->SetWindow(size);
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
	m_battleOverlay->OnDeviceLost();
	m_gameOverOverlay->OnDeviceLost();
	m_gameLoadingOverlay->OnDeviceLost();
	m_a2Video->OnDeviceLost();
	m_minimap->OnDeviceLost();
	m_daytime->OnDeviceLost();
	m_partyLayout->OnDeviceLost();

	m_gameTextureBG.Reset();
    m_states.reset();
    m_spriteBatch.reset();
	m_primitiveBatchLines.reset();
	m_primitiveBatchTriangles.reset();
	m_dxtEffectLines.reset();
	m_dxtEffectTriangles.reset();
	m_uploadBatch.reset();

	// offscreen rendering and postprocessing
	m_offscreenTexture1->ReleaseDevice();
	m_offscreenTexture2->ReleaseDevice();
	m_offscreenTexture3->ReleaseDevice();
	m_renderDescriptors.reset();
    m_postProcessBlur.reset();
	m_postProcessCopy.reset();
	m_postProcessMerge.reset();

	m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion


