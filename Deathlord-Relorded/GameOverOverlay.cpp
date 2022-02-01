#include "pch.h"
#include "GameOverOverlay.h"
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
GameOverOverlay* GameOverOverlay::s_instance;

const std::map<UINT8, std::wstring> m_adjectives =
{
	{1, L"pathetic"},
	{2, L"worthless"},
	{3, L"contemptible"},
	{4, L"puny"},
	{5, L"wretched"},
	{6, L"miserable"},
	{7, L"abject"},
	{8, L"average"},
	{9, L"deplorable"},
	{10, L"semi-pro"},
	{11, L"strong"},
	{12, L"powerful"},
	{13, L"dangerous"},
	{14, L"implacable"},
	{15, L"formidable"},
	{16, L"mighty"},
	{20, L"terrible"},
	{24, L"terrifying"},
	{26, L"deadly"},
	{28, L"uncanny"},
	{29, L"unearthly"},
	{30, L"fabled"},
	{31, L"legendary"}
};

const std::array<std::wstring, 10> m_verbs =
{
	L"killed",
	L"destroyed",
	L"terminated",
	L"obliterated",
	L"finished",
	L"wiped out",
	L"buried",
	L"slaughtered",
	L"massacred",
	L"ground to atomic particles"
};

// In main
extern std::unique_ptr<Game>* GetGamePtr();

#pragma region main
void GameOverOverlay::Initialize()
{
	bIsDisplayed = false;
	bShouldDisplay = false;
	bShouldBlockKeystrokes = false;
	m_currentRect = { 0,0,0,0 };
	m_width = 1000;
	m_height = 800;
	m_spritesheetDescriptor = TextureDescriptors::DLRLGameOver;
	m_spritesheetPath = L"Assets/GameOver.png";
	m_type = OverlayType::Bare;
	m_curtainColor = ColorCurtain;

	m_monstersKilled = 0;
	m_scale = 1.f;
}

#pragma endregion

#pragma region actions
// Update the state based on the game's data
void GameOverOverlay::Update()
{

}

#pragma endregion

#pragma region Drawing

void GameOverOverlay::Render(SimpleMath::Rectangle r)
{
	if (!bShouldDisplay)
	{
		if (bIsDisplayed)
		{
			// just kill the overlay, it shouldn't be here.
			// Don't bother animating it
			bIsDisplayed = false;
		}
		return;
	}

	// Now check if we should animate the display as it appears
	if (!bIsDisplayed)
	{
		wchar_t _buf[500];

		if (MemGetMainPtr(MEM_BATTLE_MONSTER_INDEX)[0] != 0xFF)		// killed by a monster
		{
			auto _enemyName = StringFromMemory(MONSTER_CURR_NAME, 20);
			if (_enemyName.size() > 2)
			{
				auto _enemyThac0 = MemGetMainPtr(MONSTER_CURR_THAC0)[0];
				auto _verb = m_verbs.at(rand() % m_verbs.size());
				auto _f = m_adjectives.find(_enemyThac0);
				while (_f == m_adjectives.end())
				{
					--_enemyThac0;
					_f = m_adjectives.find(_enemyThac0);
				}
				swprintf_s(_buf, L"The %s %s %s your party.", m_adjectives.find(_enemyThac0)->second.c_str(), _enemyName.c_str(), _verb.c_str());
			}
			else
			{
				swprintf_s(_buf, L"Your party is six feet under. Permanently.");
			}
		}
		else
		{
			swprintf_s(_buf, L"Deathlord hates you.");
		}
		m_line1 = std::wstring(_buf);
		swprintf_s(_buf, L"You have defeated %d enemies in this playthrough.", m_monstersKilled);
		m_line2 = std::wstring(_buf);
		m_line3 = std::wstring(L"Press Alt-R to Reboot.");
	}

	Overlay::PreRender(r);


	// Draw the main sprite
	auto _spriteSheetSize = GetTextureSize(m_overlaySpriteSheet.Get());
	XMFLOAT2 _texCenter = { m_scale * (float)_spriteSheetSize.x / 2.f, m_scale * (float)_spriteSheetSize.y / 2.f };
	XMFLOAT2 _winCenter = r.Center();
	RECT texRect = {
		_winCenter.x - _texCenter.x,			// left
		_winCenter.y - _texCenter.y - 100,		// top
		_winCenter.x + _texCenter.x,			// right
		_winCenter.y + _texCenter.y - 100		// bottom
	};
	m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)m_spritesheetDescriptor), _spriteSheetSize, texRect);

	/////////// Display the text below ///////////
	auto gamePtr = GetGamePtr();
	auto font = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular);
	XMVECTOR _sSize;
	float _sX = _winCenter.x;
	float _sY = texRect.bottom + 50;

	_sSize = font->MeasureString(m_line1.c_str(), false);
	font->DrawString(m_spriteBatch.get(), m_line1.c_str(), { _sX, _sY }, Colors::AntiqueWhite, 0.f, Vector2(XMVectorGetX(_sSize) / 2.f, 0), 1.f);
	_sSize = font->MeasureString(m_line2.c_str(), false);
	font->DrawString(m_spriteBatch.get(), m_line2.c_str(), { _sX, _sY + 20 }, Colors::AntiqueWhite, 0.f, Vector2(XMVectorGetX(_sSize) / 2.f, 0), 1.f);
	_sSize = font->MeasureString(m_line3.c_str(), false);
	font->DrawString(m_spriteBatch.get(), m_line3.c_str(), { _sX, _sY + 80 }, Colors::AntiqueWhite, 0.f, Vector2(XMVectorGetX(_sSize) / 2.f, 0), 1.f);

	/////////// End display text below ///////////


	Overlay::PostRender(r);

}

#pragma endregion