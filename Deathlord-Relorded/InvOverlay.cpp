#include "pch.h"
#include "InvOverlay.h"
#include "resource.h"
#include "Game.h"
#include "DeathlordHacks.h"
#include "Descriptors.h"
#include <SimpleMath.h>
#include <vector>
#include "InvManager.h"
#include "Emulator/CPU.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// below because "The declaration of a static data member in its class definition is not a definition"
InvOverlay* InvOverlay::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();
extern void SetSendKeystrokesToAppleWin(bool shouldSend);

// Game data
static InvManager* invMgr = InvManager::GetInstance();
std::vector<InvInstance>m_invRows;	// all inventory instances for this slot
static InventorySlots selectedSlot = InventorySlots::Melee;
static InventorySlots highlightedSlot = InventorySlots::TOTAL;	// No highlight by default
static EquipInteractableRect highlightedRect;	// Rect that is currently highlighted
static EquipInteractableRect nohighlightsRect;	// When there are no highlights, highlightedRect == nohighlightsRect

// Drawing layout
static RECT innerRect;
static UINT8 glyphWidth = 7;			// Width of each glyph in pixels, including spacing
static UINT8 glyphHeight = 16;
static UINT8 invRowSpacing = 14;		// Spacing between rows of the inventory
static UINT8 verticalBarWidth = 8;		// Added width of vertical bar between members and stash
static int widthTabsArea = 515;			// Width the left tabs area
static UINT8 maxPartySize = 6;

// InvOverlay sprite sheet rectangles
// It is split into 4 rows of 8 columns of 28x32 pixels
// The player sprites are the first 2 rows.
// The last 2 rows are the other sprites
static RECT spRectTrashClosed	= { 28 * 0, 32 * 2, 28 * 1, 32 * 3 };
static RECT spRectTrashOpen		= { 28 * 1, 32 * 2, 28 * 2, 32 * 3 };
static RECT spRectStash			= { 28 * 4, 32 * 2, 28 * 6, 32 * 3 };
static RECT spRectInvEmpty		= { 28 * 0, 32 * 3, 28 * 1, 32 * 4 };
static RECT spRectInvCarried	= { 28 * 1, 32 * 3, 28 * 2, 32 * 4 };
static RECT spRectInvWorn		= { 28 * 2, 32 * 3, 28 * 3, 32 * 4 };

static std::array<std::wstring, (int)InventorySlots::TOTAL>StringsInventorySlots =
{
	L"MELEE", L"RANGED", L"CHEST", L"SHIELD", L"MISC", L"JEWELRY", L"TOOL", L"SCROLL"
};

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
	Overlay::Initialize();
	bShouldBlockKeystrokes = true;
	m_width = 1150;
	m_height = 500;
	m_spritesheetDescriptor = TextureDescriptors::InvOverlaySpriteSheet;
	m_spritesheetPath = L"Assets/InvOverlaySpriteSheet.png";
	m_curtainColor = { 0.f, 0.f, 0.f, 0.2f };
	m_type = OverlayType::Bordered;

	m_currentItemInstance = 0;
}

#pragma endregion

#pragma region actions

// Update the inventory state based on the game's data
void InvOverlay::Update()
{
	m_invRows = invMgr->AllInventoryInSlot(selectedSlot);
	m_partySize = MemGetMainPtr(PARTY_PARTYSIZE)[0];
}

void InvOverlay::MousePosInPixels(int x, int y)
{
	if (!(m_overlayState == OverlayState::Displayed))
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
			return;
		}
		iSlot++;
	}
	for each (auto& eiRect in v_eIRects)
	{
		if (eiRect.eRect.Contains(mousePoint))
		{
			if (eiRect.eMember >= m_partySize)	// it's the stash
			{
				// check if it's full. If so, don't highlight
				if (invMgr->StashSlotCount(selectedSlot) >= STASH_MAX_ITEMS_PER_SLOT)
					return;
			}
			highlightedRect = eiRect;
			return;
		}
	}
	for each (auto & trashiRect in v_trashIRects)
	{
		if (trashiRect.eRect.Contains(mousePoint))
		{
			highlightedRect = trashiRect;
			return;
		}
	}
	highlightedRect = nohighlightsRect;
}

