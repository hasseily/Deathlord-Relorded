/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2021, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "StdAfx.h"
#include "Card.h"

#include "Mockingboard.h"
#include "Harddisk.h"
#include "LanguageCard.h"
#include "Memory.h"
#include "SAM.h"
#include "VidHD.h"

#include <sstream>

void Card::ThrowErrorInvalidSlot()
{
	ThrowErrorInvalidSlot(m_type, m_slot);
}

void Card::ThrowErrorInvalidSlot(SS_CARDTYPE type, UINT slot)
{
	std::ostringstream msg;
	msg << "The card '" << GetCardName(type);
	msg << "' is not allowed in Slot " << slot << ".";

	throw std::runtime_error(msg.str());
}

void Card::ThrowErrorInvalidVersion(UINT version)
{
	ThrowErrorInvalidVersion(m_type, version);
}

void Card::ThrowErrorInvalidVersion(SS_CARDTYPE type, UINT version)
{
	std::ostringstream msg;
	msg << "Version " << version;
	msg << " is not supported for card '" << GetCardName(type) << "'.";

	throw std::runtime_error(msg.str());
}

void DummyCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	switch (QueryType())
	{
	case CT_GenericClock:
	case CT_Echo:
	default:
		_ASSERT(0);
	}
}

void DummyCard::Update(const ULONG nExecutedCycles)
{
	switch (QueryType())
	{
	case CT_GenericClock:
	case CT_Echo:
	default:
		_ASSERT(0);
		break;
	}
}

void DummyCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	switch (QueryType())
	{
	case CT_GenericClock:
	case CT_Echo:
	default:
		_ASSERT(0);
		break;
	}
}

bool DummyCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	switch (QueryType())
	{
	case CT_GenericClock:
	case CT_Echo:
	default:
		_ASSERT(0);
	}
	return false;
}

const std::string& Card::GetCardNameEmpty()
{
	static const std::string name("Empty");
	return name;
}

std::string Card::GetCardName(void)
{
	return GetCardName(m_type);
}

std::string Card::GetCardName(const SS_CARDTYPE cardType)
{
	switch (cardType)
	{
	case CT_Empty:
		return Card::GetCardNameEmpty();
	case CT_LanguageCard:
		return LanguageCardSlot0::GetSnapshotCardName();
	case CT_Saturn128K:
		return Saturn128K::GetSnapshotCardName();
	case CT_MockingboardC:
		return MockingboardCard::GetSnapshotCardName();
	case CT_GenericHDD:
		return HarddiskInterfaceCard::GetSnapshotCardName();
	case CT_Phasor:
		return MockingboardCard::GetSnapshotCardNamePhasor();
	case CT_SAM:
		return SAMCard::GetSnapshotCardName();
	case CT_80Col:
		return MemGetSnapshotCardName80Col();
	case CT_Extended80Col:
		return MemGetSnapshotCardNameExtended80Col();
	case CT_RamWorksIII:
		return MemGetSnapshotCardNameRamWorksIII();
	case CT_VidHD:
		return VidHDCard::GetSnapshotCardName();
	case CT_MegaAudio:
		return MockingboardCard::GetSnapshotCardNameMegaAudio();
	case CT_SDMusic:
		return MockingboardCard::GetSnapshotCardNameSDMusic();
	default:
		return "Unknown";
	}
}

SS_CARDTYPE Card::GetCardType(const std::string & card)
{
	if (card == MockingboardCard::GetSnapshotCardName())
		return CT_MockingboardC;
	else if (card == MockingboardCard::GetSnapshotCardNamePhasor())
		return CT_Phasor;
	else if (card == SAMCard::GetSnapshotCardName())
		return CT_SAM;
	else if (card == HarddiskInterfaceCard::GetSnapshotCardName() || card == HarddiskInterfaceCard::GetSnapshotCardNameOld())
		return CT_GenericHDD;
	else if (card == LanguageCardSlot0::GetSnapshotCardName())
		return CT_LanguageCard;
	else if (card == Saturn128K::GetSnapshotCardName())
		return CT_Saturn128K;
	else if (card == VidHDCard::GetSnapshotCardName())
		return CT_VidHD;
	else if (card == MockingboardCard::GetSnapshotCardNameMegaAudio())
		return CT_MegaAudio;
	else if (card == MockingboardCard::GetSnapshotCardNameSDMusic())
		return CT_SDMusic;
	else
		throw std::runtime_error("Slots: Unknown card: " + card);	// todo: don't throw - just ignore & continue
}
