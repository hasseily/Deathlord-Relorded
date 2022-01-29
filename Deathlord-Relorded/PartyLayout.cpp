#include "pch.h"
#include "PartyLayout.h"
#include "DeathlordHacks.h"
#include "Game.h"
#include "InvManager.h"
#include <algorithm>

extern std::unique_ptr<Game>* GetGamePtr();

// below because "The declaration of a static data member in its class definition is not a definition"
PartyLayout* PartyLayout::s_instance;

InvManager* m_invMgr;

void PartyLayout::Initialize()
{
	m_partySize = 6;
	m_currentLeader = 0;
}

void PartyLayout::SetPartySize(UINT8 size)
{
	if (size < 7)
		m_partySize = size;
}

UINT8 PartyLayout::GetPartySize()
{
	return m_partySize;
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
	m_invMgr = InvManager::GetInstance();
	for (UINT8 i = 0; i < m_partySize; i++)
	{
		RenderMember(i, spriteBatch, r.x + PARTY_LAYOUT_X[i], r.y + PARTY_LAYOUT_Y[i]);
	}
	// Now render the top background layer
	auto bgltTexSize = GetTextureSize(m_bgLayerTop.Get());
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::MainBackgroundLayerTop), bgltTexSize,
		Vector2(r.x, r.y));
	// And the final dynamic layer
	for (UINT8 i = 0; i < m_partySize; i++)
	{
		RenderMemberTopLayer(i, spriteBatch, r.x + PARTY_LAYOUT_X[i], r.y + PARTY_LAYOUT_Y[i]);
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
	auto fontPR3Regular = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontPR3Regular);
	const UINT8 _bufsize = 200;
	wchar_t _buf[_bufsize];

	// padding of 2 all around
	UINT8 _pad = 2;

	///// Draw portrait
	RECT _portraitRect = {	MemGetMainPtr(PARTY_RACE_START)[member] * PARTY_PORTRAIT_WIDTH,
							MemGetMainPtr(PARTY_CLASS_START)[member] * PARTY_PORTRAIT_HEIGHT,
							0, 0 };
	_portraitRect.right = _portraitRect.left + PARTY_PORTRAIT_WIDTH;
	_portraitRect.bottom = _portraitRect.top + PARTY_PORTRAIT_HEIGHT;
	Vector2 _mPortraitOrigin(originX + _pad, originY + _pad);
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
	if (_mStatus & (UINT8)DeathlordCharStatus::ILL)
	{
		fontDLInverse->DrawString(spriteBatch, L"ILL", _mStatusOrigin, VColorText, 0.f, Vector2(), 1.0f);
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
	// the 2 RIP should be combined but it'd be fun to have someone get those 2 statuses (if possible)
	// and complain the game has a bug
	if (_mStatus & (UINT8)DeathlordCharStatus::RIP)
	{
		fontDLInverse->DrawString(spriteBatch, L"RIP", _mStatusOrigin, VColorText, 0.f, Vector2(), 1.0f);
		_mStatusOrigin.y -= 17;
	}
	if (_mStatus & (UINT8)DeathlordCharStatus::RIP2)
	{
		fontDLInverse->DrawString(spriteBatch, L"RIP", _mStatusOrigin, Colors::Red, 0.f, Vector2(), 1.0f);
		_mStatusOrigin.y -= 17;
	}

	///// Draw name with black outline
	std::wstring _mName = StringFromMemory(PARTY_NAME_START + (member*9), 11);
	_mName.resize(11, ' ');	// Looks better if using an inverse font, it goes closer to the level string
	Vector2 _mNameOrigin(_mPortraitOrigin.x + PARTY_PORTRAIT_WIDTH + 6, _mPortraitOrigin.y + 3);
	//fontDL->DrawString(spriteBatch, _mName.c_str(), { _mNameOrigin.x - 1.f, _mNameOrigin.y - 1.f } , Colors::Black, 0.f, Vector2(), 1.0f);
	//fontDL->DrawString(spriteBatch, _mName.c_str(), { _mNameOrigin.x - 1.f, _mNameOrigin.y + 1.f }, Colors::Black, 0.f, Vector2(), 1.0f);
	//fontDL->DrawString(spriteBatch, _mName.c_str(), { _mNameOrigin.x + 1.f, _mNameOrigin.y - 1.f }, Colors::Black, 0.f, Vector2(), 1.0f);
	fontDL->DrawString(spriteBatch, _mName.c_str(), { _mNameOrigin.x + 1.f, _mNameOrigin.y + 1.f }, VColorCurtain, 0.f, Vector2(), 1.0f);
	if (MemGetMainPtr(PARTY_CURRENT_CHAR_POS)[0] == member)
		fontDLInverse->DrawString(spriteBatch, _mName.c_str(), _mNameOrigin, Colors::Yellow, 0.f, Vector2(), 1.0f);
	else
		fontDL->DrawString(spriteBatch, _mName.c_str(), _mNameOrigin, VColorText, 0.f, Vector2(), 1.0f);

	///// Draw stuff to the right of the portrait
	Vector2 _mLevelOrigin(originX + PARTY_LAYOUT_WIDTH - 14*4 - _pad, _mNameOrigin.y);
	//if (MemGetMainPtr(PARTY_LEVELPLUS_START)[member] > 0)
		swprintf_s(_buf, _bufsize, L"%02d+%d", MemGetMainPtr(PARTY_LEVEL_START)[member],
			MemGetMainPtr(PARTY_LEVELPLUS_START)[member]);
	//else
	//	swprintf_s(_buf, _bufsize, L"%02d", MemGetMainPtr(PARTY_LEVEL_START)[member]);
	auto _mLevelColor = VColorText;
	if (MemGetMainPtr(PARTY_LEVELPLUS_START)[member] > 0)
		_mLevelColor = Colors::DarkOrange;
	//fontDL->DrawString(spriteBatch, _buf, { _mLevelOrigin.x - 1.f, _mLevelOrigin.y - 1.f }, Colors::Black, 0.f, Vector2(), 1.f);
	//fontDL->DrawString(spriteBatch, _buf, { _mLevelOrigin.x - 1.f, _mLevelOrigin.y + 1.f }, Colors::Black, 0.f, Vector2(), 1.f);
	//fontDL->DrawString(spriteBatch, _buf, { _mLevelOrigin.x + 1.f, _mLevelOrigin.y - 1.f }, Colors::Black, 0.f, Vector2(), 1.f);
	fontDL->DrawString(spriteBatch, _buf, { _mLevelOrigin.x + 1.f, _mLevelOrigin.y + 1.f }, Colors::Black, 0.f, Vector2(), 1.f);
	fontDL->DrawString(spriteBatch, _buf, _mLevelOrigin, _mLevelColor, 0.f, Vector2(), 1.f);
	//
	Vector2 _mHealthOrigin(_mNameOrigin.x, _mNameOrigin.y + 22);
	UINT16 _mHealth = MemGetMainPtr(PARTY_HEALTH_LOBYTE_START)[member] + ((UINT16)MemGetMainPtr(PARTY_HEALTH_HIBYTE_START)[member] << 8);
	UINT16 _mHealthMax = MemGetMainPtr(PARTY_HEALTH_MAX_LOBYTE_START)[member] + ((UINT16)MemGetMainPtr(PARTY_HEALTH_MAX_HIBYTE_START)[member] << 8);
	swprintf_s(_buf, _bufsize, L"H %04d/%04d", _mHealth, _mHealthMax);
	fontDL->DrawString(spriteBatch, _buf, _mHealthOrigin, VColorText, 0.f, Vector2(), 1.f);
	//
	Vector2 _mPowerOrigin(_mHealthOrigin.x, _mHealthOrigin.y + 22);
	swprintf_s(_buf, _bufsize, L"P %03d/%03d", MemGetMainPtr(PARTY_POWER_START)[member], MemGetMainPtr(PARTY_POWER_MAX_START)[member]);
	fontDL->DrawString(spriteBatch, _buf, _mPowerOrigin, VColorText, 0.f, Vector2(), 1.f);
	//
	Vector2 _mGoldOrigin(_mPowerOrigin.x, _mPowerOrigin.y + 22);
	UINT16 _mGold = MemGetMainPtr(PARTY_GOLD_LOBYTE_START)[member] + ((UINT16)MemGetMainPtr(PARTY_GOLD_HIBYTE_START)[member] << 8);
	swprintf_s(_buf, _bufsize, L"G %05d", _mGold);
	fontDL->DrawString(spriteBatch, _buf, _mGoldOrigin, VColorText, 0.f, Vector2(), 1.f);
	//
	Vector2 _mACOrigin(originX + PARTY_LAYOUT_WIDTH - 14 * 5 - _pad, _mGoldOrigin.y);
	swprintf_s(_buf, _bufsize, L"AC%+03d", 10 - MemGetMainPtr(PARTY_ARMORCLASS_START)[member]);
	fontDL->DrawString(spriteBatch, _buf, _mACOrigin, VColorText, 0.f, Vector2(), 1.f);
	//
	Vector2 _mFoodOrigin(_mGoldOrigin.x, _mACOrigin.y + 22);
	UINT8 _mFood = MemGetMainPtr(PARTY_FOOD_START)[member];
	swprintf_s(_buf, _bufsize, L"F %03d", _mFood);
	if (_mFood < 10)
		fontDL->DrawString(spriteBatch, _buf, _mFoodOrigin, Colors::Red, 0.f, Vector2(), 1.f);
	else if (_mFood < 20)
		fontDL->DrawString(spriteBatch, _buf, _mFoodOrigin, Colors::DarkOrange, 0.f, Vector2(), 1.f);
	else
		fontDL->DrawString(spriteBatch, _buf, _mFoodOrigin, VColorText, 0.f, Vector2(), 1.f);
	//
	Vector2 _mTorchesOrigin(originX + PARTY_LAYOUT_WIDTH - 14 * 4 - _pad, _mFoodOrigin.y);
	swprintf_s(_buf, _bufsize, L"T %02d", MemGetMainPtr(PARTY_TORCHES_START)[member]);
	fontDL->DrawString(spriteBatch, _buf, _mTorchesOrigin, VColorText, 0.f, Vector2(), 1.f);

	// Draw bottom stuff at smaller font size
	float _fScale = 0.5f;
	Vector2 _mClassOrigin(_mPortraitOrigin.x, _mPortraitOrigin.y + PARTY_PORTRAIT_HEIGHT + 9);
	UINT8 _mClassId = MemGetMainPtr(PARTY_CLASS_START)[member];
	std::wstring _mClass = NameOfClass((DeathlordClasses)_mClassId, true);
	auto _sSizeX = XMVectorGetX(fontDL->MeasureString(_mClass.c_str(), false)) * _fScale;		// Take half width because we'll scale the font by 0.5
	fontDL->DrawString(spriteBatch, _mClass.c_str(), { _mClassOrigin.x + (PARTY_PORTRAIT_WIDTH - _sSizeX) / 2, _mClassOrigin.y }, 
		VColorText, 0.f, Vector2(), _fScale);
	//
	Vector2 _mAttrOrigin(_mClassOrigin.x, _mClassOrigin.y + 18);
	swprintf_s(_buf, _bufsize, L"STR:%02d INT:%02d\n\nCON:%02d DEX:%02d\n\nSIZ:%02d CHA %02d",
		MemGetMainPtr(PARTY_ATTR_STR_START)[member],
		MemGetMainPtr(PARTY_ATTR_CON_START)[member],
		MemGetMainPtr(PARTY_ATTR_SIZ_START)[member],
		MemGetMainPtr(PARTY_ATTR_INT_START)[member],
		MemGetMainPtr(PARTY_ATTR_DEX_START)[member],
		MemGetMainPtr(PARTY_ATTR_CHA_START)[member]
		);
	fontDL->DrawString(spriteBatch, _buf, _mAttrOrigin, VColorText, 0.f, Vector2(), _fScale);
	//
	// Draw inventory
	auto _inv = m_invMgr->AllInventoryForMember(member);
	std::wstring _sItem;
	Vector2 _mInvOrigin(_mNameOrigin.x + 38, _mFoodOrigin.y + 27);
	for each (InvInstance _item in _inv)
	{
		_sItem = L"   .............";		// length of 13 prefixed by count
		// First set the name, with dot padding to the right
		_sItem.replace(3, _item.item->name.size(), _item.item->name);
		if (_item.item->id != EMPTY_ITEM_ID)
		{
			if (_item.charges == 0)	// Infinite!
				_sItem.replace(0, 2, L"**");
			else if (_item.charges != EMPTY_CHARGES_COUNT)
			{
				_sItem.replace(0, 2, L"00");
				if (_item.charges < 10)
					_sItem.replace(1, 1, to_wstring(_item.charges));
				else
					_sItem.replace(0, 2, to_wstring(_item.charges));
			}
			if (MemGetMainPtr(PARTY_WEAP_READY_START)[member] == _item.row)	// readied
				fontDLInverse->DrawString(spriteBatch, L"IN HANDS",
					{ _mInvOrigin.x + 120, _mInvOrigin.y }, Colors::Yellow, 0.f, Vector2(), _fScale);
			else if (_item.equipped)	// equipped
				fontDLInverse->DrawString(spriteBatch, L"EQUIPPED",
					{ _mInvOrigin.x + 120, _mInvOrigin.y }, Colors::Green, 0.f, Vector2(), _fScale);
			else // not equipped
				fontDLInverse->DrawString(spriteBatch, L"UNUSABLE",
					{ _mInvOrigin.x + 120, _mInvOrigin.y }, VColorText, 0.f, Vector2(), _fScale);
		}
		fontDL->DrawString(spriteBatch, _sItem.c_str(), _mInvOrigin, VColorText, 0.f, Vector2(), _fScale);
		_mInvOrigin.y += 9;
	}
	
}

