#pragma once
#include "AnimationSprite.h"
#include <vector>

using namespace DirectX;

class AnimationSpriteRotating : public AnimationSprite
{
public:
	void Render(SpriteBatch* spriteBatch, RECT* overlayRect = nullptr);
	AnimationSpriteRotating(DescriptorHeap* resourceDescriptors, XMUINT2 spriteSheetSize,
		Vector2 spriteSheetOrigin, int width, int height, float rotationsPerSecond = 1.f);
	~AnimationSpriteRotating() {};

	float m_rotation;
	float m_rotationsPerSecond;
	int m_maxRotations;
	XMFLOAT2 m_center;
	XMVECTORF32 m_tint;
private:
	UINT64 m_startTick;
};