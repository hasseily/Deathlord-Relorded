#pragma once

#include <cstdint>

#include <SDL3/SDL_video.h>

namespace dlrl
{

// Owns the SDL window, the OpenGL context, and the framebuffer texture(s).
// Phase 4 only opens the window and clears it; Phase 5 wires the Apple //e
// framebuffer (BGRA 32bpp) into a GL texture and blits it.
class Renderer
{
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Init(const char* title, int width, int height);
    void Shutdown();

    void BeginFrame();
    void UploadFramebuffer(const void* bgra, int w, int h);
    void BeginImGui();
    void EndImGui();
    void EndFrame();
    bool CaptureBackbufferBmp(const char* path);

    // Expose the framebuffer texture so an ImGui window can draw it via
    // ImGui::Image. Texture is BGRA stored bottom-up — flip V at draw
    // time with uv0=(0,1) uv1=(1,0).
    unsigned int FramebufferTexId() const  { return m_framebufferTex; }
    int          FramebufferWidth() const  { return m_texWidth; }
    int          FramebufferHeight() const { return m_texHeight; }

    SDL_Window*   Window() const    { return m_window; }
    SDL_GLContext GLContext() const { return m_glctx; }

private:
    SDL_Window*   m_window         = nullptr;
    SDL_GLContext m_glctx          = nullptr;
    unsigned int  m_framebufferTex = 0;
    int           m_texWidth       = 0;
    int           m_texHeight      = 0;
};

} // namespace dlrl
