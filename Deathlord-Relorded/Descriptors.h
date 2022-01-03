#pragma once

using namespace DirectX;

constexpr XMFLOAT4 ColorAmber = { 0.5f, 0.2f, 0.f, 1.000000000f };
constexpr XMVECTORF32 VColorAmber = { { { 0.5f, 0.2f, 0.f, 1.000000000f } } };
constexpr XMFLOAT4 ColorAmberDark = { 0.25f, 0.1f, 0.f, 1.000000000f };
constexpr XMVECTORF32 VColorAmberDark = { { { 0.25f, 0.1f, 0.f, 1.000000000f } } };
constexpr XMFLOAT4 ColorBlack = { 0.f, 0.f, 0.f, 1.000000000f };
constexpr XMVECTORF32 VColorBlack = { { { 0.f, 0.f, 0.f, 1.000000000f } } };

enum class TextureDescriptors
{
	MainBackground,
    Apple2Video,
	Apple2Video2,
	AutoMapBackgroundTransition,
	AutoMapTileSheet,
	AutoMapAvatar,
	AutoMapTileHidden,
	InvOverlaySpriteSheet,
	MiniMapSpriteSheet,
	DayTimeSpriteSheet,
	FontA2Regular,
	FontA2Bold,
	FontA2Italic,
	FontA2BoldItalic,
	FontDLRegular,
	FontDLInverse,
	Count
};

enum class FontDescriptors
{
	FontA2Regular = (int)TextureDescriptors::FontA2Regular,
	FontA2Bold = (int)TextureDescriptors::FontA2Bold,
	FontA2Italic = (int)TextureDescriptors::FontA2Italic,
	FontA2BoldItalic = (int)TextureDescriptors::FontA2BoldItalic,
	FontDLRegular = (int)TextureDescriptors::FontDLRegular,
	FontDLInverse = (int)TextureDescriptors::FontDLInverse,
    Count
};