void PartyLayout::RenderMemberTopLayer(UINT8 member, DirectX::SpriteBatch* spriteBatch, UINT16 originX, UINT16 originY)
{
	// Need to render the party #
	// And some overlay to show the active member
	auto gamePtr = GetGamePtr();
	auto fontDL = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular);
	auto fontDLInverse = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLInverse);
	auto fontPR3Regular = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontPR3Regular);
	const UINT8 _bufsize = 200;
	wchar_t _buf[_bufsize];

	// padding of 2 all around
	UINT8 _pad = 2;
	///// Draw party position
	Vector2 _mPortraitOrigin(originX + _pad, originY + _pad);
	swprintf_s(_buf, _bufsize, L"%d", member + 1);
	//Emboss down slightly
	fontDL->DrawString(spriteBatch, _buf, { _mPortraitOrigin.x - 2, _mPortraitOrigin.y - 2 }, VColorShadow, 0.f, Vector2(), 1.0f);
	fontDL->DrawString(spriteBatch, _buf, { _mPortraitOrigin.x - 2, _mPortraitOrigin.y - 1 }, VColorShadow, 0.f, Vector2(), 1.0f);
	//	fontDL->DrawString(spriteBatch, _buf, { _mPortraitOrigin.x - 2, _mPortraitOrigin.y + 1 }, VColorCurtain, 0.f, Vector2(), 1.0f);
