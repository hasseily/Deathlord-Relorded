#include "pch.h"
#include "HAUtils.h"

#include <wincodec.h>
#include "ReadData.h"
#include "FindMedia.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace HA
{

    std::vector<uint8_t> LoadBGRAImage(const wchar_t* filename, uint32_t& width, uint32_t& height)
    {
        ComPtr<IWICImagingFactory> wicFactory;
        DX::ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory)));

        ComPtr<IWICBitmapDecoder> decoder;
        DX::ThrowIfFailed(wicFactory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf()));

        ComPtr<IWICBitmapFrameDecode> frame;
        DX::ThrowIfFailed(decoder->GetFrame(0, frame.GetAddressOf()));

        DX::ThrowIfFailed(frame->GetSize(&width, &height));

        WICPixelFormatGUID pixelFormat;
        DX::ThrowIfFailed(frame->GetPixelFormat(&pixelFormat));

        uint32_t rowPitch = width * sizeof(uint32_t);
        uint32_t imageSize = rowPitch * height;

        std::vector<uint8_t> image;
        image.resize(size_t(imageSize));

        if (memcmp(&pixelFormat, &GUID_WICPixelFormat32bppBGRA, sizeof(GUID)) == 0)
        {
            DX::ThrowIfFailed(frame->CopyPixels(nullptr, rowPitch, imageSize, reinterpret_cast<BYTE*>(image.data())));
        }
        else
        {
            ComPtr<IWICFormatConverter> formatConverter;
            DX::ThrowIfFailed(wicFactory->CreateFormatConverter(formatConverter.GetAddressOf()));

            BOOL canConvert = FALSE;
            DX::ThrowIfFailed(formatConverter->CanConvert(pixelFormat, GUID_WICPixelFormat32bppBGRA, &canConvert));
            if (!canConvert)
            {
                throw std::exception("CanConvert");
            }

            DX::ThrowIfFailed(formatConverter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut));

            DX::ThrowIfFailed(formatConverter->CopyPixels(nullptr, rowPitch, imageSize, reinterpret_cast<BYTE*>(image.data())));
        }

        return image;
    }


    size_t ConvertWStrToStr(const std::wstring* wstr, std::string* str)
    {
        constexpr int MAX_WSTR_CONVERSION_LENGTH = 4096;
        size_t maxLength = wstr->length();
        if (maxLength > MAX_WSTR_CONVERSION_LENGTH)
            maxLength = MAX_WSTR_CONVERSION_LENGTH;
		size_t numConverted = 0;
        char szStr[MAX_WSTR_CONVERSION_LENGTH] = "";
		wcstombs_s(&numConverted, szStr, wstr->c_str(), maxLength);
        str->assign(szStr);
        return numConverted;
    }

	size_t ConvertStrToWStr(const std::string* str, std::wstring* wstr)
	{
		size_t maxLength = strlen(str->c_str());
		size_t numConverted = 0;
		wchar_t* wzStr = new wchar_t[maxLength+1];
		mbstowcs_s(&numConverted, wzStr, maxLength + 1, str->c_str(), maxLength);
		wstr->assign(wzStr);
        delete[] wzStr;
		return numConverted;
	}

	/* convert string to upper ascii Apple 2 */
	void ConvertStrToUpperAscii(std::string& str)
	{
		for (std::string::iterator i = str.begin(); i != str.end(); i++) {
			*i = *i + 0x80;   /* shift to upper ascii */
		}
	}

	/* convert upper ascii Apple 2 to str */
	void ConvertUpperAsciiToStr(std::string& str)
	{
		for (std::string::iterator i = str.begin(); i != str.end(); i++) {
			*i = *i - 0x80;   /* shift from upper ascii */
		}
	}

	//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
	std::wstring GetLastErrorAsString()
	{
		//Get the error message ID, if any.
		DWORD errorMessageID = ::GetLastError();
		if (errorMessageID == 0) {
			return std::wstring(); //No error message has been recorded
		}

		LPWSTR messageBuffer = nullptr;
		size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);

		std::wstring message(messageBuffer, size);
		LocalFree(messageBuffer);
		return message;
	}

    //Shows an alert if GetLastError() exists. Returns false if no error.
    bool AlertIfError(HWND parentWindow)
    {
		std::wstring serr = HA::GetLastErrorAsString();
        if (serr == L"")
            return false;
		serr.insert(0, L"Error:\n");
		MessageBox(parentWindow, serr.c_str(), L"Alert", MB_ICONASTERISK | MB_OK);
        return true;
    }

    // Unchecks all menu items and only checks the one position
    bool checkMenuItemByPosition(HMENU m, int pos)
    {
		int i = 0;
		while (CheckMenuItem(m, i,
			MF_BYPOSITION | (i == pos ? MF_CHECKED : MF_UNCHECKED))
			!= -1)
		{
			++m;
		}
        return false;
    }

	bool isPointInConvexPolygon(DirectX::XMFLOAT2 aPoint, DirectX::XMFLOAT2* aClockwisePolygon, UINT8 aPolygonSides)
	{
		float d;
		for (size_t i = 0; i < aPolygonSides; i++)
		{
			DirectX::XMFLOAT2 p1 = aClockwisePolygon[i % aPolygonSides];
			DirectX::XMFLOAT2 p2 = aClockwisePolygon[(i + 1) % aPolygonSides];
			d = (p2.x - p1.x) * (aPoint.y - p1.y) - (aPoint.x - p1.x) * (p2.y - p1.y);
			if (d > 0)
				return false;
		}
		return true;
	}

}