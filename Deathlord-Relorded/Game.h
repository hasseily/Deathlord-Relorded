//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "HAUtils.h"
#include "NonVolatile.h"
#include <Keyboard.h>
#include <map>
#include "Descriptors.h"

constexpr int MAX_RENDERED_FRAMES_PER_SECOND = 30;  // Only render so many frames. Give the emulator all the rest of the time

using namespace DirectX;
using namespace DirectX::SimpleMath;
constexpr Color COLOR_APPLE2_BLUE   ( (0x07/255.f), (0xA8/255.f), (0xE0/255.f) );
constexpr Color COLOR_APPLE2_ORANGE ( (0xF9/255.f), (0x56/255.f), (0x1D/255.f) );
constexpr Color COLOR_APPLE2_GREEN  ( (0x43/255.f), (0xC8/255.f), (0x00/255.f) );
constexpr Color COLOR_APPLE2_VIOLET ( (0xBB/255.f), (0x36/255.f), (0xFF/255.f) );

constexpr UINT32 MAP_WIDTH_IN_VIEWPORT = 14 * 64;   // Fixed map width in the viewport

enum class EmulatorLayout
{
	NORMAL = 0,
	FLIPPED_X = 1,
	FLIPPED_Y = 2,
	FLIPPED_XY = 3,
	NONE = UINT8_MAX
};

extern bool g_isInGameMap;          // is the player in-game?
extern bool g_hasBeenIdleOnce;      // This becomes true once the game has once hit the idle loop
extern bool g_isInBattle;           // is the player in the battle module?
extern bool g_wantsToSave;          // only TRUE when the player is asking to save
extern int g_debugLogInstructions;  // Tapping "End" key logs the next g_debugLogInstructions instructions
extern NonVolatile g_nonVolatile;

// A game implementation that creates a D3D12 device and
// provides a game loop.
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false);
    ~Game();

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
    void Initialize(HWND window);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    void ActivateLastUsedProfile();
    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
	void OnWindowSizeChanged(LONG width, LONG height);  // pass in new width and height of client rect

    // Menu commands
    void MenuActivateProfile();
    void MenuDeactivateProfile();
    void MenuShowLogWindow();
    void MenuShowSpellWindow();
	void MenuToggleLogWindow();
	void MenuToggleSpellWindow();
	void MenuToggleHacksWindow();

    // Other methods
    void SetWindowSizeOnChangedProfile();

    // Accessors
    SimpleMath::Rectangle GetDrawRectangle();
    float GetFrameScale() { return m_clientFrameScale; };
    SpriteFont* GetSpriteFontAtIndex(FontDescriptors fontIndex);
    // TODO: Either don't allow these accessors, or change them to return the underlying pointer
    std::shared_ptr<DirectX::SpriteBatch> GetSpriteBatch() { return m_spriteBatch; };

    // Properties
	bool shouldRender;
	// Rendering loop timer.
	DX::StepTimer m_timer;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    D3D12_VIEWPORT GetCurrentViewport()
    {
        auto vp = m_deviceResources->GetScreenViewport();
        if (!g_isInGameMap)
        {
            vp = m_deviceResources->GetGamelinkViewport();  // draw the original game until we are in the game map
        }
        return vp;
    }

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

	static float m_clientFrameScale;    // TODO: unused

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // video texture layout may change
    // TODO: Unused, get rid of it
    EmulatorLayout m_currentLayout;
    EmulatorLayout m_previousLayout;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;
	std::unique_ptr<DirectX::Mouse>         m_mouse;

	std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

    std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory;

	// fonts and primitives from dxtoolkit12 to draw everything
	std::map<FontDescriptors, std::unique_ptr<SpriteFont>> m_spriteFonts;
	std::shared_ptr<PrimitiveBatch<VertexPositionColor>> m_primitiveBatchLines;
	std::shared_ptr<PrimitiveBatch<VertexPositionColor>> m_primitiveBatchTriangles;
	std::unique_ptr<BasicEffect> m_dxtEffectLines;
	std::unique_ptr<BasicEffect> m_dxtEffectTriangles;
	std::shared_ptr<DirectX::SpriteBatch> m_spriteBatch;
	std::unique_ptr<DirectX::CommonStates> m_states;
    std::shared_ptr<DirectX::ResourceUploadBatch> m_uploadBatch;
    DirectX::SimpleMath::Vector2 m_fontPos;

    // The main game texture background
	Microsoft::WRL::ComPtr<ID3D12Resource> m_gameTextureBG;
};