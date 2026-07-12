#pragma once

#include "Texture.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace dlrl
{

class SpriteFont
{
public:
    struct Glyph
    {
        std::uint32_t character = 0;
        std::uint32_t left = 0;
        std::uint32_t top = 0;
        std::uint32_t right = 0;
        std::uint32_t bottom = 0;
        float xOffset = 0;
        float xAdvance = 0;
        float yOffset = 0;
    };

    bool Load(const std::filesystem::path& path);
    void Reset();
    const Glyph* Find(std::uint32_t character) const;
    const Texture& Atlas() const { return atlas_; }
    float LineSpacing() const { return lineSpacing_; }

private:
    Texture atlas_;
    std::vector<Glyph> glyphs_;
    float lineSpacing_ = 0;
};

} // namespace dlrl
