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
	// create all the lines for the billboard
	// and a single line for the log, and initialize the rest
	UINT8 billboardLineCt = PRINT_CHAR_Y_KEYPRESS - PRINT_CHAR_Y_BILLBOARD_BEGIN;
	for (UINT8 i = 0; i < billboardLineCt; i++)
	{
		m_vBillboard_raw.push_back(pair(string(PRINT_CHAR_X_BILLBOARD_LENGTH, ' '), FontDescriptors::FontDLRegular));
		m_vBillboardString_jp.push_back(string(PRINT_CHAR_X_BILLBOARD_LENGTH, ' '));
		m_vBillboardString_en.push_back(string(PRINT_CHAR_X_BILLBOARD_LENGTH, ' '));
	}
	m_vLog.push_back(pair(wstring(), FontDescriptors::FontDLRegular));
	m_strModule = wstring();
	m_strModule.append(PRINT_CHAR_X_MODULE_LENGTH, ' ');	// Intialize to a specific length
	// WARNING: Never shorten m_strModule
	m_strKeypress = wstring(PRINT_CHAR_X_KEYPRESS_LENGTH, ' ');
}

void TextOutput::Render(SimpleMath::Rectangle r, SpriteBatch* spriteBatch)
{
	// TODO: Should not rely on the gamePtr for fonts?
	auto gamePtr = GetGamePtr();
	auto fontsRegular = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular);
	auto fontsInverse = (*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLInverse);

	// Render everything on the wood plank: Party name, Module string...
	// All centered in the plank.
	float _plankCenterX = 1415.f;
	float _plankMinY = 45.f;
	XMVECTOR _sSize;
	XMFLOAT2 _origin;
	std::wstring _partyName = StringFromMemory(PARTY_PARTYNAME, 16);
	_partyName = ltrim(rtrim(_partyName));
	_sSize = fontsRegular->MeasureString(_partyName.c_str(), false);
	_origin = { r.x + _plankCenterX - (XMVectorGetX(_sSize) / 2.f),
				r.y + _plankMinY - (XMVectorGetY(_sSize) / 2.f) };
	fontsRegular->DrawString(spriteBatch, _partyName.c_str(), _origin, VColorText, 0.f, Vector2(), 1.0f);
	_plankMinY += 20;
	std::wstring _smod(m_strModule);
	_smod = (ltrim(rtrim(_smod)));
	_sSize = fontsRegular->MeasureString(_smod.c_str(), false);
	_origin = {	r.x + _plankCenterX - (XMVectorGetX(_sSize) / 2.f),
				r.y + _plankMinY - (XMVectorGetY(_sSize) / 2.f)};
	fontsRegular->DrawString(spriteBatch, _smod.c_str(), _origin, VColorText, 0.f, Vector2(), 1.0f);
	_plankMinY += 20;

	// Render Keypress string
	// Make sure it's not pure spaces otherwise the inverse font will show
	if (m_strKeypress.find_first_not_of(' ') != string::npos)
	{
		auto _totalTicks = (*gamePtr)->GetTotalTicks();
		auto _halfsec = _totalTicks / 10000000;
		_sSize = fontsRegular->MeasureString(m_strKeypress.c_str(), false);
		float _kpScale = 1.0f;
		if (_halfsec % 2)
			fontsRegular->DrawString(spriteBatch, m_strKeypress.c_str(),
				{ r.x + 1412.f - _kpScale * (XMVectorGetX(_sSize) / 2.f), r.y + 815.f }, Colors::AntiqueWhite, 0.f, Vector2(), _kpScale);
		else
			fontsInverse->DrawString(spriteBatch, m_strKeypress.c_str(),
				{ r.x + 1412.f - _kpScale * (XMVectorGetX(_sSize) / 2.f), r.y + 815.f }, Colors::AntiqueWhite, 0.f, Vector2(), _kpScale);
	}

	// Render Billboard
	float yInc = 0.f;
	auto _bb = m_vBillboardString_jp;
	if (g_nonVolatile.englishNames)
		_bb = m_vBillboardString_en;
	size_t _bbindex = 0;
	for each (auto bbLine in _bb)
	{
		(*gamePtr)->GetSpriteFontAtIndex(m_vBillboard_raw[_bbindex].second)->DrawString(spriteBatch, bbLine.c_str(),
			{ r.x + 1287.f, r.y + 994.f + yInc }, VColorText, 0.f, Vector2(), 1.f);
		yInc -= 18;
		_bbindex++;
	}

	// Render Log
	yInc = 0.f;
	size_t _lineIdx = 0;
	for each (auto logLine in m_vLog)
	{
		if (_lineIdx == 0)	// Emboss the input line
			(*gamePtr)->GetSpriteFontAtIndex(logLine.second)->DrawString(spriteBatch, logLine.first.c_str(),
				{ r.x + 1299.f, r.y + 757.f + yInc }, Colors::DarkBlue, 0.f, Vector2(), 1.f);
		(*gamePtr)->GetSpriteFontAtIndex(logLine.second)->DrawString(spriteBatch, logLine.first.c_str(),
			{ r.x + 1298.f, r.y + 755.f + yInc }, VColorText, 0.f, Vector2(), 1.f);
		_lineIdx++;
		yInc -= 18;
	}

}

