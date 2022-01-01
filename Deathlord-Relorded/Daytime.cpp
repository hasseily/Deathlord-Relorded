#include "pch.h"
#include "Emulator/Memory.h"
#include "Daytime.h"
#include <array>

// below because "The declaration of a static data member in its class definition is not a definition"
Daytime* Daytime::s_instance;

void Daytime::Initialize()
{

}

void Daytime::Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch)
{
	auto mmTexSize = GetTextureSize(m_daytimeSpriteSheet.Get());
	float _daytimeScale = 2.0f;

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
	UINT8 _minuteDigit = _minute & 0b1111;
	RECT digitRect = RECT();
	digitRect.top = DAYTIME_SPRITE_HEIGHT;	// digits are the second line
	digitRect.bottom = digitRect.top + DAYTIME_SPRITE_HEIGHT;
	RECT digitDestRect = RECT();
	digitDestRect.top = r.y + 155;
	digitDestRect.bottom = digitDestRect.top + DAYTIME_SPRITE_HEIGHT * _daytimeScale;
	digitDestRect.left = r.x + 10;	// starting point

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
				digitDestRect, &digitRect, Colors::Blue, 0.f, XMFLOAT2());
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
			digitDestRect, &digitRect, Colors::Blue, 0.f, XMFLOAT2());
	// digit minute ten
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

	// Draw the sun position
	// First calculate the height of the sun, 12:00 being zenith
	int _timeInMinutes = _hour * 60 + _minTen + 10 + _minuteDigit;
	float _sunriseRatio = 0.f;
	// Calculate the sun visibility. It only shows between 6 and 12 (and 6 PM)
	// 7 AM is like 5 PM
	if ((_timeInMinutes >= 6 * 60) && _isAM)
		_sunriseRatio = (_timeInMinutes - (6.f * 60.f)) / (6.f * 60.f);
	else if ((_timeInMinutes <= 18 * 60) && (!_isAM))
		_sunriseRatio = ((18.f * 60.f) - _timeInMinutes) / (6.f * 60.f);
	RECT sunSpriteRect = RECT();
	sunSpriteRect.left = (int)MoonPhases::count * DAYTIME_SPRITE_WIDTH;
	sunSpriteRect.right = sunSpriteRect.left + DAYTIME_SPRITE_WIDTH;
	sunSpriteRect.top = 0;
	sunSpriteRect.bottom = sunSpriteRect.top + DAYTIME_SPRITE_HEIGHT * abs(_sunriseRatio);
	RECT sunDestRect = RECT();
	if (_isAM)
		sunDestRect.left = digitDestRect.left + 4 + DAYTIME_SPRITE_WIDTH * _daytimeScale + _sunriseRatio * 15;
	else
		sunDestRect.left = digitDestRect.left + 4 + DAYTIME_SPRITE_WIDTH * _daytimeScale + 15 + (1 - _sunriseRatio) * 15;
	sunDestRect.right = sunDestRect.left + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	sunDestRect.bottom = digitDestRect.top + DAYTIME_SPRITE_HEIGHT * _daytimeScale;
	sunDestRect.top = sunDestRect.bottom - (sunSpriteRect.bottom - sunSpriteRect.top) * _daytimeScale;
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
		sunDestRect, &sunSpriteRect, Colors::White, 0.f, XMFLOAT2());

	// Draw phase of moon
	// There are 7 phases, each spanning 4 days from 0x00 to 0x1B
	UINT8 _day = MemGetMainPtr(MEM_DAY_OF_MONTH)[0];
	RECT moonSpriteRect = RECT();
	moonSpriteRect.left = DAYTIME_SPRITE_WIDTH * (UINT8)(_day / DAYTIME_DAYS_PER_MOONPHASE);
	moonSpriteRect.right = moonSpriteRect.left + DAYTIME_SPRITE_WIDTH;
	moonSpriteRect.top = 0;
	moonSpriteRect.bottom = moonSpriteRect.top + DAYTIME_SPRITE_HEIGHT;
	RECT moonDestRect = RECT();
	moonDestRect.left = r.x + 300;
	moonDestRect.right = moonDestRect.left + DAYTIME_SPRITE_WIDTH * _daytimeScale;
	moonDestRect.top = r.y + 155;
	moonDestRect.bottom = moonDestRect.top + DAYTIME_SPRITE_HEIGHT * _daytimeScale;
	spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::DayTimeSpriteSheet), mmTexSize,
		moonDestRect, &moonSpriteRect, Colors::White, 0.f, XMFLOAT2());
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