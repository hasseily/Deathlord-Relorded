#include "Renderer.h"

#include "AppleStyle.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <GL/gl.h>
#elif defined(__APPLE__)
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

// GL/gl.h on Windows is locked to OpenGL 1.1 — these constants come in at
// 1.2 / 1.3 and aren't auto-loaded yet (glad arrives in Phase 6).
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>

namespace dlrl
{

Renderer::Renderer() = default;

Renderer::~Renderer()
{
    Shutdown();
}

bool Renderer::Init(const char* title, int width, int height)
{
    // GL 4.1 Core — what the vendored postprocessor's GLSL targets
    // (#version 410). 4.1 is the highest macOS supports, so portable.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    m_window = SDL_CreateWindow(title, width, height,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!m_window)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    m_glctx = SDL_GL_CreateContext(m_window);
    if (!m_glctx)
    {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        return false;
    }

    SDL_GL_MakeCurrent(m_window, m_glctx);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // imgui.ini is set externally by main.cpp (under SDL_GetPrefPath) so
    // window layout / docking persists across sessions. nullptr disables.
    ImGui::StyleColorsDark();
    // Initial pass uses the default (Green). main.cpp reapplies after
    // LoadSettings reads the persisted choice, but doing it here too
    // means we get the font + sharp borders for the very first frame
    // (and for any code path that renders before LoadSettings runs).
    dlrl::ApplyAppleStyle(dlrl::InterfaceColor_Green);
    ImGui_ImplSDL3_InitForOpenGL(m_window, m_glctx);
    ImGui_ImplOpenGL3_Init("#version 410");

    glGenTextures(1, &m_framebufferTex);
    glBindTexture(GL_TEXTURE_2D, m_framebufferTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void Renderer::Shutdown()
{
    if (m_glctx)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (m_framebufferTex)
    {
        glDeleteTextures(1, &m_framebufferTex);
        m_framebufferTex = 0;
    }
    if (m_glctx)
    {
        SDL_GL_DestroyContext(m_glctx);
        m_glctx = nullptr;
    }
    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void Renderer::BeginFrame()
{
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    glViewport(0, 0, w, h);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::UploadFramebuffer(const void* bgra, int w, int h)
{
    if (!bgra || w <= 0 || h <= 0) return;

    glBindTexture(GL_TEXTURE_2D, m_framebufferTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    if (w != m_texWidth || h != m_texHeight)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                     GL_BGRA_EXT, GL_UNSIGNED_BYTE, bgra);
        m_texWidth  = w;
        m_texHeight = h;
    }
    else
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                        GL_BGRA_EXT, GL_UNSIGNED_BYTE, bgra);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::BeginImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Renderer::EndImGui()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Renderer::EndFrame()
{
    SDL_GL_SwapWindow(m_window);
}

bool Renderer::CaptureBackbufferBmp(const char* path)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    if (width <= 0 || height <= 0) return false;

    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    glFinish();
    glReadPixels(0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels.data());

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    const uint32_t pixelBytes = static_cast<uint32_t>(pixels.size());
    auto put16 = [&](uint16_t value) {
        const char bytes[] = { static_cast<char>(value), static_cast<char>(value >> 8) };
        out.write(bytes, sizeof(bytes));
    };
    auto put32 = [&](uint32_t value) {
        const char bytes[] = {
            static_cast<char>(value), static_cast<char>(value >> 8),
            static_cast<char>(value >> 16), static_cast<char>(value >> 24)
        };
        out.write(bytes, sizeof(bytes));
    };

    out.write("BM", 2);
    put32(14 + 40 + pixelBytes);
    put16(0);
    put16(0);
    put32(14 + 40);
    put32(40);
    put32(static_cast<uint32_t>(width));
    put32(static_cast<uint32_t>(height));
    put16(1);
    put16(32);
    put32(0);
    put32(pixelBytes);
    put32(2835);
    put32(2835);
    put32(0);
    put32(0);
    out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    return out.good();
}

} // namespace dlrl