#pragma region utilities

TextWindows TextOutput::AreaForCoordinates(UINT8 xStart, UINT8 xEnd, UINT8 yStart, UINT8 yEnd)
{
	/*
	char _bufPrint[200];
	sprintf_s(_bufPrint, 200, "AreaForCoordinates: %2d, %2d, %2d, %2d\n",
		xStart, xEnd, yStart, yEnd);
	OutputDebugStringA(_bufPrint);
	*/

	if (yStart < PRINT_CHAR_Y_MODULE)
	{
		if (xStart >= PRINT_CHAR_X_BILLBOARD_BEGIN)
		{
			switch (yStart)
			{
			case 0x06: return TextWindows::Player1;
			case 0x07: return TextWindows::Player2;
			case 0x08: return TextWindows::Player3;
			case 0x09: return TextWindows::Player4;
			case 0x0A: return TextWindows::Player5;
			case 0x0B: return TextWindows::Player6;
			case PRINT_CHAR_Y_PARTYNAME: return TextWindows::PartyName;
			default: return TextWindows::Unknown;
			}
		}
		// This is the area of the map!
		return TextWindows::Unknown;
	}
	else
	{
		if (xStart == PRINT_CHAR_X_ORIGIN_LEFT)
			return TextWindows::Log;
		if (xStart >= PRINT_CHAR_X_BILLBOARD_BEGIN)
		{
			// Unfortunately the game pools together everything below the party area
			// with the billboard. Generally we can't tell the difference
			if (yStart == PRINT_CHAR_Y_KEYPRESS)
				return TextWindows::Keypress;
			if (yStart == PRINT_CHAR_Y_MODULE)
				return TextWindows::Module;
			return TextWindows::ModuleBillboardKeypress;
		}
	}
	return TextWindows::Unknown;
}

#pragma endregion

#pragma region Deathlord printing

inline wchar_t TextOutput::ConvertChar(unsigned char ch)
{
	// To use a regular charset, we need to convert it:
	// return ARRAY_DEATHLORD_CHARSET[ch & 0x7F];
	// But when using the Deathlord charset in a spritefont, no need
	// Just remove the 'end of string' marker
	return (ch & 0x7F);
}

template <typename CharType>
inline bool TextOutput::IsEndOfToken(CharType ch) {
	// Check if it's a token delimiter
	// Takes any CharType, including char and wchar_t
	return ((ch < 0x80) && (ch != ' '));
}

void TextOutput::ScrollWindow(TextWindows tw)
{
	switch (tw)
	{
	case TextWindows::Log:
	{
		// first convert to english before scrolling if requested
		if (g_nonVolatile.englishNames)
		{
			// The text may have a bunch of spaces at the end, needs to be erased
			if (m_vLog.size() > 0)
			{
				size_t lastNonSpace = m_vLog.at(0).first.find_last_not_of(L' ');
				if (lastNonSpace != std::wstring::npos) {
					m_vLog.at(0).first.erase(lastNonSpace + 1);
				}
				for (const auto& wordPair : m_wtranslations) {
					if (m_vLog.at(0).first == wordPair.first) {
						m_vLog.at(0).first = wordPair.second;
						break;
					}
				}
			}
		}
		if (m_vLog.size() > PRINT_MAX_LOG_LINES)	// log is full, erase the last line
			m_vLog.pop_back();
		m_vLog.insert(m_vLog.begin(), pair(wstring(PRINT_CHAR_X_LOG_LENGTH, ' '), FontDescriptors::FontPR3Regular));
		break;
	}
	case TextWindows::Billboard:
	{
		m_vBillboard_raw.pop_back();
		m_vBillboardString_jp.pop_back();
		m_vBillboardString_en.pop_back();
		m_vBillboard_raw.insert(m_vBillboard_raw.begin(), pair(string(PRINT_CHAR_X_BILLBOARD_LENGTH, ' '), FontDescriptors::FontPR3Regular));
		m_vBillboardString_jp.insert(m_vBillboardString_jp.begin(), string(PRINT_CHAR_X_BILLBOARD_LENGTH, ' '));
		m_vBillboardString_en.insert(m_vBillboardString_en.begin(), string(PRINT_CHAR_X_BILLBOARD_LENGTH, ' '));
		break;
	}
	default:
#ifdef _DEBUG
		char _buf[300];
		sprintf_s(_buf, 300, "Scrolling default, nothing done: %02X\n", (int)tw);
		OutputDebugStringA(_buf);
#endif
		break;
	}
}

