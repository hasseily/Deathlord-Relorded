#pragma once


enum class TextureDescriptors
{
    Apple2Video,
	AutoMapBackground,
	AutoMapTileSheet,
	AutoMapAvatar,
	AutoMapTileHidden,
	FontA2Regular,
	FontA2Bold,
	FontA2Italic,
	FontA2BoldItalic,
	Count
};

enum class FontDescriptors
{
	FontA2Regular = (int)TextureDescriptors::FontA2Regular,
	FontA2Bold = (int)TextureDescriptors::FontA2Bold,
	FontA2Italic = (int)TextureDescriptors::FontA2Italic,
	FontA2BoldItalic = (int)TextureDescriptors::FontA2BoldItalic,
    Count
};


