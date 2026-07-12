# DLRL emulator-hook inventory

This is the preservation checklist for replacing the old Win32 AppleWin fork
with the portable NAC core. The historical implementation remains available in
Git at `Deathlord-Relorded/Emulator/CPU/cpu65C02.h`; the new implementation must
not include UI classes directly from the CPU interpreter.

## Hook surface

The legacy interpreter checks 46 distinct program-counter locations. Several
locations emit more than one behavior, and another group of memory addresses
supplies hook data. They divide into these externally observable areas.

### Application state and transitions

- title/menu/character-management state
- map loading, map-type/floor/sector changes, and transition overlay state
- entering/leaving battle, party death, and end credits
- idle-loop detection and tile redraw/visibility invalidation

### Text and presentation events

- scroll, clear, inverse-line, and character-output hooks
- map/billboard/log routing based on the game's print-window coordinates
- battle actor, attack, dodge, hit, heal, death, monster, and HP-ready events
- level-up and attribute-increase presentation events

### Input changes

- context-sensitive Apple //e arrow-key translation to Deathlord movement keys
- movement-key pit escape
- character attribute auto-reroll state machine and speed changes

### Configurable Relorded gameplay changes

- XP reallocation across all six party members
- rear-line ranged attacks
- search always succeeds
- no level drain
- magic water always increases an attribute
- no attribute ceiling
- no starvation HP loss
- race/class environmental-damage bonuses
- no autosave after a death or TPK
- expanded weapon usability from `InventoryList.csv`
- peasant attribute bonus every fifth level
- preserve overflow XP on level-up
- food distribution before/after purchase
- safe gold pooling and battle-gold distribution
- freeze passage of turns while idle

### Unconditional bug fix

- bypass the level-32 ninja/monk armor-class mask/reset

### Legacy floppy-only hooks to remove

- two-drive prompt bypass
- scenario-disk prompt and insertion
- scenario-floppy validation state (the `$845C` post-validation PC remains as
  a presentation-only HDV checkpoint for showing the Relorded credits splash)
- write interception using `g_wantsToSave`
- floppy speed-up and drive-state polling

The HDV boot path may make some pre-game PCs unreachable. They stay out of the
portable hook table unless an HDV trace proves they are still required.

## Cross-layer coupling to replace

The old CPU interpreter directly calls `AutoMap`, `BattleOverlay`, `InvManager`,
`LogWindow`, `PartyLayout`, and `TextOutput`, and directly reads `NonVolatile`.
The portable core will instead call one frontend-owned hook dispatcher with:

- CPU registers and main/aux memory access
- a settings snapshot containing the Relorded toggles
- typed events for presentation-only effects
- pure helper functions for deterministic gameplay mutations

This keeps rendering and ImGui out of the emulator target and permits hook tests
without SDL/OpenGL.

## Other emulator files with DLRL coupling

- `AppleWin.cpp`: app state reset, DX video policy, legacy timing, and Disk II
  full-speed logic. Replaced by the NAC host loop.
- `Disk.cpp`: save detection and Win32 UI. Removed with Disk II.
- `Speaker.cpp` / `Mockingboard.cpp`: legacy `Game` timing access. Replaced by
  NAC's SDL audio path and wall-clock host loop.
- `Video.cpp`: DLRL utility include. Start from NAC and reapply only proven
  video behavior.
- `DiskImageHelper.cpp`: utility include plus old mixed floppy/HDV parser. Use
  NAC's HDV-only helper.
- `RemoteControlManager`: Win32 Gamelink input and image metadata. Replace with
  NAC's platform-neutral backends if Gamelink remains enabled.

## Porting rule

Each hook receives one of four dispositions in the portable tree: `ported`,
`tested`, `removed-hdv`, or `deferred-late-game-integrity`. No hook is considered
done merely because the game appears to boot.

## Current disposition

- `ported`: application/menu state, map transitions, battle and text events,
  arrow/pit input, auto-reroll, all 16 configurable Relorded changes, and the
  unconditional Ninja/Monk AC fix now live in `src/DlrlHooks.cpp`.
- `tested`: the CPU replacement ABI, idle timer suppression, movement mapping,
  pit exit, the complete v2 autoroll state machine (including SDL lowercase-A
  input, the uppercase Apple //e latch residue, and hidden full-speed mode),
  rear ranged attacks, tile resilience, XP allocation, food/gold conservation,
  Ninja/Monk fix, typed events, and CSV equipment rules run in
  `relorded_hooks`; HDV title/menu traps run in deterministic integration tests.
- `removed-hdv`: every two-drive, scenario-floppy insertion/validation action,
  Disk II write interception, and floppy-speed hook. The action-free `$845C`
  checkpoint is retained solely to time the Relorded credits splash.
- `deferred-late-game-integrity`: no protection bypass has been added or needed
  for boot. The supplied 2.0.1 HDV reaches real gameplay unchanged and its hash
  is checked after tests. A possible later protection symptom in which major
  bosses disappear is unconfirmed and will be studied separately.
