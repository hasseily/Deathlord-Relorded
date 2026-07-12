# DLRL 3.0 party and clean-game tools

The File menu owns four destructive/persistence-related tools.

## Clean new game

The active writable HDV is a per-user copy. The immutable clean template comes
from `extras/deathlord-relorded-win-201/Images/Deathlord PRODOS.hdv` during a
development build and is staged as `Resources/Images/Deathlord PRODOS.hdv`.
After confirmation, DLRL closes the SmartPort image, prepares a complete
temporary copy, swaps it over the active HDV with a recoverable backup, removes
the backup after success, reinserts the active image, and power-cycles the
emulator. The clean template itself is never opened for writing.

## Party transfer format

Export uses UTF-8 JSON with format name `deathlord-relorded-party` and version
`1`. Its extension is `.dlrl-party.json`. The envelope records:

- the UTC export timestamp and decoded party name;
- party size, leader, and current-character controls;
- all 16 encoded bytes of the party name;
- every byte from Apple II main memory `$FD00-$FFFF`.

The raw 768-byte payload deliberately includes both documented and undocumented
party storage. Names, statistics, status, inventory, item charges, equipment,
and any party features not yet understood by the modern UI therefore survive a
round trip without being reinterpreted. Map and emulator state are outside this
range and are not imported.

An import is accepted only when its format/version, metadata, control indexes,
and exact byte counts validate. The file is loaded before the confirmation
modal, but no Apple II memory changes until the player confirms.

## Live party editor

The editor is available only with an active party and writes directly to the
same Apple II memory consumed by Deathlord and the modern UI. It exposes:

- party and character names, leader, class, race, gender, alignment, and magic
  discipline;
- level, waiting levels, XP, current/maximum health and power, gold, food,
  torches, and armor class;
- strength, constitution, size, intelligence, dexterity, and charisma;
- all known status flags;
- all eight inventory categories, raw item IDs, charges, and readied weapon.

Equipment, class, or race changes invoke Deathlord's own all-party armor-class
calculation routine. Values are constrained to the storage limits known from
v2, while the exported raw payload remains the authoritative lossless backup.
