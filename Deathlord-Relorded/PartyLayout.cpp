#include "pch.h"
#include "PartyLayout.h"
#include "DeathlordHacks.h"
#include "Game.h"
#include <algorithm>
#include "Game.h"	// TODO: This is for the spritefonts. These need to be put somewhere else

extern std::unique_ptr<Game>* GetGamePtr();	// TODO: Same for this

// below because "The declaration of a static data member in its class definition is not a definition"
PartyLayout* PartyLayout::s_instance;

void PartyLayout::Initialize()
{
	m_partySize = 6;
	m_currentLeader = 0;
}

void PartyLayout::setPartySize(UINT8 size)
{
	if (size < 7)
		m_partySize = size;
}

bool PartyLayout::Update(UINT8 leader)
{
	if (!g_hasBeenIdleOnce)
		return false;
	if (leader == m_currentLeader)
		return false;
	m_currentLeader = leader;
	return true;
}

void PartyLayout::Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch)
{
	if (!g_hasBeenIdleOnce)
		return;
	auto plTexSize = GetTextureSize(m_partyLayoutSpriteSheet.Get());

//	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::PartyLayoutSpriteSheet), mmTexSize,
//		_overlayPinPosInMap, &_pinRect, Colors::White, 0.f, _originPin);
	for (UINT8 i = 0; i < m_partySize; i++)
	{
		RenderMember(i, spriteBatch, r.x + PARTY_LAYOUT_X[i], r.y + PARTY_LAYOUT_Y[i]);
	}
}

