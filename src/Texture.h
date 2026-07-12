#pragma once

#include <filesystem>
#include <cstdint>
#include <vector>

namespace dlrl
{

class Texture
{
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool Load(const std::filesystem::path& path, bool nearest = false,
              bool keepPixels = false);
    bool Upload(const std::uint8_t* rgba, int width, int height, bool nearest = false);
    void Reset();

    unsigned int Id() const { return id_; }
    int Width() const { return width_; }
    int Height() const { return height_; }
    bool IsValid() const { return id_ != 0; }
    const std::vector<std::uint8_t>& Pixels() const { return pixels_; }

private:
    unsigned int id_ = 0;
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> pixels_;
};

} // namespace dlrl
