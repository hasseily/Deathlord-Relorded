#pragma once
#include <Windows.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <vector>

#pragma warning( disable:4324 )
// C4324: structure was padded due to alignment specifier
// used in all XMVECTOR

constexpr UINT8 SIDEBAR_MAX_BLOCKS = 100;
constexpr UINT8 SIDEBAR_OUTSIDE_MARGIN = 3; // margin around the sidebar not including block padding
constexpr UINT8 SIDEBAR_BLOCK_PADDING = 2;   // PADDING around each block

enum class SidebarError
{
	ERR_NONE			= 0,
	ERR_OUT_OF_RANGE	= 254,
	ERR_UNKNOWN			= 254,
};

enum class BlockType
{
	Header,
	Content,
	Empty,
	Count
};


/// <summary>
/// A sidebar handles a series of blocks.
/// It is given an origin point, and calculates each block's origin point, and manages the textual
/// data of each block. A block height is the sidebar height / number of blocks.
/// The sidebar is assigned width, height and number of blocks by the Sidebar Manager.
/// </summary>

enum class FontDescriptors
{
	A2FontRegular,
	A2FontBold,
	A2FontItalic,
	A2FontBoldItalic,
	Count
};

// TODO: See if we need a flag to determine if it should redraw
// TODO: Transform everywhere every point, color or vector2 into XMVECTOR?

struct BlockStruct
{
	DirectX::XMFLOAT2 position = {0.f, 0.f};
	BlockType type = BlockType::Empty;
	DirectX::XMVECTOR color = DirectX::Colors::GhostWhite;
	FontDescriptors fontId = FontDescriptors::A2FontRegular;
	std::string text = "";
};

enum class SidebarTypes
{
	Right,
	Bottom,
	Count
};

class Sidebar
{
public:
	int id;
	SidebarTypes type;
	int width;
	int height;
	UINT8 maxBlocks;
	DirectX::XMFLOAT2 position;
	std::vector< std::shared_ptr<BlockStruct> > blocks;

	Sidebar(UINT8 id, SidebarTypes type, int width, int height, UINT8 maxBlocks, DirectX::XMFLOAT2 position);

	// Clear the text from all the blocks
	void Clear();

	SidebarError SetBlock(BlockStruct bS, UINT8 id);

	SidebarError SetBlockText(std::string str, UINT8 id);
};

