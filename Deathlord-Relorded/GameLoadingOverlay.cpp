#include "pch.h"
#include "GameLoadingOverlay.h"
#include "Game.h"
#include "DeathlordHacks.h"
#include "Emulator/CPU.h"
#include "AutoMap.h"
#include "Descriptors.h"
#include <SimpleMath.h>


using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;

// below because "The declaration of a static data member in its class definition is not a definition"
GameLoadingOverlay* GameLoadingOverlay::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();

static float startTime = 0.f;
static std::wstring s_pressSpace = std::wstring(L"PRESS SPACE");
static FontDescriptors _fd = FontDescriptors::FontDLRegular;

#pragma region main
void GameLoadingOverlay::Initialize()
{
	Overlay::Initialize();
	bShouldBlockKeystrokes = true;
	m_width = 1900;
	m_height = 1000;
	m_spritesheetDescriptor = TextureDescriptors::DLRLLoadingScreen;
	m_spritesheetPath = L"Assets/DLRL_Loading_Screen.png";
	m_type = OverlayType::Bare;
	m_curtainColor = ColorCurtain;

	m_shaderParameters.barThickness = 0.01f;
	m_shaderParameters.maxInterference = 2.5f;
	m_transitionTime = 1.0f;

	m_scale = 1.f;
}

#pragma endregion

#pragma region actions
// Update the state based on the game's data
void GameLoadingOverlay::Update()
{
	if (bShouldDisplay)
	{
		auto gamePtr = GetGamePtr();
		auto timer = (*gamePtr)->m_timer;
		auto totalTime = static_cast<float>(timer.GetTotalSeconds());

		if ((static_cast<size_t>(totalTime) % 2) == 0)
			_fd = FontDescriptors::FontDLRegular;
		else
			_fd = FontDescriptors::FontDLInverse;

		if (m_overlayState != OverlayState::Displayed)
		{
			// If animation hasn't started, update data and start animation
			if (m_overlayState != OverlayState::TransitionIn)
			{
				startTime = totalTime;
				m_overlayState = OverlayState::TransitionIn;
			}

			if (startTime > 0)
			{
				if ((totalTime - startTime) > m_transitionTime)		// Check if animation ended
				{
					m_overlayState = OverlayState::Displayed;
					m_shaderParameters.maxInterference = 0.f;
					m_shaderParameters.deltaT = 1.f;
				}
				else
				{
					m_shaderParameters.deltaT = (totalTime - startTime) / m_transitionTime;
					m_shaderParameters.maxInterference = (1 - m_shaderParameters.deltaT) * 1.f;
				}
			}
		}
	}
}

#pragma endregion

#pragma region Drawing

void GameLoadingOverlay::Render(SimpleMath::Rectangle r)
{
	if (!bShouldDisplay)
	{
		if (m_overlayState == OverlayState::Displayed)
		{
			// just kill the overlay, it shouldn't be here.
			// Don't bother animating it
			m_overlayState = OverlayState::Hidden;
		}
		return;
	}

	Overlay::PreRender(r);


	// Draw the main sprite
	auto _spriteSheetSize = GetTextureSize(m_overlaySpriteSheet.Get());
	XMFLOAT2 _texCenter = { m_scale * (float)_spriteSheetSize.x / 2.f, m_scale * (float)_spriteSheetSize.y / 2.f };
	XMFLOAT2 _winCenter = r.Center();
	RECT texRect = {
		_winCenter.x - _texCenter.x,		// left
		_winCenter.y - _texCenter.y,		// top
		_winCenter.x + _texCenter.x,		// right
		_winCenter.y + _texCenter.y 		// bottom
	};
	m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)m_spritesheetDescriptor), _spriteSheetSize, texRect);

	if (g_isInGameMap)
	{
		// Pause the game emulator
		g_nAppMode = AppMode_e::MODE_PAUSED;

		if (m_overlayState == OverlayState::Displayed)
		{
			/////////// Display the text below ///////////
			auto gamePtr = GetGamePtr();
			auto font = (*gamePtr)->GetSpriteFontAtIndex(_fd);
			auto _sSize = font->MeasureString(s_pressSpace.c_str(), false);
			float _sX = _winCenter.x;
			float _sY = _winCenter.y + 400;

			font->DrawString(m_overlaySB.get(), s_pressSpace.c_str(), { _sX, _sY },
				Colors::AntiqueWhite, 0.f, Vector2(XMVectorGetX(_sSize) / 2.f, 0), 1.f);
			/////////// End display text below ///////////
		}
	}

	Overlay::PostRender(r);

}

#pragma endregion