//	fontDL->DrawString(spriteBatch, _buf, { _mPortraitOrigin.x + 0, _mPortraitOrigin.y - 0 }, VColorCurtain, 0.f, Vector2(), 1.0f);
//	fontDL->DrawString(spriteBatch, _buf, { _mPortraitOrigin.x + 0, _mPortraitOrigin.y + 0 }, VColorCurtain, 0.f, Vector2(), 1.0f);
	if (MemGetMainPtr(PARTY_CURRENT_CHAR_POS)[0] == member)
	{
		fontDL->DrawString(spriteBatch, _buf, { _mPortraitOrigin.x - 1, _mPortraitOrigin.y },
			Colors::Yellow, 0.f, Vector2(), 1.0f);
	}
	else
	{
		fontDL->DrawString(spriteBatch, _buf, { _mPortraitOrigin.x - 1, _mPortraitOrigin.y },
			VColorText, 0.f, Vector2(), 1.0f);
	}
}

#pragma region D3D stuff
void PartyLayout::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Background_Relorded_LayerTop.png",
			m_bgLayerTop.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_bgLayerTop.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::MainBackgroundLayerTop));
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/SpriteSheet_PartyLayout.png",
			m_partyLayoutSpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_partyLayoutSpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::PartyLayoutSpriteSheet));
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Spritesheet_Portraits_Male.jpg",
			m_portraitsMale.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_portraitsMale.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::PortraitsMale));
	DX::ThrowIfFailed(	// TODO: Make the female portraits
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Spritesheet_Portraits_Female.jpg",
			m_portraitsFemale.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_portraitsFemale.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::PortraitsFemale));
}

void PartyLayout::OnDeviceLost()
{
	m_bgLayerTop.Reset();
	m_partyLayoutSpriteSheet.Reset();
	m_portraitsMale.Reset();
	m_portraitsFemale.Reset();
}

#pragma endregion

#pragma region Other