#include "pch.h"
#include "TextOutput.h"
#include "DeathlordHacks.h"
#include "Game.h"	// TODO: This is for the spritefonts. These need to be put somewhere else

extern std::unique_ptr<Game>* GetGamePtr();	// TODO: Same for this

static UINT8 m_previousX = 0;
static UINT8 m_previousY = 0;

void TextOutput::Initialize()
{
	fontsAvailable[FontDescriptors::FontA2Regular] = L"a2-12pt.spritefont";
	fontsAvailable[FontDescriptors::FontA2Bold] = L"a2-12pt-bold.spritefont";
	fontsAvailable[FontDescriptors::FontA2Italic] = L"a2-12pt-italic.spritefont";
	fontsAvailable[FontDescriptors::FontA2BoldItalic] = L"a2-12pt-bolditalic.spritefont";
	fontsAvailable[FontDescriptors::FontDLRegular] = L"pr3_dlcharset_12pt.spritefont";
	fontsAvailable[FontDescriptors::FontDLInverse] = L"pr3_dlcharset_12pt_inverse.spritefont";
	// create all the lines for the billboard
	UINT8 billboardLineCt = PRINT_CHAR_Y_BOTTOM - PRINT_CHAR_Y_BILLBOARD_BEGIN;
	for (UINT8 i = 0; i < billboardLineCt; i++)
	{
		m_vBillboard.push_back(pair(wstring(), FontDescriptors::FontDLRegular));
	}
	m_strModule = wstring();
	m_strKeypress = wstring();
	v_linesToPrint = vector<pair<wstring, XMFLOAT2>>();
}

void TextOutput::Render(DirectX::SpriteBatch* spriteBatch)
{
	auto gamePtr = GetGamePtr();
	auto fontsRegular = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular);
	(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch, m_strModule.c_str(),
		{ 1100.f, 800.f }, Colors::White, 0.f, Vector2(), 1.f);

	// TODO
	// Render v_linesToPrint, Module, Keypress, Billboard and Log
}

#pragma region utilities

void TextOutput::PrintWStringAtOrigin(std::wstring wstr, XMFLOAT2 origin)
{
	v_linesToPrint.push_back(std::pair(wstr, origin));
}

#pragma endregion

#pragma region Deathlord printing

wchar_t TextOutput::ConvertChar(unsigned char ch)
{
	return ARRAY_DEATHLORD_CHARSET[ch & 0x7F];
}

void TextOutput::PrintCharRaw(unsigned char ch, UINT8 X_Origin, UINT8 Y_Origin, UINT8 X, UINT8 Y, bool bInverse)
{
	// Disregard anything in the upper area of the screen
	// We don't print the party list
	if (Y < PRINT_Y_MIN)
		return;

	// Print to the specified area on screen
	if (X_Origin == PRINT_CHAR_X_ORIGIN_LEFT)
		PrintCharToLog(ch, X, bInverse);
	else if (X_Origin == PRINT_CHAR_X_ORIGIN_RIGHT)
	{
		if (Y == PRINT_CHAR_Y_CENTER)
			PrintCharToModule(ch, X, bInverse);
		else if (Y == PRINT_CHAR_Y_BOTTOM)
			PrintCharToKeypress(ch, X, bInverse);
		else
		{
			// Here Deathlord prints to the billboard area
			// But during battle it uses it as a bigger log print area
			// Unless the user wants to choose a spell. In which case
			// it uses it as a list selection area
			PrintCharToBillboard(ch, X, Y, bInverse);
		}
	}
}

void TextOutput::PrintCharToModule(unsigned char ch, UINT8 X, bool bInverse)
{
	if (X < m_XModule)	// reset
		m_strModule.clear();
	m_strModule.append(1, ConvertChar(ch));
	m_XModule = X;
}

void TextOutput::PrintCharToKeypress(unsigned char ch, UINT8 X, bool bInverse)
{
	if (X < m_XKeypress)
		m_strKeypress.clear();
	m_strKeypress.append(1, ConvertChar(ch));
	m_XKeypress = X;
}

void TextOutput::PrintCharToBillboard(unsigned char ch, UINT8 X, UINT8 Y, bool bInverse)
{
	// The billboard uses multiple lines
	// If the code wants to write a previous line, it means it is redrawing the billboard
	if (Y < m_YBillboard)
	{
		// Clear the billboard
		UINT8 billboardLineCt = PRINT_CHAR_Y_BOTTOM - PRINT_CHAR_Y_BILLBOARD_BEGIN;
		for (UINT8 i = 0; i < billboardLineCt; i++)
		{
			m_vBillboard.at(i).first.clear();
			m_vBillboard.at(i).second = FontDescriptors::FontDLRegular;
		}
	}
	m_vBillboard.at(Y - PRINT_CHAR_Y_BILLBOARD_BEGIN).first.append(1, ConvertChar(ch));
	if (bInverse)
		m_vBillboard.at(Y - PRINT_CHAR_Y_BILLBOARD_BEGIN).second = FontDescriptors::FontDLInverse;
	else
		m_vBillboard.at(Y - PRINT_CHAR_Y_BILLBOARD_BEGIN).second = FontDescriptors::FontDLRegular;
	m_YBillboard = Y;
}

void TextOutput::PrintCharToLog(unsigned char ch, UINT8 X, bool bInverse)
{
	if (X < m_XLog)		// it's a new line
	{
		if (m_vLog.size() > PRINT_MAX_LOG_LINES)
		{
			// log is full, erase the last line
			m_vLog.pop_back();
			if (bInverse)
				m_vLog.insert(m_vLog.begin(), pair(wstring(), FontDescriptors::FontDLInverse));
			else
				m_vLog.insert(m_vLog.begin(), pair(wstring(), FontDescriptors::FontDLRegular));
		}
	}
	m_vLog.at(0).first.append(1, ConvertChar(ch));
	m_XLog = X;
}

#pragma endregion