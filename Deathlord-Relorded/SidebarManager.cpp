#include "pch.h"
#include "SidebarManager.h"
#include <vector>
#include <DirectXPackedVector.h>
#include <DirectXMath.h>
#include "Emulator/AppleWin.h"
#include "Sidebar.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

// Default frame
int m_baseFrameWidth = 0;
int m_baseFrameHeight = 0;

RECT m_clientRect;
RECT m_gameLinkRect;

static std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, nullptr, 0);
    auto* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}

SidebarManager::SidebarManager()
{
    fontsAvailable[FontDescriptors::FontA2Regular]      = L"a2-12pt.spritefont";
	fontsAvailable[FontDescriptors::FontA2Bold]         = L"a2-12pt-bold.spritefont";
	fontsAvailable[FontDescriptors::FontA2Italic]       = L"a2-12pt-italic.spritefont";
	fontsAvailable[FontDescriptors::FontA2BoldItalic]   = L"a2-12pt-bolditalic.spritefont";
	fontsAvailable[FontDescriptors::FontPR3Regular]      = L"pr3-dlcharset-12pt.spritefont";
	fontsAvailable[FontDescriptors::FontPR3Inverse]      = L"pr3-dlcharset-12pt-inverse.spritefont";
	fontsAvailable[FontDescriptors::FontDLRegular]       = L"dlfont_12pt.spritefont";
	fontsAvailable[FontDescriptors::FontDLInverse]       = L"dlfont_12pt_inverse.spritefont";
    sidebars = {};
    SetBaseSize(GetFrameBufferWidth(), GetFrameBufferHeight());
}

SidebarError SidebarManager::CreateSidebar(SidebarTypes type, UINT8 numBlocks, UINT16 size, __out UINT8* id)
{
    *id = 0;
    // the last block id being 0-based, it's one less than activeBlocks
    auto newId = (UINT8)sidebars.size();
    if (newId >= SIDEBARS_MAX_COUNT)
    {
        return SidebarError::ERR_OUT_OF_RANGE;
    }

    int w, h, bw, bh;
    DirectX::XMFLOAT2 position = { 0.F, 0.f };

    GetBaseSize(bw, bh);

    switch (type)
    {
    case SidebarTypes::Right:
    {
        if (size == 0)
            w = SIDEBAR_LR_WIDTH;
        else
            w = size;
        h = SIDEBAR_LR_MAX_HEIGHT;
        position.x = (float)bw;
        position.y = 0.f;
        bw = bw + w;
        break;
    }
    case SidebarTypes::Bottom:
    {
        w = SIDEBAR_TB_MAX_WIDTH;
        if (size == 0)
            h = SIDEBAR_TB_HEIGHT;
        else
            h = size;
        position.x = 0.f;
        position.y = (float)bh;
        bh = bh + h;
        break;
    }
    default:
        return SidebarError::ERR_UNKNOWN;
        break;
    }

    sidebars.emplace_back(newId, type, w, h, numBlocks, position);
    *id = newId;

    SetBaseSize(bw, bh);
    return SidebarError::ERR_NONE;
}

SidebarError SidebarManager::ClearSidebar(UINT8 id)
{
    try
    {
        sidebars.at(id).Clear();
        return SidebarError::ERR_NONE;
    }
    catch (std::out_of_range const& exc)
    {
        char buf[sizeof(exc.what()) + 500];
        snprintf(buf, 500, "Sidebar %d doesn't exist: %s\n", id, exc.what());
        SidebarExceptionHandler(buf);
        //OutputDebugStringA(buf);
        return SidebarError::ERR_OUT_OF_RANGE;
    }
}

void SidebarManager::ClearAllSidebars()
{
    for each (auto sb in sidebars)
    {
        sb.Clear();
    }
}

void SidebarManager::DeleteAllSidebars()
{
    sidebars.clear();
    SetBaseSize(GetFrameBufferWidth(), GetFrameBufferHeight());
}

// Properties
void SidebarManager::GetBaseSize(__out int& width, __out int& height) noexcept
{
    width = m_baseFrameWidth;
    height = m_baseFrameHeight;
}

void SidebarManager::SetBaseSize(const int width, const int height) noexcept
{
    m_baseFrameWidth = width;
    m_baseFrameHeight = height;
}