void TextOutput::ScrollLog()
{
	// This is generally called when starting a battle
	// For now let's just scroll a bit to give some space
	ScrollWindow(TextWindows::Log);
	ScrollWindow(TextWindows::Log);
	ScrollWindow(TextWindows::Log);
}

void TextOutput::ClearLog()
{
	m_vLog.clear();
}

void TextOutput::ClearBillboard()
{
	UINT8 billboardLineCt = PRINT_CHAR_Y_BILLBOARD_BEGIN;
	while (billboardLineCt <= PRINT_CHAR_Y_BILLBOARD_END)
	{
		m_vBillboard_raw.at(billboardLineCt - PRINT_CHAR_Y_BILLBOARD_BEGIN).first = string(PRINT_CHAR_X_BILLBOARD_LENGTH, ' ');
		m_vBillboard_raw.at(billboardLineCt - PRINT_CHAR_Y_BILLBOARD_BEGIN).second = FontDescriptors::FontPR3Regular;
		m_vBillboardString_jp.at(billboardLineCt - PRINT_CHAR_Y_BILLBOARD_BEGIN) = string(PRINT_CHAR_X_BILLBOARD_LENGTH, ' ');
		m_vBillboardString_en.at(billboardLineCt - PRINT_CHAR_Y_BILLBOARD_BEGIN) = string();
		++billboardLineCt;
	}
}

void TextOutput::PrintCharRaw(unsigned char ch, TextWindows tw, UINT8 X, UINT8 Y, bool bInverse)
{
	switch (tw)
	{
	case TextWindows::Unknown:
	{
		// Disregard anything in the upper area of the screen
		// We don't print the party list
#ifdef _DEBUG
		char _bufPrint[200];
		sprintf_s(_bufPrint, 200, "TRYING TO PRINT TO UNKNOWN AREA: %c, %2d, %2d, %2d\n",
			ARRAY_DEATHLORD_CHARSET[ch & 0x7F], tw, X, Y);
		OutputDebugStringA(_bufPrint);
#endif
		return;
		break;
	}
	case TextWindows::PartyName:
		break;
	case TextWindows::Module:
		PrintCharToModule(ch, X, bInverse);
		break;
	case TextWindows::Log:
		PrintCharToLog(ch, X, bInverse);
		break;
	case TextWindows::ModuleBillboardKeypress:
		if (Y == PRINT_CHAR_Y_MODULE)
			PrintCharToModule(ch, X, bInverse);
		else if (Y == PRINT_CHAR_Y_KEYPRESS)
			PrintCharToKeypress(ch, X, bInverse);
		else
		{
			PrintCharToBillboard(ch, X, Y, bInverse);
			/*  poor man's debugging
			wchar_t _buf[200];
			swprintf_s(_buf, 200, L"Printing char: %c - %03d,%03d", ARRAY_DEATHLORD_CHARSET[ch & 0x7F], X, Y);
			PrintWStringToLog(std::wstring(_buf), bInverse);
			*/
		}
		break;
	case TextWindows::Billboard:
		// Here Deathlord prints to the billboard area
		// But during battle it uses it as a bigger log print area
		// Unless the user wants to choose a spell. In which case
		// it uses it as a list selection area
		PrintCharToBillboard(ch, X, Y, bInverse);
		break;
	case TextWindows::Keypress:
		PrintCharToKeypress(ch, X, bInverse);
		break;
	default:
		// This is the list of party members
		// TODO: here we could trigger an update of the characters module
		break;
	}
}

