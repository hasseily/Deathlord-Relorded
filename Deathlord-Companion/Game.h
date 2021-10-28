//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "LogWindow.h"
#include "Sidebar.h"
#include "DeathlordHacks.h"
#include "TilesetCreator.h"
#include "AutoMap.h"
#include "HAUtils.h"
#include "NonVolatile.h"
#include <Keyboard.h>
#include <map>
#include "Descriptors.h"

#ifdef _DEBUG
constexpr int MAX_RENDERED_FRAMES_PER_SECOND = 10;  // In debug, the emulator is slow needs a lot more time than the render
#else
constexpr int MAX_RENDERED_FRAMES_PER_SECOND = 30;  // Only render so many frames. Give the emulator all the rest of the time
#endif

using namespace DirectX::SimpleMath;
constexpr Color COLOR_APPLE2_BLUE   ( (0x07/255.f), (0xA8/255.f), (0xE0/255.f) );
constexpr Color COLOR_APPLE2_ORANGE ( (0xF9/255.f), (0x56/255.f), (0x1D/255.f) );
constexpr Color COLOR_APPLE2_GREEN  ( (0x43/255.f), (0xC8/255.f), (0x00/255.f) );
constexpr Color COLOR_APPLE2_VIOLET ( (0xBB/255.f), (0x36/255.f), (0xFF/255.f) );

constexpr UINT32 MAP_WIDTH_IN_VIEWPORT = 800;   // Fixed map width in the viewport


enum class EmulatorLayout
{
	NORMAL = 0,
	FLIPPED_X = 1,
	FLIPPED_Y = 2,
	FLIPPED_XY = 3,
	NONE = UINT8_MAX
};

extern bool g_isInGameMap;          // is the player in-game or on the loading/utilities screens?
extern bool g_wantsToSave;          // only TRUE when the player is asking to save
extern NonVolatile g_nonVolatile;
static std::shared_ptr<LogWindow>m_logWindow;
static std::shared_ptr<DeathlordHacks>m_dlHacks;

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
    void Initialize(HWND window, int width, int height);

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
	void MenuToggleLogWindow();
	void MenuToggleHacksWindow();

    // Other methods
    D3D12_RESOURCE_DESC ChooseTexture();
    void SetVideoLayout(EmulatorLayout layout);
    void SetWindowSizeOnChangedProfile();

    // Accessors
    void GetBaseSize(__out int& width, __out int& height) noexcept;
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

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void SetVertexData(HA::Vertex* v, float wRatio, float hRatio, EmulatorLayout layout);

    // This method must be called when sidebars are added or deleted
    // so the relative vertex boundaries can be updated to stay within
    // the aspect ratio of the gamelink texture. Pass in width and height ratios
    // (i.e. how much of the total width|height the vertex should fill, 1.f being full width|height)
    void UpdateGamelinkVertexData(int width, int height, float wRatio, float hRatio);

	static float m_clientFrameScale;    // TODO: unused
    AutoMap* m_automap;

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Background image when in LOGO mode
    std::vector<uint8_t> m_bgImage;
    uint32_t m_bgImageWidth;
    uint32_t m_bgImageHeight;

    // video texture layout may change
    // TODO: Unused, get rid of it
    EmulatorLayout m_currentLayout;
    EmulatorLayout m_previousLayout;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;


	std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

    std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory;

	// fonts and primitives from dxtoolkit12 to draw lines
	std::map<FontDescriptors, std::unique_ptr<SpriteFont>> m_spriteFonts;
	std::unique_ptr<PrimitiveBatch<VertexPositionColor>> m_primitiveBatchLines;
	std::unique_ptr<BasicEffect> m_lineEffectLines;
	std::shared_ptr<DirectX::SpriteBatch> m_spriteBatch;
    DirectX::SimpleMath::Vector2 m_fontPos;

    // Direct3D 12 objects for AppleWin video texture
    Microsoft::WRL::ComPtr<ID3D12RootSignature>     m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>     m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_texture;
    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW                         m_indexBufferView;
};