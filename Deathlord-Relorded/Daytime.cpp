#include "pch.h"
#include "Emulator/Memory.h"
#include "Daytime.h"
#include <array>

// below because "The declaration of a static data member in its class definition is not a definition"
Daytime* Daytime::s_instance;

void Daytime::Initialize()
{

}

float Daytime::TimeOfDayInFloat()
{
	UINT8 _hour = MemGetMainPtr(MEM_DAY_HOUR)[0];
	UINT8 _minute = MemGetMainPtr(MEM_DAY_MINUTE)[0];	// In BCD format!!!
	UINT8 _minTen = _minute >> 4;
	UINT8 _minDigit = _minute & 0b1111;
	return (_hour + (_minTen * 10 + _minDigit) / 60.f);
}

void Daytime::Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch)
{
	auto mmTexSize = GetTextureSize(m_daytimeSpriteSheet.Get());
	float _daytimeScale = 0.25f;

	// Draw the time
	bool _isAM = true;
	UINT8 _hour = MemGetMainPtr(MEM_DAY_HOUR)[0];
	if (_hour > 12)
		_isAM = false;
	UINT8 _hourTen = (_hour % 12) / 10;
	UINT _hourDigit = (_hour % 12) - _hourTen * 10;
	if (_hour == 0 || _hour == 12)	// Hours 00AM, 00PM are changed to 12AM and 12PM
	{
		_hourTen = 1;
		_hourDigit = 2;
	}
	UINT8 _minute = MemGetMainPtr(MEM_DAY_MINUTE)[0];	// In BCD format!!!
	UINT8 _minTen = _minute >> 4;
	UINT8 _minDigit = _minute & 0b1111;

	// The whole chunk below is to display the time digits
	RECT digitRect = RECT();
	digitRect.top = DAYTIME_SPRITE_HEIGHT;	// digits are the second line (except for separator)
	digitRect.bottom = digitRect.top + DAYTIME_SPRITE_HEIGHT;
	RECT digitDestRect = RECT();
	digitDestRect.top = r.y + 210;
	digitDestRect.bottom = digitDestRect.top + DAYTIME_SPRITE_HEIGHT * _daytimeScale;
	digitDestRect.left = r.x + 151;	// starting point

	// digit hour ten
	if (_hourTen > 0)	// don't display 0
	{
		digitRect.left = _hourTen * DAYTIME_SPRITE_WIDTH;
		digitRect.right = digitRect.left + DAYTIME_SPRITE_WIDTH;
		digitDestRect.right = digitDestRect.left + DAYTIME_SPRITE_WIDTH * _daytimeScale;
		if ((_hour >= 6) && (_hour < 18))
			spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
				digitDestRect, &digitRect, Colors::Orange, 0.f, XMFLOAT2());
		else
			spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
				digitDestRect, &digitRect, Colors::Black, 0.f, XMFLOAT2());
	}
	// digit hour
	digitRect.left = _hourDigit * DAYTIME_SPRITE_WIDTH;
	digitRect.right = digitRect.left + DAYTIME_SPRITE_WIDTH;
	digitDestRect.left += 4 + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	digitDestRect.right = digitDestRect.left + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	if ((_hour >= 6) && (_hour < 18))
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
			digitDestRect, &digitRect, Colors::Orange, 0.f, XMFLOAT2());
	else
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
			digitDestRect, &digitRect, Colors::Black, 0.f, XMFLOAT2());
	// hour/minute colon separator
	digitRect.top = 0;
	digitRect.bottom = digitRect.top + DAYTIME_SPRITE_HEIGHT;
	digitRect.left = 9 * DAYTIME_SPRITE_WIDTH;
	digitRect.right = digitRect.left + DAYTIME_SPRITE_WIDTH;
	digitDestRect.left += 4 + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	digitDestRect.right = digitDestRect.left + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	if ((_hour >= 6) && (_hour < 18))
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
			digitDestRect, &digitRect, Colors::Orange, 0.f, XMFLOAT2());
	else
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
			digitDestRect, &digitRect, Colors::Black, 0.f, XMFLOAT2());
	// digit minute ten
	digitRect.top = DAYTIME_SPRITE_HEIGHT;
	digitRect.bottom = digitRect.top + DAYTIME_SPRITE_HEIGHT;
	digitRect.left = _minTen * DAYTIME_SPRITE_WIDTH;
	digitRect.right = digitRect.left + DAYTIME_SPRITE_WIDTH;
	digitDestRect.left += 4 + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	digitDestRect.right = digitDestRect.left + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	if ((_hour >= 6) && (_hour < 18))
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
			digitDestRect, &digitRect, Colors::Orange, 0.f, XMFLOAT2());
	else
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
			digitDestRect, &digitRect, Colors::Blue, 0.f, XMFLOAT2());
	// digit minute
	// only display 0 as the original, because minutes are only updated every 4 or so.
	digitRect.left = 0 * DAYTIME_SPRITE_WIDTH;
	digitRect.right = digitRect.left + DAYTIME_SPRITE_WIDTH;
	digitDestRect.left += 4 + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	digitDestRect.right = digitDestRect.left + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	if ((_hour >= 6) && (_hour < 18))
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
			digitDestRect, &digitRect, Colors::Orange, 0.f, XMFLOAT2());
	else
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
			digitDestRect, &digitRect, Colors::Blue, 0.f, XMFLOAT2());