void TextOutput::PrintCharToModule(unsigned char ch, UINT8 X, bool bInverse)
{
	// The module string is updated on a char by char basis
	// Replace a single character at a time
	if (m_strModule.size() < (X - PRINT_CHAR_X_MODULE_BEGIN + 1))
		m_strModule.resize(X - PRINT_CHAR_X_MODULE_BEGIN + 1, ' ');
	m_strModule.replace(X - PRINT_CHAR_X_MODULE_BEGIN, 1, 1, ConvertChar(ch));
	m_XModule = X;
}

void TextOutput::PrintCharToKeypress(unsigned char ch, UINT8 X, bool bInverse)
{
	// The module string is updated on a char by char basis
	// Replace a single character at a time
	if (m_strKeypress.size() > (X - PRINT_CHAR_X_KEYPRESS_BEGIN))
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
	{
		// Behave like a billboard where the code modifies every line
		m_vBillboard_raw.at(PRINT_CHAR_Y_BILLBOARD_END - Y).first.replace(X - PRINT_CHAR_X_BILLBOARD_BEGIN, 1, 1, ch);
		if (bInverse)
			m_vBillboard_raw.at(PRINT_CHAR_Y_BILLBOARD_END - Y).second = FontDescriptors::FontDLInverse;
		else
			m_vBillboard_raw.at(PRINT_CHAR_Y_BILLBOARD_END - Y).second = FontDescriptors::FontDLRegular;

		// And generate the string that will be printed by default ("fake japanese")
		m_vBillboardString_jp.at(PRINT_CHAR_Y_BILLBOARD_END - Y).replace(X - PRINT_CHAR_X_BILLBOARD_BEGIN, 1, 1, ConvertChar(ch));
	}

	m_XBillboard = X;
	m_YBillboard = Y;

	// Now let's do the english stuff
	// Whenever a character is modified in the billboard line, we parse the whole billboard line
	// Look for end-of-token markers, and then search for matches from the beginning of the last marker
	// It's a lot of wasted work but the only way to reliably replace words with differing lengths
	{
		char _bufPrint[200];
		auto _bbstr = m_vBillboard_raw.at(PRINT_CHAR_Y_BILLBOARD_END - Y).first;
		auto _bbstrJP = m_vBillboardString_jp.at(PRINT_CHAR_Y_BILLBOARD_END - Y);
		sprintf_s(_bufPrint, 200, "bbstrjp: *%s*\n", _bbstrJP.c_str());
		OutputDebugStringA(_bufPrint);
		auto _bbstrEN = _bbstr;
		for (const auto& [key, value] : m_translations) {
			size_t pos = 0;
			while ((pos = _bbstrEN.find(key, pos)) != std::string::npos) {
				std::string replacement = value;
				if (replacement.size() < key.size()) {
					replacement += std::string(key.size() - replacement.size(), ' ');
				}
				sprintf_s(_bufPrint, 200, "key: *%s*  replacement: *%s*\n", key.c_str(), replacement.c_str());
				OutputDebugStringA(_bufPrint);

				_bbstrEN.replace(pos, replacement.size(), replacement);
				pos += replacement.size();  // Move the position to after the replacement
			}
		}
		for (char& ch : _bbstrEN) {
			ch = ConvertChar(ch);
		}
		m_vBillboardString_en.at(PRINT_CHAR_Y_BILLBOARD_END - Y) = _bbstrEN;
	}

	/*
	if (IsEndOfToken(ch))
	{
		size_t startPos = 0;
		auto _bbstr = m_vBillboard_raw.at(PRINT_CHAR_Y_BILLBOARD_END - Y).first;
		auto _bbstrJP = m_vBillboardString_jp.at(PRINT_CHAR_Y_BILLBOARD_END - Y);
		auto _bbstrEN = std::wstring();
		wchar_t _convwc;	// converted wchar into the actual character

		while (startPos < _bbstr.length()) {
			// Find the next end of token (could be a 2-word token like "Toshi Bow")
			auto it = std::find_if(_bbstr.begin() + startPos, _bbstr.end(), [this](unsigned char ch) {
				return this->IsEndOfToken(ch);
				});

			auto foundPos = std::distance(_bbstr.begin(), it);
			std::string word = _bbstr.substr(startPos, foundPos - startPos + 1);

			char _bufPrint[200];
			sprintf_s(_bufPrint, 200, "WORD FOUND: *%s*\n", word.c_str());
			OutputDebugStringA(_bufPrint);

			// Check if the word is in the itemNames dictionary
			bool _didMatch = false;
			for (const auto& wordPair : m_translations) {
				if (word == wordPair.first) {
					_bbstrEN += wordPair.second;
					_didMatch = true;
					break;
				}
			}
			if (!_didMatch)
			{
				_bbstrEN += _bbstrJP.substr(startPos, foundPos - startPos + 1);
			}

			// Move to the next word
			if (foundPos == _bbstr.length())
				break;
			startPos = foundPos + 1;
		}
		m_vBillboardString_en.at(PRINT_CHAR_Y_BILLBOARD_END - Y) = _bbstrEN;

		// Find the start of the next word
		_convwc = _bbstrJP[startPos];
		while (startPos < _bbstr.length() && HA::IsDelimiter(_convwc)) {
			// In between words, if it's a space, see if the JP and EN are different sizes
			// In that case, either add a space to EN or don't, in order to, in the end,
			// have the exact string sizes for both. Otherwise the merchant info won't align
			// Things like:   DAGGER    15   may end up with too many or too few spaces
			if (_bbstr[startPos] == ' ')
			{
				while (_bbstrEN.length() <= startPos)
				{
					_bbstrEN += L' ';
				}
			}
			else {
				_bbstrEN += _convwc;
			}
			startPos++;
		}
	}
	*/
}

