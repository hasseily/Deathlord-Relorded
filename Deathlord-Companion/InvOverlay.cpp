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
static InventorySlots selectedSlot = InventorySlots::Melee;
static InventorySlots highlightedSlot = InventorySlots::TOTAL;	// No highlight by default
static UINT8 partySize = 6;		// Also defined in InvManager.cpp
static UINT8 glyphWidth = 7;		// Width of each glyph in pixels, including spacing
static UINT8 glyphHeight = 16;

// InvOverlay sprite sheet rectangles
// It is split into 4 rows of 8 columns of 28x32 pixels
// The player sprites are the first 2 rows.
// The last 2 rows are the other sprites which span 2 cols each
static RECT spRectTrashClosed	= { 28 * 0, 32 * 2, 28 * 2, 32 * 3 };
static RECT spRectTrashOpen	= { 28 * 2, 32 * 2, 28 * 4, 32 * 3 };
static RECT spRectStash		= { 28 * 4, 32 * 2, 28 * 6, 32 * 3 };
static RECT spRectInvEmpty		= { 28 * 0, 32 * 3, 28 * 2, 32 * 4 };
static RECT spRectInvWorn		= { 28 * 2, 32 * 3, 28 * 4, 32 * 4 };
static RECT spRectInvCarried	= { 28 * 4, 32 * 3, 28 * 6, 32 * 4 };

// Interactable shapes for the overlay
static std::vector<SimpleMath::Rectangle>slotsTabsRects;

#pragma region utils

inline int PaddingToCenterString(UINT8 maxStringLength, UINT8 stringLength)
{
	return ((maxStringLength - stringLength) * glyphWidth) / 2;
}

#pragma endregion

#pragma region main
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
#pragma endregion

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
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/InvOverlaySpriteSheet.png",
			m_invOverlaySpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_invOverlaySpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet));
}

void InvOverlay::OnDeviceLost()
{
	m_invOverlaySpriteSheet.Reset();
}

#pragma endregion

#pragma region Drawing

void InvOverlay::DrawInvOverlay(
	std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, 
	std::shared_ptr<DirectX::PrimitiveBatch<VertexPositionColor>>& primitiveBatch, 
	RECT* overlayRect)
{
	auto mmBGTexSize = DirectX::XMUINT2(1400, 700);
	auto mmSSTextureSize = GetTextureSize(m_invOverlaySpriteSheet.Get());
	auto commandList = m_deviceResources->GetCommandList();
	SimpleMath::Rectangle overlayScissorRect(*overlayRect);
	Vector2 _overlayCenter = overlayScissorRect.Center();
	m_currentRect.left = _overlayCenter.x - mmBGTexSize.x / 2;
	m_currentRect.top = _overlayCenter.y - mmBGTexSize.y / 2;
	m_currentRect.right = _overlayCenter.x + mmBGTexSize.x / 2;
	m_currentRect.bottom = _overlayCenter.y + mmBGTexSize.y / 2;

	auto gamePtr = GetGamePtr();
	auto font = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular);
	int lineThickness = 3;	// Will be drawn as quads
	int borderPadding = 20;	// Empty Pixels inside the border
	int invMemberColWidth = 150;	// column width for each party member (and stash)

	RECT innerRect = {		// The inner rect after padding has been applied
		m_currentRect.left + borderPadding,
		m_currentRect.top + borderPadding,
		m_currentRect.right - borderPadding,
		m_currentRect.bottom - borderPadding };

	float invSlotsOriginX = innerRect.left;			// beginning of inventory slots
	float invSlotsOriginY = innerRect.top + 200.f;

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

		///// Begin Draw crossed lines
	primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(innerRect.left,	invSlotsOriginY + 60, 0),					ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right,	invSlotsOriginY + 60, 0),					ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right,	invSlotsOriginY + 60 + lineThickness, 0),	ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.left,	invSlotsOriginY + 60 + lineThickness, 0),	ColorAmber)
	);
	primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(innerRect.right - invMemberColWidth,					innerRect.top, 0),		ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right - invMemberColWidth + lineThickness,	innerRect.top, 0),		ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right - invMemberColWidth + lineThickness,	innerRect.bottom, 0),	ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right - invMemberColWidth,					innerRect.bottom, 0),	ColorAmber)
	);
	///// End Draw crossed lines
	
	///// Begin Draw inventory slots tabs
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
		// Draw tab graphics (border around the tab)
		if (((InventorySlots)iSlot == highlightedSlot) && (selectedSlot != highlightedSlot))	// Highlighted
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
			primitiveBatch->DrawTriangle(
				VertexPositionColor(XMFLOAT3(invSlotEnd - 11.f, invSlotsOriginY - 10.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY + 1, 0), ColorAmberDark)
			);
			primitiveBatch->DrawTriangle(
				VertexPositionColor(XMFLOAT3(invSlotEnd - 10.f + lineThickness, invSlotsOriginY - 10.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black))
			);
		}
		if ((InventorySlots)iSlot == selectedSlot)	// Selected
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
			primitiveBatch->DrawTriangle(
				VertexPositionColor(XMFLOAT3(invSlotEnd -11.f, invSlotsOriginY - 10.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY + 1, 0), ColorAmber)
			);
			primitiveBatch->DrawTriangle(
				VertexPositionColor(XMFLOAT3(invSlotEnd - 10.f + lineThickness, invSlotsOriginY - 10.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black))
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

	///// Begin Draw Column Headers (party members)
	int xCol = 700;		// x value at start of column drawing
	int maxGlyphs = 12;	// Max number of glyphs in the column
	int colWidth = maxGlyphs * glyphWidth;	// Column width for party members
	for (size_t iMember = 0; iMember < partySize; iMember++)
	{
		std::string _bufStr;
		int yCol = innerRect.top;	// y value of the start of drawing
		// First draw the class icon in the center of the column
		UINT8 memberClass = MemGetMainPtr(PARTY_CLASS_START)[iMember];
		int memberLeft = 28 *(memberClass % 8);
		int memberTop = 32 *(memberClass / 8);
		RECT memberSpriteRect = { memberLeft, memberTop, memberLeft + 28, memberTop + 32 };
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
			mmSSTextureSize, XMFLOAT2(xCol + (colWidth - 28)/2, yCol), &memberSpriteRect, Colors::White, 0.f, XMFLOAT2());
		// Then draw the member name, class and AC
		yCol += 32 + 5;
		_bufStr = StringFromMemory(PARTY_NAME_START + (iMember * 0x09), maxGlyphs);
		font->DrawString(spriteBatch.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
		yCol += glyphHeight + 2;
		_bufStr = NameOfClass((DeathlordClasses)memberClass, false);
		font->DrawString(spriteBatch.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
		yCol += glyphHeight + 2;
		UINT8 memberRace = MemGetMainPtr(PARTY_RACE_START)[iMember];
		_bufStr = NameOfRace((DeathlordRaces)memberRace, false);
		font->DrawString(spriteBatch.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
		yCol += glyphHeight + 2;
		UINT8 memberArmor = MemGetMainPtr(PARTY_ARMORCLASS_START)[iMember];
		_bufStr = "AC " + std::to_string((int)10 - memberArmor);
		font->DrawString(spriteBatch.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);

		xCol += colWidth;
	}
	///// End Draw Column Headers (party members)

	bIsDisplayed = true;
}

#pragma endregion
