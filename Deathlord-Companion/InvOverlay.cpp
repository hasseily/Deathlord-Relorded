#include "pch.h"
#include "InvOverlay.h"
#include "resource.h"
#include "Game.h"
#include "resource.h"
#include "DeathlordHacks.h"

using namespace DirectX;

// below because "The declaration of a static data member in its class definition is not a definition"
InvOverlay* InvOverlay::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();
extern void SetSendKeystrokesToAppleWin(bool shouldSend);

void InvOverlay::Initialize()
{
	bIsDisplayed = false;
	m_currentRect = { 0,0,0,0 };
}


void InvOverlay::DrawInvOverlay(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, RECT* overlayRect)
{
	// TODO: Fix this to always show the overlay in the center of the screen given the main overlay background
	auto mmBGTexSize = GetTextureSize(m_inventoryTextureBG.Get());
	auto commandList = m_deviceResources->GetCommandList();
	SimpleMath::Rectangle overlayScissorRect(*overlayRect);
	Vector2 _overlayCenter = overlayScissorRect.Center();
	m_currentRect.left = _overlayCenter.x - mmBGTexSize.x / 2;
	m_currentRect.top = _overlayCenter.y - mmBGTexSize.y / 2;
	m_currentRect.right = _overlayCenter.x + mmBGTexSize.x / 2;
	m_currentRect.bottom = _overlayCenter.y + mmBGTexSize.y / 2;

	spriteBatch->Begin(commandList, DirectX::SpriteSortMode_Deferred);
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlayBackground), mmBGTexSize,
		m_currentRect, nullptr, Colors::White, 0.f, XMFLOAT2());
	auto gamePtr = GetGamePtr();
	(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), "TESTING DRAWING",
		Vector2(m_currentRect.left + 10.f, m_currentRect.top + 20.f), Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
	spriteBatch->End();
	bIsDisplayed = true;
}

void InvOverlay::ShowInvOverlay()
{
	SetSendKeystrokesToAppleWin(false);
	bIsDisplayed = true;
}

void InvOverlay::HideInvOverlay()
{
	SetSendKeystrokesToAppleWin(true);
	bIsDisplayed = false;
}

bool InvOverlay::IsInvOverlayDisplayed()
{
	return bIsDisplayed;
}

#pragma region actions

void InvOverlay::leftMouseButtonClicked(int x, int y)
{
	DirectX::XMFLOAT2 clickPoint = XMFLOAT2((float)x, (float)y);
}

#pragma endregion

# pragma region D3D stuff
void InvOverlay::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/InventoryTest.png",
			m_inventoryTextureBG.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_inventoryTextureBG.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::InvOverlayBackground));
}

void InvOverlay::OnDeviceLost()
{
	m_inventoryTextureBG.Reset();
}

#pragma endregion