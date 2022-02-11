#pragma once

#include "Overlay.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

class BattleOverlay	: public Overlay
{
public:
	void SetActiveActor(UINT8 actorNumber);
	void BattleEnemyHPIsSet();	// call this from PC_BATTLE_ENEMY_HP_SET to grab the MEM_ENEMY_HP_START
	void SpriteBeginAttack(UINT8 charPosition);
	void SpriteDodge(UINT8 charPosition);
	void SpriteIsHit(UINT8 charPosition, UINT8 damage);
	void SpriteIsHealed(UINT8 charPosition, UINT16 healingAmount);
	void SpriteDied(UINT8 charPosition);

	void Update();
	void Render(SimpleMath::Rectangle r);

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


protected:
	void Initialize();

private:
	void BattleSetEnemyMaxHP();

	UINT8 m_monsterId;	// ID of the monster being fought in the main monster spritesheet
	UINT8 m_activeActor;	// Currently active actor (Below 6, it's a party member. 6 or above, it's a monster)

	static BattleOverlay* s_instance;

	BattleOverlay(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		Initialize();
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
	}

};
