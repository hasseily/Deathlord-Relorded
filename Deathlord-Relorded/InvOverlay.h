#pragma once

#include "Overlay.h"
#include "InvItem.h"
#include <array>
#include <string>

using namespace DirectX;
using namespace DirectX::SimpleMath;

struct EquipInteractableRect
{
	SimpleMath::Rectangle eRect = SimpleMath::Rectangle();
	InventorySlots eSlot = InventorySlots::TOTAL;
	UINT8 eRow = 0xFF;
	UINT8 eItemId = EMPTY_ITEM_ID;
	UINT8 eMember = 0xFF;
	bool isTrash = false;
};

class InvOverlay : public Overlay	// Singleton
{
public:
	void LeftMouseButtonClicked(int x, int y);
	void MousePosInPixels(int x, int y);

	void Update();
	void Render(SimpleMath::Rectangle r);

	// public singleton code
	static InvOverlay* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new InvOverlay(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static InvOverlay* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}

protected:
	void Initialize();

private:

	void DrawItem(InvInstance* pItemInstance, DirectX::SpriteFont* font, int memberColWidth, int xPos, int yPos);
	EquipInteractableRect RectOfOwnerOfItemInstance(InvInstance& pItemInstance);
	InvInstance ItemInstanceOfOwner(UINT8 owner);

	std::vector<EquipInteractableRect>v_eIRects;		// equipment interactable buttons
	std::vector<EquipInteractableRect>v_trashIRects;	// equipment trash buttons
	UINT8 m_currentItemInstance;	// Row in use

	static InvOverlay* s_instance;

	InvOverlay(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		Initialize();
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
	}

};
