#pragma once

#include "Card.h"
#include "LanguageCard.h"
#include "MockingboardCardManager.h"
#include "Common.h"

class CardManager
{
public:
	CardManager(void) :
		m_pVidHDCard(NULL)
	{
		// LoadConfiguration() now sets up default cards for a new install
		InsertInternal(SLOT0, CT_Empty);
		InsertInternal(SLOT1, CT_Empty);
		InsertInternal(SLOT2, CT_Empty);
		InsertInternal(SLOT3, CT_Empty);
		InsertInternal(SLOT4, CT_Empty);
		InsertInternal(SLOT5, CT_Empty);
		InsertInternal(SLOT6, CT_Empty);
		InsertInternal(SLOT7, CT_Empty);
		InsertAuxInternal(CT_Extended80Col);	// For Apple //e and above
	}
	~CardManager(void)
	{
		for (UINT i=0; i<NUM_SLOTS; i++)
			RemoveInternal(i);
		RemoveAuxInternal();
	}

	void Insert(UINT slot, SS_CARDTYPE type, bool updateRegistry = true);
	void Remove(UINT slot, bool updateRegistry = true);
	SS_CARDTYPE QuerySlot(UINT slot) { _ASSERT(slot<NUM_SLOTS); return m_slot[slot]->QueryType(); }
	Card& GetRef(UINT slot)
	{
		_ASSERT(m_slot[slot]);
		return *m_slot[slot];
	}
	Card* GetObj(UINT slot)
	{
		return m_slot[slot];
	}

	void InsertAux(SS_CARDTYPE type, bool updateRegistry = true);
	void RemoveAux(void);
	SS_CARDTYPE QueryAux(void) { return m_aux->QueryType(); }
	Card* GetObjAux(void) { _ASSERT(0); return m_aux; }	// ASSERT because this is a DummyCard

	//

	LanguageCardManager& GetLanguageCardMgr(void) { return m_languageCardMgr; }
	MockingboardCardManager& GetMockingboardCardMgr(void) { return m_mockingboardCardMgr; }
	class VidHDCard* GetVidHDCard(void) { return m_pVidHDCard; }
	SS_CARDTYPE QueryDefaultCardForSlot(UINT slot, eApple2Type model);

	void GetCardChoicesForSlot(const UINT slot, const SS_CARDTYPE currConfig[NUM_SLOTS], std::vector<SS_CARDTYPE>& choicesList);
	void GetCardChoicesForAuxSlot(std::vector<SS_CARDTYPE>& choicesList);

	void InitializeIO(LPBYTE pCxRomPeripheral);
	void Destroy(void);
	void Reset(const bool powerCycle);
	void Update(const ULONG nExecutedCycles);
	void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);

private:
	void InsertInternal(UINT slot, SS_CARDTYPE type);
	void InsertAuxInternal(SS_CARDTYPE type);
	void RemoveInternal(UINT slot);
	void RemoveAuxInternal(void);
	bool IsSingleInstanceCard(SS_CARDTYPE card);

	Card* m_slot[NUM_SLOTS];
	Card* m_aux;
	LanguageCardManager m_languageCardMgr;
	MockingboardCardManager m_mockingboardCardMgr;
	class VidHDCard* m_pVidHDCard;
};
