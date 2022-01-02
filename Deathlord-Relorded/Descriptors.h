#pragma once


enum class TextureDescriptors
{
	MainBackground,
    Apple2Video,
	AutoMapBackground,
	AutoMapBackgroundGrass,
	AutoMapTileSheet,
	AutoMapAvatar,
	AutoMapTileHidden,
	InvOverlaySpriteSheet,
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


