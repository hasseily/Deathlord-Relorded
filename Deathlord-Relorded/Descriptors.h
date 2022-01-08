#pragma once

using namespace DirectX;

constexpr XMFLOAT4 ColorText = { 1.f, 1.f, 1.f, 0.600000000f };
constexpr XMVECTORF32 VColorText = { { { 1.f, 1.f, 1.f, 0.600000000f } } };
constexpr XMFLOAT4 ColorAmber = { 0.5f, 0.2f, 0.f, 1.000000000f };
constexpr XMVECTORF32 VColorAmber = { { { 0.5f, 0.2f, 0.f, 1.000000000f } } };
constexpr XMFLOAT4 ColorAmberDark = { 0.25f, 0.1f, 0.f, 1.000000000f };
constexpr XMVECTORF32 VColorAmberDark = { { { 0.25f, 0.1f, 0.f, 1.000000000f } } };
constexpr XMFLOAT4 ColorBlack = { 0.f, 0.f, 0.f, 1.000000000f };
constexpr XMVECTORF32 VColorBlack = { { { 0.f, 0.f, 0.f, 1.000000000f } } };
constexpr XMFLOAT4 ColorCurtain = { 0.f, 0.f, 0.f, 0.8f };
constexpr XMVECTORF32 VColorCurtain = { { { 0.f, 0.f, 0.f, 0.8f } } };

enum class TextureDescriptors
{
	MainBackground,
	Apple2Video,
	AutoMapBackgroundTransition,
	AutoMapOverlandTiles,
	AutoMapDungeonTiles,
	AutoMapElementsTiles,
	AutoMapMonsterSpriteSheet,
	AutoMapHiddenSpriteSheet,
	InvOverlaySpriteSheet,
	MiniMapSpriteSheet,
	DayTimeSpriteSheet,
	PartyLayoutSpriteSheet,
	PortraitsMale,
	PortraitsFemale,
	FontA2Regular,
	FontA2Bold,
	FontA2Italic,
	FontA2BoldItalic,
	FontPR3Regular,
	FontPR3Inverse,
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
	FontPR3Regular = (int)TextureDescriptors::FontPR3Regular,		// PR3 Font 12pt in Deathlord Charset
	FontPR3Inverse = (int)TextureDescriptors::FontPR3Inverse,		// PR3 Font 12pt inverse in Deathlord Charset
	FontDLRegular = (int)TextureDescriptors::FontDLRegular,			// Original Deathlord font (Deathlord Charset)
	FontDLInverse = (int)TextureDescriptors::FontDLInverse,			// Original inverse Deathlord font (Deathlord Charset)
    Count
};