void InvOverlay::LeftMouseButtonClicked(int x, int y)
{
	if (!(m_overlayState == OverlayState::Displayed))
		return;
	Vector2 mousePoint(x, y);

	// Tabs management
	int iSlot = 0;
	for each (auto iRect in slotsTabsRects)
	{
		if (iRect.Contains(mousePoint))
		{
			selectedSlot = (InventorySlots)iSlot;
			goto ENDLEFTCLICK;
		}
		iSlot++;
	}
	for each (auto & eiRect in v_eIRects)
	{
		if (eiRect.eRect.Contains(mousePoint))
		{
			InvInstance* _invI = &m_invRows.at(eiRect.eRow);
			if (eiRect.eMember < m_partySize)	// give to a person
			{
				if (_invI->owner < m_partySize)
				{
					invMgr->ExchangeBetweeenPartyMembers(
						_invI->owner,
						eiRect.eMember,
						selectedSlot
					);
				}
				else
				{
					// exchange stash->person
					invMgr->SwapStashWithPartyMember(
						_invI->owner - m_partySize,	// the stash slot
						eiRect.eMember,
						selectedSlot
					);
				}
			}
			else // give to the stash
			{
				// if stash is full it will do nothing
				invMgr->PutInStash(_invI->owner, selectedSlot);
			}
			goto ENDLEFTCLICK;
		}
	}
	for each (auto & trashiRect in v_trashIRects)
	{
		if (trashiRect.eRect.Contains(mousePoint))
		{
			invMgr->DeleteItem(selectedSlot, trashiRect.eMember - m_partySize);
			goto ENDLEFTCLICK;
		}
	}
ENDLEFTCLICK:
	// Always recalculate AC after a change.
	// Deathlord only recalcs AC during fights and if you view a party member's info
	ExecuteDeathlordArmorClassRoutine();
	Update();
}

#pragma endregion


#pragma region Drawing

