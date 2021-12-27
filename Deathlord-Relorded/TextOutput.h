#pragma once
#include "DeviceResources.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

class TextOutput	// Singleton
{
	// public singleton code
	static TextOutput* GetInstance(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		if (NULL == s_instance)
			s_instance = new TextOutput(deviceResources, resourceDescriptors);
		return s_instance;
	}
	static TextOutput* GetInstance()
	{
		if (NULL == s_instance)
			_ASSERT(0);
		return s_instance;
	}
	~TextOutput()
	{
		m_resourceDescriptors = NULL;
		m_deviceResources = NULL;
	}
private:
	void Initialize();
	void PrintCharRaw(UINT8 X_Origin, UINT8 Y_Origin, UINT8 X, UINT8 Y);
	void PrintChar();
	void PrintString();

	static TextOutput* s_instance;
	RECT m_currentRect;	// Rect of the overlay

	TextOutput(std::unique_ptr<DX::DeviceResources>& deviceResources,
		std::unique_ptr<DirectX::DescriptorHeap>& resourceDescriptors)
	{
		m_deviceResources = deviceResources.get();
		m_resourceDescriptors = resourceDescriptors.get();
		Initialize();
	}
	DX::DeviceResources* m_deviceResources;
	DescriptorHeap* m_resourceDescriptors;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_TextOutputSpriteSheet;
};