void PartyLayout::RenderMember(UINT8 member, DirectX::SpriteBatch* spriteBatch, UINT16 originX, UINT16 originY)
{
	// Need to render:
	// portrait (based on class+race+gender)
	// name
	// level +1/2
	// XP
	// health/max
	// power/max
	// class
	// attributes
	// 8 inventory slots
	// gold
	// torches
	// food
	// AC

	// TODO: Should not rely on the gamePtr for fonts?
	auto gamePtr = GetGamePtr();
	auto fontDL = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular);
	auto fontDLInverse = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLInverse);
	const UINT8 _bufsize = 200;
	wchar_t _buf[_bufsize];

	///// Draw portrait
	RECT _portraitRect = {	MemGetMainPtr(PARTY_RACE_START)[member] * PARTY_PORTRAIT_WIDTH,
							MemGetMainPtr(PARTY_CLASS_START)[member] * PARTY_PORTRAIT_HEIGHT,
							0, 0 };
	_portraitRect.right = _portraitRect.left + PARTY_PORTRAIT_WIDTH;
	_portraitRect.bottom = _portraitRect.top + PARTY_PORTRAIT_HEIGHT;
	Vector2 _mPortraitOrigin(originX + 2, originY + 2);
	if (MemGetMainPtr(PARTY_GENDER_START)[member] == 0)	// male
	{
		auto pTexSize = GetTextureSize(m_portraitsMale.Get());
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::PortraitsMale), pTexSize,
			_mPortraitOrigin, & _portraitRect, Colors::White, 0.f, XMFLOAT2(), 1.f);
	}
	else // Sorry, only M/F in Deathlord. Everything else has to go in Female
	{
		auto pTexSize = GetTextureSize(m_portraitsFemale.Get());
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::PortraitsFemale), pTexSize,
			_mPortraitOrigin, &_portraitRect, Colors::White, 0.f, XMFLOAT2(), 1.f);
	}

	///// Draw status with black outline
	UINT8 _mStatus = MemGetMainPtr(PARTY_STATUS_START)[member];
	Vector2 _mStatusOrigin(_mPortraitOrigin.x + PARTY_PORTRAIT_WIDTH - 4 - 14*3, 
		_mPortraitOrigin.y + PARTY_PORTRAIT_HEIGHT - 4 - 16);
	if (_mStatus & (UINT8)DeathlordCharStatus::STV)
	{
		fontDLInverse->DrawString(spriteBatch, L"STV", _mStatusOrigin, VColorText, 0.f, Vector2(), 1.0f);
		_mStatusOrigin.y -= 17;
	}
	if (_mStatus & (UINT8)DeathlordCharStatus::TOX)
	{
		fontDLInverse->DrawString(spriteBatch, L"TOX", _mStatusOrigin, VColorText, 0.f, Vector2(), 1.0f);
		_mStatusOrigin.y -= 17;
	}
	if (_mStatus & (UINT8)DeathlordCharStatus::DIS)
	{
		fontDLInverse->DrawString(spriteBatch, L"DIS", _mStatusOrigin, VColorText, 0.f, Vector2(), 1.0f);
		_mStatusOrigin.y -= 17;
	}
	if (_mStatus & (UINT8)DeathlordCharStatus::PAR)
	{
		fontDLInverse->DrawString(spriteBatch, L"PAR", _mStatusOrigin, VColorText, 0.f, Vector2(), 1.0f);
		_mStatusOrigin.y -= 17;
	}
	if (_mStatus & (UINT8)DeathlordCharStatus::STN)
	{
		fontDLInverse->DrawString(spriteBatch, L"STN", _mStatusOrigin, VColorText, 0.f, Vector2(), 1.0f);
		_mStatusOrigin.y -= 17;
	}
	if (_mStatus & (UINT8)DeathlordCharStatus::RIP)
	{
		fontDLInverse->DrawString(spriteBatch, L"RIP", _mStatusOrigin, VColorText, 0.f, Vector2(), 1.0f);
		_mStatusOrigin.y -= 17;
	}
	if (_mStatus & (UINT8)DeathlordCharStatus::STO)
	{
		fontDLInverse->DrawString(spriteBatch, L"STO", _mStatusOrigin, VColorText, 0.f, Vector2(), 1.0f);
		_mStatusOrigin.y -= 17;
	}

	///// Draw name with black outline
	std::wstring _mName = StringFromMemory(PARTY_NAME_START + (member*9), 9);
	Vector2 _mNameOrigin(_mPortraitOrigin.x + PARTY_PORTRAIT_WIDTH + 6, _mPortraitOrigin.y + 2);
	fontDL->DrawString(spriteBatch, _mName.c_str(), { _mNameOrigin.x - 1.f, _mNameOrigin.y - 1.f } , Colors::Black, 0.f, Vector2(), 1.0f);
	fontDL->DrawString(spriteBatch, _mName.c_str(), { _mNameOrigin.x - 1.f, _mNameOrigin.y + 1.f }, Colors::Black, 0.f, Vector2(), 1.0f);
	fontDL->DrawString(spriteBatch, _mName.c_str(), { _mNameOrigin.x + 1.f, _mNameOrigin.y - 1.f }, Colors::Black, 0.f, Vector2(), 1.0f);
	fontDL->DrawString(spriteBatch, _mName.c_str(), { _mNameOrigin.x + 1.f, _mNameOrigin.y + 1.f }, Colors::Black, 0.f, Vector2(), 1.0f);
	fontDL->DrawString(spriteBatch, _mName.c_str(), _mNameOrigin, VColorText, 0.f, Vector2(), 1.0f);

	///// Draw stuff to the right of the portrait
	Vector2 _mACOrigin(_mNameOrigin.x + 145, _mNameOrigin.y);
	wsprintf(_buf, L"AC%3d", 10 - MemGetMainPtr(PARTY_ARMORCLASS_START)[member]);
	fontDL->DrawString(spriteBatch, _buf, _mACOrigin, VColorText, 0.f, Vector2(), 1.f);
	//
	Vector2 _mLevelOrigin(_mNameOrigin.x, _mNameOrigin.y + 25);
	if (MemGetMainPtr(PARTY_LEVELPLUS_START)[member] > 0)
		wsprintf(_buf, L"LEVEL:%3d+%d", MemGetMainPtr(PARTY_LEVEL_START)[member],
			MemGetMainPtr(PARTY_LEVELPLUS_START)[member]);
	else
		wsprintf(_buf, L"LEVEL:%3d", MemGetMainPtr(PARTY_LEVEL_START)[member]);
	fontDL->DrawString(spriteBatch, _buf, _mLevelOrigin, VColorText, 0.f, Vector2(), 1.f);
	//
	Vector2 _mHealthOrigin(_mLevelOrigin.x, _mLevelOrigin.y + 25);
	UINT16 _mHealth = MemGetMainPtr(PARTY_HEALTH_LOBYTE_START)[member] + ((UINT16)MemGetMainPtr(PARTY_HEALTH_HIBYTE_START)[member] << 8);
	UINT16 _mHealthMax = MemGetMainPtr(PARTY_HEALTH_MAX_LOBYTE_START)[member] + ((UINT16)MemGetMainPtr(PARTY_HEALTH_MAX_HIBYTE_START)[member] << 8);
	wsprintf(_buf, L"HEALTH:%d/%d", _mHealth, _mHealthMax);
	fontDL->DrawString(spriteBatch, _buf, _mHealthOrigin, VColorText, 0.f, Vector2(), 1.f);
	//
	Vector2 _mPowerOrigin(_mHealthOrigin.x, _mHealthOrigin.y + 25);
	wsprintf(_buf, L"POWER:%d/%d", MemGetMainPtr(PARTY_POWER_START)[member], MemGetMainPtr(PARTY_POWER_MAX_START)[member]);
	fontDL->DrawString(spriteBatch, _buf, _mPowerOrigin, VColorText, 0.f, Vector2(), 1.f);

	// Draw bottom stuff at smaller font size
	float _fScale = 0.5f;
	Vector2 _mClassOrigin(_mPortraitOrigin.x, _mPortraitOrigin.y + PARTY_PORTRAIT_HEIGHT + 4);
	UINT8 _mClassId = MemGetMainPtr(PARTY_CLASS_START)[member];
	std::wstring _mClass = NameOfClass((DeathlordClasses)_mClassId, true);
	auto _sSizeX = XMVectorGetX(fontDL->MeasureString(_mClass.c_str(), false)) * _fScale;		// Take half width because we'll scale the font by 0.5
	fontDL->DrawString(spriteBatch, _mClass.c_str(), { _mClassOrigin.x + (PARTY_PORTRAIT_WIDTH - _sSizeX) / 2, _mClassOrigin.y }, 
		VColorText, 0.f, Vector2(), _fScale);

	Vector2 _mAttrOrigin(_mClassOrigin.x, _mClassOrigin.y + 20);
	wsprintf(_buf, L"STR:%02d INT:%02d\n\nCON:%02d DEX:%02d\n\nSIZ:%02d CHA %02d",
		MemGetMainPtr(PARTY_ATTR_STR_START)[member],
		MemGetMainPtr(PARTY_ATTR_CON_START)[member],
		MemGetMainPtr(PARTY_ATTR_SIZ_START)[member],
		MemGetMainPtr(PARTY_ATTR_INT_START)[member],
		MemGetMainPtr(PARTY_ATTR_DEX_START)[member],
		MemGetMainPtr(PARTY_ATTR_CHA_START)[member]
		);
	fontDL->DrawString(spriteBatch, _buf, _mAttrOrigin, VColorText, 0.f, Vector2(), _fScale);
}

#pragma region D3D stuff
void PartyLayout::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/SpriteSheet_PartyLayout.png",
			m_partyLayoutSpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_partyLayoutSpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::PartyLayoutSpriteSheet));
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Spritesheet_Portraits_Male.png",
			m_portraitsMale.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_portraitsMale.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::PortraitsMale));
	DX::ThrowIfFailed(	// TODO: Make the female portraits
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Spritesheet_Portraits_Male.png",
			m_portraitsFemale.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_portraitsFemale.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::PortraitsFemale));
}

void PartyLayout::OnDeviceLost()
{
	m_partyLayoutSpriteSheet.Reset();
	m_portraitsMale.Reset();
	m_portraitsFemale.Reset();
}

#pragma endregion

#pragma region Other