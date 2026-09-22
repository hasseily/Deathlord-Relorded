# Hacking menu

The top-level **Hacking** menu contains:

- **Party Editor…** — the full existing live editor, moved from File. Edit
  party/character identity, leader, classes, races, attributes, levels, XP,
  health, power, money, supplies, statuses, inventory, charges, and equipment.
  It is enabled only when there is an active party. See [party tools](party-tools.md).
- **Invincible** — prevents gameplay HP loss, including ordinary combat and
  spell damage, poison/starvation damage, traps, whole-party HP halving,
  instant-death HP clearing, and maximum-HP drain/destruction. Healing still
  works. It does not cure nonfatal status effects or resurrect existing dead
  characters. Explicit edits in Party Editor remain available.
- **Automatic Battle Success** — defeats enemies without requiring combat
  actions, then runs Deathlord's normal post-battle XP, level, map, leader,
  and loot handling. It can also be enabled at an existing battle command
  prompt. It does not count as fleeing or grant unrelated quest items.

Both checkboxes default to off and persist in host settings under `hacking`.
They are independent of **Relorded → Relorded Changes**, whose **Enable all
fixes** action does not enable cheats. These hooks target the bundled 2.0.1
HDV game; no disk-image code is patched.

## Verification

`hdv_hacking` boots a temporary copy of the canonical HDV, then executes the
actual loaded HP and battle routines. It checks 16-bit borrow/overflow cases,
lethal damage and death flags, every party member's HP-halving protection,
direct HP clearing, maximum-HP reductions, healing, and toggling protection
off again. Native battles are tested both with automatic victory enabled
before combat and enabled at the command prompt, with fair XP allocation on
and off, ordinary/high-level enemies, and scripted room encounters.

The hook unit tests also check default-off behavior, unmodified normal
combat, stack preservation, the native XP kill limit, and duplicate-award
prevention. `sdl_hacking_menu` captures the new menu with both cheats checked;
`sdl_party_editor_smoke` continues to exercise the full editor.