void InvOverlay::Render(SimpleMath::Rectangle r)
{
	if (!bShouldDisplay)
	{
		if (m_overlayState == OverlayState::Displayed)
		{
			// Kill the overlay, it shouldn't be here.
			// Don't bother animating it
			m_overlayState = OverlayState::Hidden;
		}
		return;
	}
	this->Update();

	// Now check if we should animate the display as it appears
	if (!(m_overlayState == OverlayState::Displayed))
	{
		// No animation for inventory overlay showing up
		// m_state = OverlayState::TransitionIn;
		m_overlayState = OverlayState::Displayed;
	}

	Overlay::PreRender(r);

	auto mmBGTexSize = DirectX::XMUINT2(m_width, m_height);
	auto mmSSTextureSize = GetTextureSize(m_overlaySpriteSheet.Get());
	Vector2 _overlayCenter = r.Center();
	m_currentRect.left = _overlayCenter.x - mmBGTexSize.x / 2;
	m_currentRect.top = _overlayCenter.y - mmBGTexSize.y / 2;
	m_currentRect.right = _overlayCenter.x + mmBGTexSize.x / 2;
	m_currentRect.bottom = _overlayCenter.y + mmBGTexSize.y / 2;

	auto gamePtr = GetGamePtr();
	auto font = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular);
	UINT8 lineThickness = 3;	// Will be drawn as quads
	UINT8 borderPadding = 20;	// Empty Pixels inside the border
	UINT8 maxGlyphs = 12;						// Max number of glyphs in the column
	UINT16 memberColWidth = maxGlyphs * glyphWidth;	// Column width for party members
	std::wstring _bufStr;

	innerRect = {		// The inner rect after padding has been applied
		m_currentRect.left + borderPadding,
		m_currentRect.top + borderPadding,
		m_currentRect.right - borderPadding,
		m_currentRect.bottom - borderPadding };

	float invSlotsOriginX = innerRect.left;			// beginning of inventory slots
	float invSlotsOriginY = innerRect.top + 90.f;

		///// Begin Draw crossed lines
	m_primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(innerRect.left,	invSlotsOriginY + 60, 0),					ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right,	invSlotsOriginY + 60, 0),					ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right,	invSlotsOriginY + 60 + lineThickness, 0),	ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.left,	invSlotsOriginY + 60 + lineThickness, 0),	ColorAmber)
	);
	m_primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(innerRect.right - memberColWidth,					innerRect.top, 0),		ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right - memberColWidth + lineThickness,	innerRect.top, 0),		ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right - memberColWidth + lineThickness,	innerRect.bottom, 0),	ColorAmber),
		VertexPositionColor(XMFLOAT3(innerRect.right - memberColWidth,					innerRect.bottom, 0),	ColorAmber)
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
			m_primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(invSlotBegin, invSlotsOriginY - 10.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY + lineThickness + 20.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotBegin, invSlotsOriginY + lineThickness + 20.f, 0), ColorAmberDark)
			);
			m_primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(invSlotBegin + lineThickness, invSlotsOriginY - 10.f + lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY - 10.f + lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY + 20.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotBegin + lineThickness, invSlotsOriginY + 20.f, 0), static_cast<XMFLOAT4>(Colors::Black))
			);
			m_primitiveBatch->DrawTriangle(
				VertexPositionColor(XMFLOAT3(invSlotEnd - 11.f, invSlotsOriginY - 10.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), ColorAmberDark),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY + 1, 0), ColorAmberDark)
			);
			m_primitiveBatch->DrawTriangle(
				VertexPositionColor(XMFLOAT3(invSlotEnd - 10.f + lineThickness, invSlotsOriginY - 10.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black))
			);
		}
		if ((InventorySlots)iSlot == selectedSlot)	// Selected
		{
			m_primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(invSlotBegin, invSlotsOriginY - 10.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY + lineThickness + 20.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotBegin, invSlotsOriginY + lineThickness + 20.f, 0), ColorAmber)
			);
			m_primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(invSlotBegin + lineThickness, invSlotsOriginY - 10.f + lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY - 10.f + lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY + 20.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotBegin + lineThickness, invSlotsOriginY + 20.f, 0), static_cast<XMFLOAT4>(Colors::Black))
			);
			m_primitiveBatch->DrawTriangle(
				VertexPositionColor(XMFLOAT3(invSlotEnd -11.f, invSlotsOriginY - 10.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY + 1, 0), ColorAmber)
			);
			m_primitiveBatch->DrawTriangle(
				VertexPositionColor(XMFLOAT3(invSlotEnd - 10.f + lineThickness, invSlotsOriginY - 10.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - 10.f, 0), static_cast<XMFLOAT4>(Colors::Black)),
				VertexPositionColor(XMFLOAT3(invSlotEnd + lineThickness, invSlotsOriginY - lineThickness, 0), static_cast<XMFLOAT4>(Colors::Black))
			);
		}
		// Draw tab string
		font->DrawString(m_overlaySB.get(), _str.c_str(),
			Vector2(invSlotBegin + stringHalfSpacing, invSlotsOriginY),
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
		invSlotBegin = invSlotEnd;
		iSlot++;
	}
	// Draw line below tabs
	m_primitiveBatch->DrawQuad(
		VertexPositionColor(XMFLOAT3(invSlotsOriginX, invSlotsOriginY + 20.f, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY + 20.f, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(invSlotEnd, invSlotsOriginY + 20.f + lineThickness, 0), ColorAmber),
		VertexPositionColor(XMFLOAT3(invSlotsOriginX, invSlotsOriginY + 20.f + lineThickness, 0), ColorAmber)
	);
	///// End Draw inventory slots tabs

	///// Begin Draw Inventory Headers
	char _invhBuf[200];
	if (selectedSlot < InventorySlots::Chest)
	{
		sprintf_s(_invhBuf, 200, "%-20s  %s", "Name", "TH0    Damage    AC   Special");
	}
	else
	{
		sprintf_s(_invhBuf, 200, "%-20s   %s", "Name", "TH0   AC   Special");

	}
	font->DrawString(m_overlaySB.get(), _invhBuf,
		Vector2(invSlotsOriginX, invSlotsOriginY + 40),
		Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
	///// End Draw Inventory Headers

	///// Begin Draw Column Headers (party members)
	int xCol = innerRect.left + widthTabsArea;		// x value at start of column drawing
	int yCol = innerRect.top;						// y value of the start of drawing
	for (size_t iMember = 0; iMember < m_partySize; iMember++)
	{
		yCol = innerRect.top;
		// First draw the class icon in the center of the column
		UINT8 memberClass = MemGetMainPtr(PARTY_CLASS_START)[iMember];
		int memberLeft = 28 *(memberClass % 8);
		int memberTop = 32 *(memberClass / 8);
		RECT memberSpriteRect = { memberLeft, memberTop, memberLeft + 28, memberTop + 32 };
		m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
			mmSSTextureSize, XMFLOAT2(xCol + (memberColWidth - 28)/2, yCol), &memberSpriteRect, Colors::White, 0.f, XMFLOAT2());
		// Then draw the member name, class, race and AC
		yCol += 32 + 5;
		_bufStr = StringFromMemory(PARTY_NAME_START + (iMember * 0x09), maxGlyphs);
		_bufStr = ltrim(rtrim(_bufStr));
		font->DrawString(m_overlaySB.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
		yCol += glyphHeight + 2;
		_bufStr = NameOfClass((DeathlordClasses)memberClass);
		font->DrawString(m_overlaySB.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
		yCol += glyphHeight + 2;
		UINT8 memberRace = MemGetMainPtr(PARTY_RACE_START)[iMember];
		_bufStr = NameOfRace((DeathlordRaces)memberRace);
		font->DrawString(m_overlaySB.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
		yCol += glyphHeight + 2;
		UINT8 memberArmor = MemGetMainPtr(PARTY_ARMORCLASS_START)[iMember];
		_bufStr = L"AC " + std::to_wstring((int)10 - memberArmor);
		font->DrawString(m_overlaySB.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);

		// Now draw hands/melee/ranged readied status background, and then the text
		yCol += glyphHeight + 10;
		m_primitiveBatch->DrawQuad(
			VertexPositionColor(XMFLOAT3(xCol + 2,				yCol, 0), ColorAmber),
			VertexPositionColor(XMFLOAT3(xCol + memberColWidth - 2,	yCol, 0), ColorAmber),
			VertexPositionColor(XMFLOAT3(xCol + memberColWidth - 2,	yCol + glyphHeight + 4, 0), ColorAmber),
			VertexPositionColor(XMFLOAT3(xCol + 2,				yCol + glyphHeight + 4, 0), ColorAmber)
		);
		yCol += 2;
		UINT8 memberReady = MemGetMainPtr(PARTY_WEAP_READY_START)[iMember];
		switch (memberReady)
		{
		case 0:
			_bufStr = L"MELEE";
			break;
		case 1:
			_bufStr = L"RANGED";
			break;
		default:
			_bufStr = L"FISTS";
		}
		font->DrawString(m_overlaySB.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);

		xCol += memberColWidth;
	}
	///// End Draw Column Headers (party members)

	///// Begin Draw Column Headers (stash)
	// Here we're drawing backwards because we have the yCol pointer already set to the last row
	UINT8 _maxStashItems = invMgr->MaxItemsPerSlot();
	UINT8 _currentItems = invMgr->StashSlotCount(selectedSlot);
	xCol = innerRect.right - memberColWidth;
	// yCol is still aligned with the readied status text
	if (_currentItems == _maxStashItems)
	{
		_bufStr = L"FULL";
		font->DrawString(m_overlaySB.get(), _bufStr.c_str(),
			Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
			VColorAmber, 0.f, Vector2(0.f, 0.f), 1.f);
	}
	yCol -= glyphHeight + 10 + 2;
	_bufStr = std::to_wstring(_currentItems) + L" / " + std::to_wstring(_maxStashItems);
	font->DrawString(m_overlaySB.get(), _bufStr.c_str(),
		Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
		Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
	yCol -= glyphHeight + 2;
	_bufStr = L"STASH";
	font->DrawString(m_overlaySB.get(), _bufStr.c_str(),
		Vector2(xCol + PaddingToCenterString(maxGlyphs, _bufStr.length()), yCol),	// center the string
		Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
	// Finally draw the icon
	yCol -= spRectStash.bottom - spRectStash.top + 5;
	m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
		mmSSTextureSize, XMFLOAT2(xCol + (memberColWidth - 28*2) / 2, yCol), &spRectStash, Colors::White, 0.f, XMFLOAT2());
	///// End Draw Column Headers (stash)

	///// Begin Draw Swapping Helper Lines
	// When highlighting an item to swap, draw helper lines between
	// the 2 members swapping (or the stash), for both items that are being swapped
	// unless one of the items is empty of course
	// 
	// We need at most 2 rects which are the endpoints of the 2 helper lines
	// We already have the toRect of the member highlighted, it's highlightedRect
	EquipInteractableRect _fromRect;
	EquipInteractableRect _otherFromRect;
	if (highlightedRect.isTrash)
		goto ENDSWAPHELPERS;
	if (highlightedRect.eSlot >= InventorySlots::TOTAL)
		goto ENDSWAPHELPERS;
	InvInstance* _invI = &m_invRows.at(highlightedRect.eRow);
	if (_invI->owner == highlightedRect.eMember)	// No need to highlight, the rect is owner or equipped
		goto ENDSWAPHELPERS;
	_fromRect = RectOfOwnerOfItemInstance(*_invI);
	if (_fromRect.eItemId == EMPTY_ITEM_ID)
		goto ENDSWAPHELPERS;
	// _xPos0 is the x pos of the rects of the first party member
	int _xPos0 = innerRect.left + widthTabsArea + (memberColWidth / 2) - (spRectInvEmpty.right - spRectInvEmpty.left) / 2;
	// And the stash x pos is
	int _xPosStash = _xPos0 + memberColWidth * maxPartySize + 20;
	if (highlightedRect.eMember < m_partySize)	// give to a person
	{
		// Figure out the other person/stash that will be impacted by the move
		bool noOtherItem = false;
		InvInstance _otherInvI = ItemInstanceOfOwner(highlightedRect.eMember);
		if (_otherInvI.item == NULL)	// there's no other item
		{
			_otherFromRect = highlightedRect;
			noOtherItem = false;
		}
		else
		{
			_otherFromRect = RectOfOwnerOfItemInstance(_otherInvI);
		}
		if (_invI->owner < m_partySize)
		{
			// From a person to a person (_invI->owner to highlighted and vice versa, 2 lines)
			// First the item we clicked on
			m_primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(_fromRect.eRect.Center().x,		_fromRect.eRect.Center().y - 2, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(_otherFromRect.eRect.Center().x,	_fromRect.eRect.Center().y - 2, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(_otherFromRect.eRect.Center().x,	_fromRect.eRect.Center().y + 2, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(_fromRect.eRect.Center().x,		_fromRect.eRect.Center().y + 2, 0), ColorAmber)
			);
			// And then the item that'll be swapped
			if (!noOtherItem)
			{
				m_primitiveBatch->DrawQuad(
					VertexPositionColor(XMFLOAT3(_fromRect.eRect.Center().x, _otherFromRect.eRect.Center().y - 2, 0), ColorAmber),
					VertexPositionColor(XMFLOAT3(_otherFromRect.eRect.Center().x, _otherFromRect.eRect.Center().y - 2, 0), ColorAmber),
					VertexPositionColor(XMFLOAT3(_otherFromRect.eRect.Center().x, _otherFromRect.eRect.Center().y + 2, 0), ColorAmber),
					VertexPositionColor(XMFLOAT3(_fromRect.eRect.Center().x, _otherFromRect.eRect.Center().y + 2, 0), ColorAmber)
				);
			}
		}
		else
		{
			// exchange stash->person (stash _invI->owner to highlighted, potentially 2 lines)
			// First the person we clicked on
			m_primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(_xPosStash,						_fromRect.eRect.Center().y - 2, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(_otherFromRect.eRect.Center().x,	_fromRect.eRect.Center().y - 2, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(_otherFromRect.eRect.Center().x,	_fromRect.eRect.Center().y + 2, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(_xPosStash,						_fromRect.eRect.Center().y + 2, 0), ColorAmber)
			);
			// And then the person who is going to lose the item
			m_primitiveBatch->DrawQuad(
				VertexPositionColor(XMFLOAT3(_xPosStash,						_otherFromRect.eRect.Center().y - 2, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(_otherFromRect.eRect.Center().x,	_otherFromRect.eRect.Center().y - 2, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(_otherFromRect.eRect.Center().x,	_otherFromRect.eRect.Center().y + 2, 0), ColorAmber),
				VertexPositionColor(XMFLOAT3(_xPosStash,						_otherFromRect.eRect.Center().y + 2, 0), ColorAmber)
			);
		}
	}
	else // give to the stash
	{
		// exchange person->stash (_invI->owner to stash, only one line)
		// it won't be highlighted if the stash is full, so we're good
		m_primitiveBatch->DrawQuad(
			VertexPositionColor(XMFLOAT3(_fromRect.eRect.Center().x,	_fromRect.eRect.Center().y - 2, 0), ColorAmber),
			VertexPositionColor(XMFLOAT3(_xPosStash,					_fromRect.eRect.Center().y - 2, 0), ColorAmber),
			VertexPositionColor(XMFLOAT3(_xPosStash,					_fromRect.eRect.Center().y + 2, 0), ColorAmber),
			VertexPositionColor(XMFLOAT3(_fromRect.eRect.Center().x,	_fromRect.eRect.Center().y + 2, 0), ColorAmber)
		);
	}
ENDSWAPHELPERS:

	///// End Draw Swapping Helper Lines


	///// Begin Draw Inventory Rows
	// Recreate the rects here for the next iteration
	v_eIRects.clear();
	v_trashIRects.clear();
	m_currentItemInstance = 0;
	xCol = innerRect.left;
	yCol = invSlotsOriginY + 60 + invRowSpacing;
	for each (auto _row in m_invRows)
	{
		DrawItem(&_row, font, memberColWidth, xCol, yCol);
		yCol += glyphHeight + invRowSpacing;
		++m_currentItemInstance;
	}
	///// End Draw Inventory Rows

	Overlay::PostRender(r);

}

// Returns rect of the owner of an item instance
// (i.e. given a row, returns the selected column)
EquipInteractableRect InvOverlay::RectOfOwnerOfItemInstance(InvInstance& pItemInstance)
{
	if (pItemInstance.item == NULL)
		return EquipInteractableRect();
	std::vector<EquipInteractableRect> _vec;
	pItemInstance.owner < m_partySize ? _vec = v_eIRects : _vec = v_trashIRects;
	for each (auto _eiRect in _vec)
	{
		if ((_eiRect.eMember == pItemInstance.owner) && (_eiRect.eRow == pItemInstance.row))
			return _eiRect;
	}
	return EquipInteractableRect();
}

// Returns the item instance owned by a member
// (i.e. give a column, returns the selected row)
InvInstance InvOverlay::ItemInstanceOfOwner(UINT8 owner)
{
	for each (auto itemInstance in m_invRows)
	{
		if (itemInstance.owner == owner)
			return itemInstance;
	}
	return InvInstance();
}

void InvOverlay::DrawItem(InvInstance* pItemInstance, DirectX::SpriteFont* font,
	int memberColWidth, int xPos, int yPos)
{
	wchar_t _spBuf[200];
	// First draw the item info
	InvItem* item = pItemInstance->item;
	auto itemname = item->name;
	if (g_nonVolatile.englishNames)
		itemname = item->nameEnglish;
	if (item->slot < InventorySlots::Chest)
	{
		if (pItemInstance->charges != EMPTY_CHARGES_COUNT)
			swprintf_s(_spBuf, 200, L"%-14s (%03d)  %+d    %dx %2d-%-2d   %+d   %s",
				itemname.c_str(), pItemInstance->charges, item->thaco, item->numAttacks, item->damageMin, item->damageMax, -1 * item->ac, item->special.c_str());
		else
			swprintf_s(_spBuf, 200, L"%-20s  %+d    %dx %2d-%-2d   %+d   %s",
				itemname.c_str(), item->thaco, item->numAttacks, item->damageMin, item->damageMax, -1 * item->ac, item->special.c_str());
	}
	else
	{
		if (pItemInstance->charges != EMPTY_CHARGES_COUNT)
			swprintf_s(_spBuf, 200, L"%-14s (%03d)   %+d    %+d   %s",
				itemname.c_str(), pItemInstance->charges, item->thaco, -1 * item->ac, item->special.c_str());
		else
			swprintf_s(_spBuf, 200, L"%-20s   %+d    %+d   %s",
				itemname.c_str(), item->thaco, -1 * item->ac, item->special.c_str());
	}
	font->DrawString(m_overlaySB.get(), _spBuf,
		Vector2(xPos, yPos),
		Colors::White, 0.f, Vector2(0.f, 0.f), 1.f);
	// Finished drawing the item info. Now draw the equipped status
	// Calculate the xpos of the first equipment status sprite. It is centered in the column
	// The next ones will just be shifted by memberColWidth
	int _xPos = innerRect.left + widthTabsArea + (memberColWidth / 2) - (spRectInvEmpty.right - spRectInvEmpty.left) / 2;
	auto mmSSTextureSize = GetTextureSize(m_overlaySpriteSheet.Get());
	for (UINT8 i = 0; i < m_partySize; i++)
	{
		m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
			mmSSTextureSize, XMFLOAT2(_xPos, yPos), &spRectInvEmpty, Colors::White, 0.f, XMFLOAT2());
		RECT rectSprite = spRectInvEmpty;
		if (pItemInstance->owner == i)
		{
			if (pItemInstance->equipped)
				m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
					mmSSTextureSize, XMFLOAT2(_xPos, yPos), &spRectInvWorn, Colors::White, 0.f, XMFLOAT2());
			else
				m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
					mmSSTextureSize, XMFLOAT2(_xPos, yPos), &spRectInvCarried, Colors::White, 0.f, XMFLOAT2());
		}
		else // set up highlighting to do something with the button when the item not equipped for this party member
		{
			bool isHighlighted = true;
			isHighlighted = isHighlighted && (highlightedRect.eSlot == pItemInstance->item->slot);
			isHighlighted = isHighlighted && (highlightedRect.eRow == m_currentItemInstance);
			isHighlighted = isHighlighted && (highlightedRect.eMember == i);
			if (isHighlighted)
			{
				if (pItemInstance->item->canEquip(
					(DeathlordClasses)MemGetMainPtr(PARTY_CLASS_START)[i],
					(DeathlordRaces)MemGetMainPtr(PARTY_RACE_START)[i])
					)
					m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
						mmSSTextureSize, XMFLOAT2(_xPos, yPos), &spRectInvWorn, Colors::White, 0.f, XMFLOAT2());
				else
					m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
						mmSSTextureSize, XMFLOAT2(_xPos, yPos), &spRectInvCarried, Colors::White, 0.f, XMFLOAT2());
			}
		}
		// Tell the system we have an equipment button
		EquipInteractableRect _aEIR;
		_aEIR.eRect = { _xPos + 5, yPos, 18, 18 };	// size of the square + 1
		_aEIR.eRow = m_currentItemInstance;
		_aEIR.eItemId = pItemInstance->item->id;
		_aEIR.eSlot = pItemInstance->item->slot;
		_aEIR.eMember = i;
		_aEIR.isTrash = false;
		v_eIRects.push_back(_aEIR);
		_xPos += memberColWidth;
	}
	// Now the stash
	_xPos += memberColWidth * (maxPartySize - m_partySize);	// in case the party isn't full
	_xPos += verticalBarWidth;	// the width of the vertical bar
	m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
		mmSSTextureSize, XMFLOAT2(_xPos, yPos), &spRectInvEmpty, Colors::White, 0.f, XMFLOAT2());

	EquipInteractableRect _aEIR;
	_aEIR.eRect = { _xPos + 5, yPos, 18, 18 };	// size of the square + 1
	_aEIR.eRow = m_currentItemInstance;
	_aEIR.eItemId = pItemInstance->item->id;
	_aEIR.eSlot = pItemInstance->item->slot;
	_aEIR.eMember = maxPartySize;	// The "member" is the stash
	_aEIR.isTrash = false;
	v_eIRects.push_back(_aEIR);
	for (UINT8 i = maxPartySize; i < (maxPartySize + STASH_MAX_ITEMS_PER_SLOT); i++)
	{
		if (pItemInstance->owner == i)
		{
			m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
				mmSSTextureSize, XMFLOAT2(_xPos, yPos), &spRectInvCarried, Colors::White, 0.f, XMFLOAT2());
			_xPos += 40;
			// Here draw the open trash when highlighted, otherwise closed trash
			bool isHighlighted = true;
			isHighlighted = isHighlighted && (highlightedRect.eSlot == pItemInstance->item->slot);
			isHighlighted = isHighlighted && (highlightedRect.eRow == m_currentItemInstance);
			isHighlighted = isHighlighted && (highlightedRect.eMember == i);
			isHighlighted = isHighlighted && highlightedRect.isTrash;
			if (isHighlighted)
				m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
					mmSSTextureSize, XMFLOAT2(_xPos, yPos - 5), &spRectTrashOpen, Colors::White, 0.f, XMFLOAT2());
			else
				m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
					mmSSTextureSize, XMFLOAT2(_xPos, yPos - 5), &spRectTrashClosed, Colors::White, 0.f, XMFLOAT2());
			// And tell the system we have a trash button
			EquipInteractableRect _aTrashIR;
			_aTrashIR.eRect = { _xPos, yPos, 16, 22 };	// trash icon
			_aTrashIR.eRow = m_currentItemInstance;
			_aTrashIR.eItemId = pItemInstance->item->id;
			_aTrashIR.eSlot = pItemInstance->item->slot;
			_aTrashIR.eMember = i;
			_aTrashIR.isTrash = true;
			v_trashIRects.push_back(_aTrashIR);
			break;	// Need to break here because the stash is unique in that it's one "member" with multiple inventory instances
		}
		else
		{
				// highlight the equipment button on the stash when the equipment isn't in the stash
			bool isHighlighted = true;
			isHighlighted = isHighlighted && (highlightedRect.eSlot == pItemInstance->item->slot);
			isHighlighted = isHighlighted && (highlightedRect.eRow == m_currentItemInstance);
			isHighlighted = isHighlighted && (highlightedRect.eMember == maxPartySize);
			if (isHighlighted)
			{
				m_overlaySB->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::InvOverlaySpriteSheet),
					mmSSTextureSize, XMFLOAT2(_xPos, yPos), &spRectInvCarried, Colors::White, 0.f, XMFLOAT2());
			}
		}
	}

}
#pragma endregion
