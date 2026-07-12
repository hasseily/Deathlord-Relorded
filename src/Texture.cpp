#include "Texture.h"

#include "glad/glad.h"
#include "stb_image.h"

#include <cstdio>

namespace dlrl
{

Texture::~Texture()
{
    Reset();
}

bool Texture::Load(const std::filesystem::path& path, bool nearest, bool keepPixels)
{
    Reset();
    int channels = 0;
    unsigned char* pixels = stbi_load(path.string().c_str(), &width_, &height_, &channels, 4);
    if (!pixels)
    {
        std::fprintf(stderr, "Unable to load texture %s: %s\n",
                     path.string().c_str(), stbi_failure_reason());
        width_ = height_ = 0;
        return false;
    }

    if (keepPixels)
        pixels_.assign(pixels, pixels + static_cast<std::size_t>(width_) * height_ * 4);
    Upload(pixels, width_, height_, nearest);
    stbi_image_free(pixels);
    return true;
}

bool Texture::Upload(const std::uint8_t* rgba, int width, int height, bool nearest)
{
    if (!rgba || width <= 0 || height <= 0) return false;
    if (!id_) glGenTextures(1, &id_);
    width_ = width;
    height_ = height;
    glBindTexture(GL_TEXTURE_2D, id_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void Texture::Reset()
{
    if (id_) glDeleteTextures(1, &id_);
    id_ = 0;
    width_ = height_ = 0;
    pixels_.clear();
}

} // namespace dlrl
