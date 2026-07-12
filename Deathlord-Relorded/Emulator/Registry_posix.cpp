// Non-Windows stub for the AppleWin registry surface. NAC persists its
// own settings via SDL_GetPrefPath() + nac.json, so the emulator core's
// REGSAVE / REGLOAD calls have nowhere to go on Linux / macOS — they
// become no-ops and Loads always report "value not present", which is
// exactly the behaviour the emulator already handles (it falls back to
// the default for every loaded key).

#include "StdAfx.h"
#include "Registry.h"
#include "Common.h"   // REG_CONFIG / REG_CONFIG_SLOT* / REGVALUE_CARD_TYPE / SLOT_AUX

BOOL RegLoadString(LPCTSTR /*section*/, LPCTSTR /*key*/, BOOL /*peruser*/,
                   LPTSTR /*buffer*/, uint32_t /*chars*/)
{
    return FALSE;
}

BOOL RegLoadString(LPCTSTR section, LPCTSTR key, BOOL peruser,
                   LPTSTR buffer, uint32_t chars, LPCTSTR defaultValue)
{
    if (defaultValue && buffer && chars > 0)
    {
        StringCbCopy(buffer, chars, defaultValue);
    }
    return FALSE;
}

BOOL RegLoadValue(LPCTSTR /*section*/, LPCTSTR /*key*/, BOOL /*peruser*/,
                  uint32_t* /*value*/)
{
    return FALSE;
}

BOOL RegLoadValue(LPCTSTR /*section*/, LPCTSTR /*key*/, BOOL /*peruser*/,
                  uint32_t* value, uint32_t defaultValue)
{
    if (value) *value = defaultValue;
    return FALSE;
}

void RegSaveString(LPCTSTR /*section*/, LPCTSTR /*key*/, BOOL /*peruser*/,
                   const std::string& /*buffer*/)
{
}

void RegSaveValue(LPCTSTR /*section*/, LPCTSTR /*key*/, BOOL /*peruser*/,
                  uint32_t /*value*/)
{
}

// Section-name helpers — these don't touch any OS API, just build the
// same path string the Win32 path uses, so consumers that only care
// about the slot name still work.
static inline std::string RegGetSlotSection(UINT slot)
{
    return (slot == SLOT_AUX)
        ? std::string(REG_CONFIG_SLOT_AUX)
        : (std::string(REG_CONFIG_SLOT) + (char)('0' + slot));
}

std::string RegGetConfigSlotSection(UINT slot)
{
    return std::string(REG_CONFIG "\\") + RegGetSlotSection(slot);
}

void RegDeleteConfigSlotSection(UINT /*slot*/)
{
}

void RegSetConfigSlotNewCardType(UINT slot, SS_CARDTYPE type)
{
    RegSaveValue(RegGetConfigSlotSection(slot).c_str(), REGVALUE_CARD_TYPE,
                 TRUE, type);
}
