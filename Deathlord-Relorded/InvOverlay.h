#pragma once

#include "DeviceResources.h"
#include <array>
#include <string>
#include "InvItem.h"

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

class InvOverlay	// Singleton
{
public:
	void Render(SimpleMath::Rectangle r);
	void ShowOverlay();
	void HideOverlay();
	void ToggleOverlay();
	bool IsOverlayDisplayed();
	void UpdateState();
	void LeftMouseButtonClicked(int x, int y);
	void MousePosInPixels(int x, int y);

	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload, CommonStates* states);
	void OnDeviceLost();

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
	~InvOverlay()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();
	void DrawItem(InvInstance* pItemInstance, DirectX::SpriteFont* font, int memberColWidth, int xPos, int yPos);
	EquipInteractableRect RectOfOwnerOfItemInstance(InvInstance& pItemInstance);
	InvInstance ItemInstanceOfOwner(UINT8 owner);

	static InvOverlay* s_instance;
	bool bShouldDisplay;
	bool bIsDisplayed;
	RECT m_currentRect;	// Rect of the overlay
	std::vector<EquipInteractableRect>v_eIRects;		// equipment interactable buttons
	std::vector<EquipInteractableRect>v_trashIRects;	// equipment trash buttons
	UINT8 m_currentItemInstance;	// Row in use

	InvOverlay(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_invOverlaySpriteSheet;
	// It's totally independent, and uses its own DTX pieces
	std::unique_ptr<SpriteBatch>m_spriteBatch;
	std::unique_ptr<PrimitiveBatch<VertexPositionColor>>m_primitiveBatch;
	std::unique_ptr<BasicEffect> m_dxtEffect;
};
