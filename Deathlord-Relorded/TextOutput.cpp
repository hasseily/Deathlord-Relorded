#include "pch.h"
#include "TextOutput.h"
#include "DeathlordHacks.h"
#include "Game.h"	// TODO: This is for the spritefonts. These need to be put somewhere else

extern std::unique_ptr<Game>* GetGamePtr();	// TODO: Same for this

// below because "The declaration of a static data member in its class definition is not a definition"
TextOutput* TextOutput::s_instance;

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
	// and a single line for the log, and initialize the rest
	UINT8 billboardLineCt = PRINT_CHAR_Y_KEYPRESS - PRINT_CHAR_Y_BILLBOARD_BEGIN;
	for (UINT8 i = 0; i < billboardLineCt; i++)
	{
		m_vBillboard.push_back(pair(wstring(), FontDescriptors::FontDLRegular));
	}
	m_vLog.push_back(pair(wstring(), FontDescriptors::FontDLRegular));
	m_strModule = wstring();
	m_strModule.append(PRINT_CHAR_X_MODULE_LENGTH, ' ');	// Intialize to a specific length
	m_strKeypress = wstring();
	v_linesToPrint = vector<pair<wstring, XMFLOAT2>>();
}

void TextOutput::Render(DirectX::SpriteBatch* spriteBatch)
{
	// TODO: Should not rely on the gamePtr for fonts?
	// TODO: Render v_linesToPrint
	auto gamePtr = GetGamePtr();
	auto fontsRegular = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular);

	// Render Module string
	(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular)->DrawString(spriteBatch, m_strModule.c_str(),
		{ 930.f, 200.f }, Colors::White, 0.f, Vector2(), 1.f);
	// Render Keypress string
	(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular)->DrawString(spriteBatch, m_strKeypress.c_str(),
		{ 930.f, 100.f }, Colors::White, 0.f, Vector2(), 1.f);
	// Render Billboard
	int yInc = 0;
	for each (auto bbLine in m_vBillboard)
	{
		(*gamePtr)->GetSpriteFontAtIndex(bbLine.second)->DrawString(spriteBatch, bbLine.first.c_str(),
			{ 930.f, 400.f + yInc }, Colors::White, 0.f, Vector2(), 1.f);
		yInc -= 16;
	}
	yInc = 0;
	for each (auto logLine in m_vLog)
	{
		(*gamePtr)->GetSpriteFontAtIndex(logLine.second)->DrawString(spriteBatch, logLine.first.c_str(),
			{ 1100.f, 450.f + yInc }, Colors::White, 0.f, Vector2(), 1.f);
		yInc -= 16;
	}

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
	// To use a regular charset, we need to convert it:
	// return ARRAY_DEATHLORD_CHARSET[ch & 0x7F];
	// But when using the Deathlord charset in a spritefont, no need
	// Just remove the 'end of string' marker
	return (ch & 0x7F);
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
		if (Y == PRINT_CHAR_Y_MODULE)
			PrintCharToModule(ch, X, bInverse);
		else if (Y == PRINT_CHAR_Y_KEYPRESS)
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
	// The module string is updated on a char by char basis
	// Replace a single character at a time
	m_strModule.replace(X - PRINT_CHAR_X_MODULE_BEGIN, 1, 1, ConvertChar(ch));
	m_XModule = X;
}

void TextOutput::PrintCharToKeypress(unsigned char ch, UINT8 X, bool bInverse)
{
	// The module string is updated on a char by char basis
	// Replace a single character at a time
	m_strKeypress.replace(X - PRINT_CHAR_X_KEYPRESS_BEGIN, 1, 1, ConvertChar(ch));
	m_XKeypress = X;
}

void TextOutput::PrintCharToBillboard(unsigned char ch, UINT8 X, UINT8 Y, bool bInverse)
{
	// The billboard uses multiple lines
	// It behaves differently if in combat than not
	// If in combat, it behaves like the log.
	// If not in combat, it draws to each line directly
	UINT8 billboardLineCt = PRINT_CHAR_Y_BILLBOARD_END - PRINT_CHAR_Y_BILLBOARD_BEGIN + 1;
	if (Y < m_YBillboard)
	{
		// If the code wants to write a previous line, it means it is redrawing the billboard
		// TODO: NOT ALWAYS!!! Can't rely on this, need to find where the code clears it
		// Clear the billboard
		for (UINT8 i = 0; i < billboardLineCt; i++)
		{
			m_vBillboard.at(i).first = wstring(PRINT_CHAR_X_BILLBOARD_LENGTH, ' ');
			m_vBillboard.at(i).second = FontDescriptors::FontDLRegular;
		}
	}
	if ((Y == m_YBillboard)
		&& (Y == PRINT_CHAR_Y_BILLBOARD_END)
		&& (X < m_XBillboard))
	{
		// The billboard is behaving like a log, always updating the bottom row
		// This happens when in battle. Behave like a log
		// Remove the top line and insert a new line at the bottom
		m_vBillboard.pop_back();
		if (bInverse)
			m_vBillboard.insert(m_vBillboard.begin(), pair(wstring(PRINT_CHAR_X_BILLBOARD_LENGTH, ' '), FontDescriptors::FontDLRegular));
		else
			m_vBillboard.insert(m_vBillboard.begin(), pair(wstring(PRINT_CHAR_X_BILLBOARD_LENGTH, ' '), FontDescriptors::FontDLInverse));
		m_vBillboard.at(0).first.replace(X - PRINT_CHAR_X_BILLBOARD_BEGIN, 1, 1, ConvertChar(ch));
	}
	else
	{
		// Behave like a billboard where the code modifies every line
		m_vBillboard.at(PRINT_CHAR_Y_BILLBOARD_END - Y).first.replace(X - PRINT_CHAR_X_BILLBOARD_BEGIN, 1, 1, ConvertChar(ch));
		if (bInverse)
			m_vBillboard.at(PRINT_CHAR_Y_BILLBOARD_END - Y).second = FontDescriptors::FontDLInverse;
		else
			m_vBillboard.at(PRINT_CHAR_Y_BILLBOARD_END - Y).second = FontDescriptors::FontDLRegular;
	}

	m_XBillboard = X;
	m_YBillboard = Y;
}

void TextOutput::PrintCharToLog(unsigned char ch, UINT8 X, bool bInverse)
{
	// TODO: This newline logic where X goes back FAILS when user deletes a char as they type
	// Need to figure out the code where the log goes up for a newline
	if (X < m_XLog)		// it's a new line
	{
		if (m_vLog.size() > PRINT_MAX_LOG_LINES)
		{
			// log is full, erase the last line
			m_vLog.pop_back();
		}
		if (bInverse)
			m_vLog.insert(m_vLog.begin(), pair(wstring(PRINT_CHAR_X_LOG_LENGTH, ' '), FontDescriptors::FontDLInverse));
		else
			m_vLog.insert(m_vLog.begin(), pair(wstring(PRINT_CHAR_X_LOG_LENGTH, ' '), FontDescriptors::FontDLRegular));
	}
	m_vLog.at(0).first.replace(X - PRINT_CHAR_X_LOG_BEGIN, 1, 1, ConvertChar(ch));
	m_XLog = X;
}

#pragma endregion