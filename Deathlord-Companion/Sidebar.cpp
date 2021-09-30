#include "pch.h"
#include "Sidebar.h"

static BlockStruct m_defaultBlock;

Sidebar::Sidebar(UINT8 _id, SidebarTypes _type, int _width, int _height, UINT8 _maxBlocks, DirectX::XMFLOAT2 _position)
{
	id			= _id;
	type		= _type;
	width		= _width;
	height		= _height;
	maxBlocks	= std::min(_maxBlocks, SIDEBAR_MAX_BLOCKS);
	position.x		= _position.x + SIDEBAR_OUTSIDE_MARGIN;
	position.y		= _position.y + SIDEBAR_OUTSIDE_MARGIN;

	// Determine each block's origin point. They all flow downwards from the sidebar's origin
	int blockHeight = ((height - (2 * SIDEBAR_OUTSIDE_MARGIN)) / maxBlocks) - (2 * SIDEBAR_BLOCK_PADDING);
	float hoffset = position.y + SIDEBAR_OUTSIDE_MARGIN;	// starting height offset
	for (UINT8 i = 0; i < maxBlocks; i++)
	{
		auto b = std::make_shared <BlockStruct>();
		b->position.x = position.x + SIDEBAR_BLOCK_PADDING;
		hoffset += SIDEBAR_BLOCK_PADDING;
		b->position.y = hoffset;
		hoffset += blockHeight + SIDEBAR_BLOCK_PADDING;
		blocks.push_back(b);
	}
}

void Sidebar::Clear()
{
	for each (auto b in blocks)
	{
		b->text = m_defaultBlock.text;
	}
}

SidebarError Sidebar::SetBlock(BlockStruct bS, UINT8 _id)
{
	try
	{
		auto b = blocks.at(_id);
		b->type = bS.type;
		b->color = bS.color;
		b->fontId = bS.fontId;
		b->text = bS.text;
	}
	catch (std::out_of_range const& exc)
	{
		char buf[sizeof(exc.what()) + 300];
		sprintf_s(buf, "Block %d doesn't exist: %s\n", _id, exc.what());
		OutputDebugStringA(buf);
		return SidebarError::ERR_OUT_OF_RANGE;
	}

	return SidebarError::ERR_NONE;
}

SidebarError Sidebar::SetBlockText(std::string str, UINT8 _id)
{
	try
	{
		auto b = blocks.at(_id);
		b->text = str;
	}
	catch (std::out_of_range const& exc)
	{
		char buf[sizeof(exc.what()) + 300];
		sprintf_s(buf, "Block %d doesn't exist: %s\n", _id, exc.what());
		OutputDebugStringA(buf);
		return SidebarError::ERR_OUT_OF_RANGE;
	}

	return SidebarError::ERR_NONE;
}