#if 0
	// Draw the sun position
	// First calculate the height of the sun, 12:00 being zenith
	UINT8 _visibleSunHeight = 0;	// Height in pixels of the visible sun
	// Calculate the sun visibility. It only shows between 6 and 12 (and 6 PM)
	// 7 AM is like 5 PM
	if (_isAM && _hour >= 6)
		_visibleSunHeight = DAYTIME_SPRITE_HEIGHT * (_hour - 6) / 6;
	else if ((!_isAM) && _hour <= 18)
		_visibleSunHeight = DAYTIME_SPRITE_HEIGHT * (18 - _hour) / 6;
	RECT sunSpriteRect = RECT();
	sunSpriteRect.left = (int)MoonPhases::count * DAYTIME_SPRITE_WIDTH;
	sunSpriteRect.right = sunSpriteRect.left + DAYTIME_SPRITE_WIDTH;
	sunSpriteRect.top = 0;
	sunSpriteRect.bottom = sunSpriteRect.top + _visibleSunHeight;
	RECT sunDestRect = RECT();
	sunDestRect.left = digitDestRect.left + 4 + DAYTIME_SPRITE_WIDTH * _daytimeScale + _hour * 5;
	sunDestRect.right = sunDestRect.left + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	sunDestRect.bottom = digitDestRect.top + DAYTIME_SPRITE_HEIGHT * _daytimeScale;
	sunDestRect.top = sunDestRect.bottom - (sunSpriteRect.bottom - sunSpriteRect.top) * _daytimeScale;
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
		sunDestRect, &sunSpriteRect, Colors::White, 0.f, XMFLOAT2());
#endif

	// Draw phase of moon
	// There are 7 phases, each spanning 4 days from 0x00 to 0x1B
	UINT8 _day = MemGetMainPtr(MEM_DAY_OF_MONTH)[0];
	RECT moonSpriteRect = RECT();
	moonSpriteRect.left = DAYTIME_SPRITE_WIDTH * (UINT8)(_day / DAYTIME_DAYS_PER_MOONPHASE);
	moonSpriteRect.right = moonSpriteRect.left + DAYTIME_SPRITE_WIDTH;
	moonSpriteRect.top = 0;
	moonSpriteRect.bottom = moonSpriteRect.top + DAYTIME_SPRITE_HEIGHT;
	RECT moonDestRect = RECT();
	moonDestRect.left = r.x + 162;
	moonDestRect.right = moonDestRect.left + DAYTIME_SPRITE_WIDTH;
	moonDestRect.top = r.y + 260;
	moonDestRect.bottom = moonDestRect.top + DAYTIME_SPRITE_HEIGHT;
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
		moonDestRect, &moonSpriteRect, Colors::White, 0.f, XMFLOAT2());

	// Draw the clock hand. It is originally pointing at 0:00 (down)
	UINT16 dayMinutes = (_hour * 60) + (_minTen * 10) + _minDigit;
	RECT handsSourceRect = RECT();
	handsSourceRect.left = DAYTIME_HAND_TOP_X;
	handsSourceRect.top = DAYTIME_HAND_TOP_Y;
	handsSourceRect.right = DAYTIME_HAND_TOP_X + DAYTIME_HAND_WIDTH;
	handsSourceRect.bottom = DAYTIME_HAND_TOP_Y + DAYTIME_HAND_HEIGHT;
	XMFLOAT2 clockCenter = XMFLOAT2(r.x + 177, r.y + 244);
	// Rotation is in radians so it's 2*pi*ratio
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
		clockCenter, &handsSourceRect, Colors::White, 3.141593 * 2 * dayMinutes / 1440,
		XMFLOAT2(DAYTIME_HAND_ORIGIN_X, DAYTIME_HAND_ORIGIN_Y));

}

#pragma region D3D stuff
void Daytime::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Tileset_Relorded_MoonPhases.png",
			m_daytimeSpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_daytimeSpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::DayTimeSpriteSheet));
}

void Daytime::OnDeviceLost()
{
	m_daytimeSpriteSheet.Reset();
}

#pragma endregion