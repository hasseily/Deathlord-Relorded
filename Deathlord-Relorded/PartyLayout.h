#pragma once
#include "DeviceResources.h"
#include "Descriptors.h"
#include <array>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;

// Where to put the party on the main screen
constexpr array<UINT16, 6> PARTY_LAYOUT_X = { 10, 10, 10, 450, 450, 450 };
constexpr array<UINT16, 6> PARTY_LAYOUT_Y = { 550, 700, 850, 550, 700, 850 };
// Portrait size
constexpr UINT16 PARTY_PORTRAIT_WIDTH = 185;
constexpr UINT16 PARTY_PORTRAIT_HEIGHT = 242;

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

	void PartyLayout::RenderMember(UINT8 member, UINT16 originX, UINT16 originY);

	UINT8 m_currentLeader;

	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_partyLayoutSpriteSheet;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_portraitsMale;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_portraitsFemale;
};

