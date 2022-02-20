#pragma once
#include "Animation.h"
#include <vector>

using namespace DirectX;

class AnimationSprite : public Animation
{
public:
	void SetTotalAnimationLength(float seconds);
	void SetSpriteSheetOrigin(Vector2 origin);
	void Update() override { };
	void Render(SpriteBatch* spriteBatch, RECT* overlayRect = nullptr);
	AnimationSprite(DescriptorHeap* resourceDescriptors, XMUINT2 spriteSheetSize,
		Vector2 spriteSheetOrigin, int width, int height, int frameCount = 1);
	~AnimationSprite() {};

	TextureDescriptors m_textureDescriptor;
	int m_width;
	int m_height;
	float m_scale;
private:
	Vector2 m_spriteSheetOrigin;
};