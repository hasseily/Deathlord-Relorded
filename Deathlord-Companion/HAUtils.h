#pragma once
#include <string>
#include <DirectXMath.h>
#include <vector>

namespace HA
{
    struct Vertex
    {
        DirectX::XMFLOAT4 position;
        DirectX::XMFLOAT2 texcoord;
    };

	std::vector<uint8_t> LoadBGRAImage(const wchar_t* filename, uint32_t& width, uint32_t& height);

    size_t ConvertWStrToStr(const std::wstring* wstr, std::string* str);
	size_t ConvertStrToWStr(const std::string* str, std::wstring* wstr);

    void ConvertStrToUpperAscii(std::string& str);
    void ConvertUpperAsciiToStr(std::string& str);

    std::wstring GetLastErrorAsString();
    bool AlertIfError(HWND parentWindow);

    bool checkMenuItemByPosition(HMENU m, int pos);

    // Check if a point is within the bounds of a convex clockwise polygon
    // The polygon is defined by a series of points (XMFLOAT2)
    bool isPointInConvexPolygon(DirectX::XMFLOAT2 aPoint, DirectX::XMFLOAT2* aClockwisePolygon, UINT8 aPolygonSides);
}

