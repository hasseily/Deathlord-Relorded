#pragma once

#include "DeviceResources.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

class BattleOverlay	// Singleton
{
public:
	void Render(SimpleMath::Rectangle r);
	void ShowOverlay();
	void HideOverlay();
	void ToggleOverlay();
	bool IsOverlayDisplayed();
	void Update();
	void SetActiveActor(UINT8 actorNumber);
	void BattleEnemyHPIsSet();	// call this from PC_BATTLE_ENEMY_HP_SET to grab the MEM_ENEMY_HP_START
	void SpriteBeginAttack(UINT8 charPosition);
	void SpriteDodge(UINT8 charPosition);
	void SpriteIsHit(UINT8 charPosition, UINT8 damage);
	void SpriteDied(UINT8 charPosition);
	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload, CommonStates* states);
	void OnDeviceLost();

	// public singleton code
	static BattleOverlay* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new BattleOverlay(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static BattleOverlay* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~BattleOverlay()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();
	void BattleSetEnemyMaxHP();

	static BattleOverlay* s_instance;
	bool bShouldDisplay;
	bool bIsDisplayed;
	RECT m_currentRect;	// Rect of the overlay
	UINT8 m_monsterId;	// ID of the monster being fought in the main monster spritesheet
	UINT8 m_activeActor;	// Currently active actor (Below 6, it's a party member. 6 or above, it's a monster)

	BattleOverlay(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_overlaySpriteSheet;
	// It's totally independent, and uses its own DTX pieces
	std::unique_ptr<SpriteBatch>m_spriteBatch;
	std::unique_ptr<PrimitiveBatch<VertexPositionColor>>m_primitiveBatch;
	std::unique_ptr<BasicEffect> m_dxtEffect;
};
