#include "pch.h"
#include "InvOverlay.h"
#include "resource.h"
#include "Game.h"
#include "resource.h"
#include "DeathlordHacks.h"
#include <SimpleMath.h>
#include <vector>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// below because "The declaration of a static data member in its class definition is not a definition"
InvOverlay* InvOverlay::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();
extern void SetSendKeystrokesToAppleWin(bool shouldSend);

// Game data
InventorySlots selectedSlot = InventorySlots::Melee;
InventorySlots highlightedSlot = InventorySlots::TOTAL;	// No highlight by default

// Interactable shapes for the overlay
std::vector<SimpleMath::Rectangle>slotsTabsRects;

void InvOverlay::Initialize()
{
	bIsDisplayed = false;
	m_currentRect = { 0,0,0,0 };
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

// Update the inventory state based on the game's data
void InvOverlay::UpdateState()
{

}

void InvOverlay::MousePosInPixels(int x, int y)
{
	if (!bIsDisplayed)
		return;
	Vector2 mousePoint(x, y);

	// Tabs management
	highlightedSlot = InventorySlots::TOTAL;
	int iSlot = 0;
	for each (auto iRect in slotsTabsRects)
	{
		if (iRect.Contains(mousePoint))
		{
			highlightedSlot = (InventorySlots)iSlot;
			break;
		}
		iSlot++;
	}
}

void InvOverlay::LeftMouseButtonClicked(int x, int y)
{
	if (!bIsDisplayed)
		return;
	Vector2 mousePoint(x, y);

	// Tabs management
	int iSlot = 0;
	for each (auto iRect in slotsTabsRects)
	{
		if (iRect.Contains(mousePoint))
		{
			selectedSlot = (InventorySlots)iSlot;
			break;
		}
		iSlot++;
	}
}

#pragma endregion

#pragma region D3D stuff
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

#pragma region Drawing

void InvOverlay::DrawInvOverlay(
	std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, 
	std::shared_ptr<DirectX::PrimitiveBatch<VertexPositionColor>>& primitiveBatch, 
	RECT* overlayRect)
{
	// TODO: Fix this to always show the overlay in the center of the screen given the main overlay background
	//auto mmBGTexSize = GetTextureSize(m_inventoryTextureBG.Get());
	auto mmBGTexSize = DirectX::XMUINT2(1400, 700);
	auto commandList = m_deviceResources->GetCommandList();
	SimpleMath::Rectangle overlayScissorRect(*overlayRect);
	Vector2 _overlayCenter = overlayScissorRect.Center();
	m_currentRect.left = _overlayCenter.x - mmBGTexSize.x / 2;
	m_currentRect.top = _overlayCenter.y - mmBGTexSize.y / 2;
	m_currentRect.right = _overlayCenter.x + mmBGTexSize.x / 2;
	m_currentRect.bottom = _overlayCenter.y + mmBGTexSize.y / 2;

	auto gamePtr = GetGamePtr();
	auto font = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular);
	int glyphWidth = 7;		// Width of each glyph in pixels, including spacing
	int lineThickness = 3;	// Will be drawn as quads

	///// Begin Draw Border (2 quads, the black one 10px smaller per side for a 5px thickness
	primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(m_currentRect.left, m_currentRect.top, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(m_currentRect.right, m_currentRect.top, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(m_currentRect.right, m_currentRect.bottom, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(m_currentRect.left, m_currentRect.bottom, 0), ColorAmber)
	);
	primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(m_currentRect.left+5, m_currentRect.top+5, 0), static_cast<XMFLOAT4>(Colors::Black)),
		VertexPositionColor(XMFLOAT3(m_currentRect.right-5, m_currentRect.top+5, 0), static_cast<XMFLOAT4>(Colors::Black)),
		VertexPositionColor(XMFLOAT3(m_currentRect.right-5, m_currentRect.bottom-5, 0), static_cast<XMFLOAT4>(Colors::Black)),
		VertexPositionColor(XMFLOAT3(m_currentRect.left+5, m_currentRect.bottom-5, 0), static_cast<XMFLOAT4>(Colors::Black))
	);
	///// End Draw Border

	///// Begin Draw inventory slots tabs
	float invSlotsOriginX = m_currentRect.left + 20.f;
	float invSlotsOriginY = m_currentRect.top + 200.f;
	int stringHalfSpacing = 10;			// Half spacing between the strings
	int invSlotBegin = invSlotsOriginX;	// Beginning of the inventory slot string, including half spacing
	int invSlotEnd = 0;					// End of the inventory slot string, including half spacing
	slotsTabsRects.clear();
	int iSlot = 0;
	for each (auto _str in StringsInventorySlots)
	{
		// Calculate the clickable rectangle for mouse input
		invSlotEnd = invSlotBegin + stringHalfSpacing + _str.length() * glyphWidth + stringHalfSpacing;
		SimpleMath::Rectangle r(invSlotBegin, invSlotsOriginY, invSlotEnd - invSlotBegin, 20.f);
		slotsTabsRects.push_back(r);
		// Draw selected tab graphics (border around the tab)
		if ((InventorySlots)iSlot == selectedSlot)
		{
			primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(invSlotBegin, invSlotsOriginY - 10.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY + lineThickness + 20.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotBegin, invSlotsOriginY + lineThickness + 20.f, 0), ColorAmber)
			);
			primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(invSlotBegin + lineThickness, invSlotsOriginY - 10.f + lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY - 10.f + lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY + 20.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotBegin + lineThickness, invSlotsOriginY + 20.f, 0), static_cast<XMFLOAT4>(Colors::Black))
			);
		}
		if (((InventorySlots)iSlot == highlightedSlot) && (selectedSlot != highlightedSlot))
		{
			primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(invSlotBegin, invSlotsOriginY - 10.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY + lineThickness + 20.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotBegin, invSlotsOriginY + lineThickness + 20.f, 0), ColorAmberDark)
			);
			primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(invSlotBegin + lineThickness, invSlotsOriginY - 10.f + lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY - 10.f + lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY + 20.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotBegin + lineThickness, invSlotsOriginY + 20.f, 0), static_cast<XMFLOAT4>(Colors::Black))
			);
		}
		// Draw tab string
		font->DrawString(spriteBatch.get(), _str.c_str(),
			Vector2(invSlotBegin + stringHalfSpacing, invSlotsOriginY),
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
		invSlotBegin = invSlotEnd;
		iSlot++;
	}
	// Draw line below tabs
	primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(invSlotsOriginX, invSlotsOriginY + 20.f, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY + 20.f, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY + 20.f + lineThickness, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(invSlotsOriginX, invSlotsOriginY + 20.f + lineThickness, 0), ColorAmber)
	);
	///// End Draw inventory slots tabs

	///// Begin Draw Inventory Headers
	int iCol = 0;
	int ctCols = 0;
	int colBegin = invSlotsOriginX;
	int rowBegin = invSlotsOriginY + 40;
	std::array<std::string, 5>stringsHeaders;
	std::array<int, 5>widthHeaders;
	if (selectedSlot < InventorySlots::Chest)
	{
		stringsHeaders = StringsHeadersWeapons;
		widthHeaders = WidthHeadersWeapons;
		ctCols = 5;
	}
	else
	{
		stringsHeaders = StringsHeadersOther;
		widthHeaders = WidthHeadersOthers;
		ctCols = 4;
	}
	for (size_t i = 0; i < ctCols; i++)
	{
		font->DrawString(spriteBatch.get(), stringsHeaders[i].c_str(),
			Vector2(colBegin, rowBegin),
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
		colBegin += widthHeaders[i] + stringHalfSpacing;
	}
	///// End Draw Inventory Headers


	//spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlayBackground), mmBGTexSize,
//	m_currentRect, nullptr, Colors::White, 0.f, XMFLOAT2());

	bIsDisplayed = true;
}

#pragma endregion