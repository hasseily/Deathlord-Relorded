#pragma once
#include "DeviceResources.h"
#include "Descriptors.h"
#include "DeathlordHacks.h"
#include <array>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;

// Where to put the party on the main screen
constexpr array<UINT16, 6> PARTY_LAYOUT_X = { 17, 17, 17, 1570, 1570, 1570 };
constexpr array<UINT16, 6> PARTY_LAYOUT_Y = { 386, 606, 826, 386, 606, 826 };
constexpr UINT16 PARTY_LAYOUT_WIDTH = 320;
constexpr UINT16 PARTY_LAYOUT_HEIGHT = 198;

// Portrait size
constexpr UINT16 PARTY_PORTRAIT_WIDTH = 92;
constexpr UINT16 PARTY_PORTRAIT_HEIGHT = 121;

class PartyLayout
{
public:
	// Methods
	// Call Update(leader) to update the party layout
	// Returns true if the leader has changed
	bool Update(UINT8 leader);

	// Call Render() in the rendering stage to draw
	// r is the rectangle of the game itself
	void Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch);
	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload);
	void OnDeviceLost();
	UINT8 GetPartySize();
	void LevelUpIncremented(UINT8 member);
	void AttributeIncreased(UINT8 member, DeathlordAttributes attr);

	// Properties

	// public singleton code
	static PartyLayout* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new PartyLayout(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static PartyLayout* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~PartyLayout()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();

	static PartyLayout* s_instance;
	PartyLayout(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}

	void RenderMember(UINT8 member, DirectX::SpriteBatch* spriteBatch, UINT16 originX, UINT16 originY);
	void RenderMemberTopLayer(UINT8 member, DirectX::SpriteBatch* spriteBatch, UINT16 originX, UINT16 originY);

	UINT8 m_currentLeader;

	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_bgLayerTop;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_partyLayoutSpriteSheet;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_portraitsMale;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_portraitsFemale;
};

