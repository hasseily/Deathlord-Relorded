#include "pch.h"
#include "PartyLayout.h"
#include "DeathlordHacks.h"
#include "Game.h"
#include <algorithm>

// below because "The declaration of a static data member in its class definition is not a definition"
PartyLayout* PartyLayout::s_instance;

void PartyLayout::Initialize()
{

}

bool PartyLayout::Update(UINT8 leader)
{
	return false;
}

void PartyLayout::Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch)
{
	auto plTexSize = GetTextureSize(m_partyLayoutSpriteSheet.Get());

//	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::PartyLayoutSpriteSheet), mmTexSize,
//		_overlayPinPosInMap, &_pinRect, Colors::White, 0.f, _originPin);
}

void PartyLayout::RenderMember(UINT8 member, UINT16 originX, UINT16 originY)
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

}

#pragma region D3D stuff
void PartyLayout::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/PartyLayoutSpriteSheet.png",
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