void TextOutput::InverseLineInBillboard(UINT8 line)
{
	if (m_vBillboard_raw.at(line).second == FontDescriptors::FontDLRegular)
		m_vBillboard_raw.at(line).second = FontDescriptors::FontDLInverse;
	else
		m_vBillboard_raw.at(line).second = FontDescriptors::FontDLRegular;
}

void TextOutput::PrintCharToLog(unsigned char ch, UINT8 X, bool bInverse)
{
	m_vLog.at(0).first.replace(X - PRINT_CHAR_X_LOG_BEGIN, 1, 1, ConvertChar(ch));
	if (bInverse)
		m_vLog.at(0).second = FontDescriptors::FontDLInverse;
	else
		m_vLog.at(0).second = FontDescriptors::FontDLRegular;
	m_XLog = X;
}

void TextOutput::PrintWStringToLog(std::wstring ws, bool bInverse)
{
	OutputDebugString(ws.c_str());
	OutputDebugString(L"\n");
	m_vLog.at(0).first = ws;
	if (bInverse)
		m_vLog.at(0).second = FontDescriptors::FontDLInverse;
	else
		m_vLog.at(0).second = FontDescriptors::FontDLRegular;
	m_XLog = 0;
	ScrollWindow(TextWindows::Log);
}

void TextOutput::AddTranslation(std::wstring* jp, std::wstring* en)
{
	if (!jp || !en)
		return;
	if (jp->empty())
		return;
	m_wtranslations[*jp] = *en;
	auto _key = std::string();
	auto _val = std::string();
	HA::ConvertWStrToStr(jp, &_key);
	// For each character, except the last, set the high bit
	for (size_t i = 0; i < _key.size() - 1; ++i) {
		_key[i] |= 0x80;  // Set the high bit
	}
	// For the last character, remove the high bit
	_key.back() &= 0x7F;

	HA::ConvertWStrToStr(en, &_val);
	m_translations[_key] = _val;

#ifdef _DEBUG
	for (size_t i = 0; i < _key.size() - 1; ++i) {
		_key[i] &= 0x7F;
	}
	char _bufPrint[200];
	sprintf_s(_bufPrint, 200, "Added translation *%s* : *%s*\n", _key.c_str(), _val.c_str());
	OutputDebugStringA(_bufPrint);
#endif
}

void TextOutput::RemoveTranslation(std::wstring* jp)
{
	m_wtranslations.erase(*jp);
	auto _key = std::string();
	HA::ConvertWStrToStr(jp, &_key);
	// For each character, except the last, set the high bit
	for (size_t i = 0; i < _key.size() - 1; ++i) {
		_key[i] |= 0x80;  // Set the high bit
	}
	// For the last character, remove the high bit
	_key.back() &= 0x7F;

	m_translations.erase(_key);
}

void TextOutput::ClearTranslations()
{
	m_wtranslations.clear();
	m_translations.clear();
}

#pragma endregion