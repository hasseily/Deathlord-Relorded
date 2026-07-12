#include "SpriteFont.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>

namespace dlrl
{
namespace
{

std::uint32_t ReadU32(const std::vector<std::uint8_t>& data, std::size_t& cursor)
{
    if (cursor + 4 > data.size()) return 0;
    std::uint32_t value = 0;
    std::memcpy(&value, data.data() + cursor, sizeof(value));
    cursor += 4;
    return value;
}

float ReadFloat(const std::vector<std::uint8_t>& data, std::size_t& cursor)
{
    const std::uint32_t bits = ReadU32(data, cursor);
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::array<std::uint8_t, 4> Color565(std::uint16_t value)
{
    return {
        static_cast<std::uint8_t>(((value >> 11) & 31) * 255 / 31),
        static_cast<std::uint8_t>(((value >> 5) & 63) * 255 / 63),
        static_cast<std::uint8_t>((value & 31) * 255 / 31),
        255
    };
}

bool DecodeBc2(const std::uint8_t* blocks, std::size_t size,
               std::uint32_t width, std::uint32_t height,
               std::uint32_t stride, std::uint32_t rows,
               std::vector<std::uint8_t>& rgba)
{
    if (!blocks || size < static_cast<std::size_t>(stride) * rows
        || stride % 16 != 0)
        return false;
    rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
    const std::uint32_t blocksWide = stride / 16;
    for (std::uint32_t blockY = 0; blockY < rows; ++blockY)
    {
        for (std::uint32_t blockX = 0; blockX < blocksWide; ++blockX)
        {
            const std::uint8_t* block = blocks
                + static_cast<std::size_t>(blockY) * stride + blockX * 16;
            std::uint64_t alpha = 0;
            std::uint16_t color0 = 0;
            std::uint16_t color1 = 0;
            std::uint32_t indices = 0;
            std::memcpy(&alpha, block, 8);
            std::memcpy(&color0, block + 8, 2);
            std::memcpy(&color1, block + 10, 2);
            std::memcpy(&indices, block + 12, 4);
            std::array<std::array<std::uint8_t, 4>, 4> colors{};
            colors[0] = Color565(color0);
            colors[1] = Color565(color1);
            for (int channel = 0; channel < 3; ++channel)
            {
                colors[2][channel] = static_cast<std::uint8_t>(
                    (2 * colors[0][channel] + colors[1][channel]) / 3);
                colors[3][channel] = static_cast<std::uint8_t>(
                    (colors[0][channel] + 2 * colors[1][channel]) / 3);
            }
            colors[2][3] = colors[3][3] = 255;
            for (std::uint32_t pixelY = 0; pixelY < 4; ++pixelY)
            {
                for (std::uint32_t pixelX = 0; pixelX < 4; ++pixelX)
                {
                    const std::uint32_t pixel = pixelY * 4 + pixelX;
                    const std::uint32_t x = blockX * 4 + pixelX;
                    const std::uint32_t y = blockY * 4 + pixelY;
                    if (x >= width || y >= height) continue;
                    const auto& color = colors[(indices >> (pixel * 2)) & 3];
                    const std::size_t destination =
                        (static_cast<std::size_t>(y) * width + x) * 4;
                    rgba[destination] = color[0];
                    rgba[destination + 1] = color[1];
                    rgba[destination + 2] = color[2];
                    rgba[destination + 3] = static_cast<std::uint8_t>(
                        ((alpha >> (pixel * 4)) & 15) * 17);
                }
            }
        }
    }
    return true;
}

} // namespace

bool SpriteFont::Load(const std::filesystem::path& path)
{
    Reset();
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(stream)), {});
    if (data.size() < 12 || std::memcmp(data.data(), "DXTKfont", 8) != 0) return false;
    std::size_t cursor = 8;
    const std::uint32_t glyphCount = ReadU32(data, cursor);
    if (glyphCount == 0 || cursor + static_cast<std::size_t>(glyphCount) * 32 > data.size())
        return false;
    glyphs_.reserve(glyphCount);
    for (std::uint32_t index = 0; index < glyphCount; ++index)
    {
        Glyph glyph;
        glyph.character = ReadU32(data, cursor);
        glyph.left = ReadU32(data, cursor);
        glyph.top = ReadU32(data, cursor);
        glyph.right = ReadU32(data, cursor);
        glyph.bottom = ReadU32(data, cursor);
        glyph.xOffset = ReadFloat(data, cursor);
        glyph.yOffset = ReadFloat(data, cursor);
        glyph.xAdvance = ReadFloat(data, cursor);
        glyphs_.push_back(glyph);
    }
    lineSpacing_ = ReadFloat(data, cursor);
    (void)ReadU32(data, cursor); // Default character; v2's font has none.
    const std::uint32_t width = ReadU32(data, cursor);
    const std::uint32_t height = ReadU32(data, cursor);
    const std::uint32_t format = ReadU32(data, cursor);
    const std::uint32_t stride = ReadU32(data, cursor);
    const std::uint32_t rows = ReadU32(data, cursor);
    const std::size_t textureBytes = static_cast<std::size_t>(stride) * rows;
    // MakeSpriteFont stores this asset as DXGI_FORMAT_BC2_UNORM (74).
    if (format != 74 || cursor + textureBytes > data.size()) return false;
    std::vector<std::uint8_t> pixels;
    if (!DecodeBc2(data.data() + cursor, textureBytes, width, height,
                   stride, rows, pixels))
        return false;
    return atlas_.Upload(pixels.data(), static_cast<int>(width),
                         static_cast<int>(height), false);
}

void SpriteFont::Reset()
{
    atlas_.Reset();
    glyphs_.clear();
    lineSpacing_ = 0;
}

const SpriteFont::Glyph* SpriteFont::Find(std::uint32_t character) const
{
    const auto found = std::lower_bound(glyphs_.begin(), glyphs_.end(), character,
        [](const Glyph& glyph, std::uint32_t value)
        {
            return glyph.character < value;
        });
    return found != glyphs_.end() && found->character == character ? &*found : nullptr;
}

} // namespace dlrl
