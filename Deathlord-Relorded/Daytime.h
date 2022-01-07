#pragma once
#include "DeviceResources.h"
#include "Descriptors.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;

constexpr UINT16 MEM_DAY_HOUR = 0xFC00;
constexpr UINT16 MEM_DAY_MINUTE = 0xFC01;
constexpr UINT16 MEM_DAY_OF_MONTH = 0xFC02;
constexpr UINT8 DAYTIME_DAYS_PER_MOONPHASE = 4;
constexpr UINT8 DAYTIME_SPRITE_WIDTH = 28;
constexpr UINT8 DAYTIME_SPRITE_HEIGHT = 32;
constexpr UINT8 DAYTIME_HAND_TOP_X = 0;
constexpr UINT8 DAYTIME_HAND_TOP_Y = 64;
constexpr UINT8 DAYTIME_HAND_WIDTH = 28;
constexpr UINT8 DAYTIME_HAND_HEIGHT = 75;
constexpr UINT8 DAYTIME_HAND_ORIGIN_X = 14;	// Relative origin to rotate the hand around
constexpr UINT8 DAYTIME_HAND_ORIGIN_Y = 16;

enum class MoonPhases	// based on day of month
{
	Waxing25 = 0,
	Waxing50,
	Waxing75,
	Full,
	Waning75,
	Waning50,
	Waning25,
	count
};

class Daytime	// Singleton
{
public:
	// Methods
	// Call Render() at the end of the rendering stage to draw the daytime stuff
	// r is the rectangle of the game itself
	void Render(SimpleMath::Rectangle r, DirectX::SpriteBatch* spriteBatch);
	void CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload);
	void OnDeviceLost();

	// Properties

	// public singleton code
	static Daytime* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new Daytime(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static Daytime* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~Daytime()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();

	static Daytime* s_instance;
	Daytime(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_daytimeSpriteSheet;
};

