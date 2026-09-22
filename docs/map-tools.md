# Map visibility and teleportation

Both tools live under **Relorded → Map**.

- **Fog of War** toggles the automap and world-minimap visibility mask. The
  setting persists across launches. Turning fog off does not mark every tile
  explored: normal exploration continues, and turning fog back on restores
  the appropriate explored/unexplored view.
- **Teleport to Map…** pauses the game while the dialog is open. Search by map
  or region, optionally filter by map type, select a dungeon floor, then click
  a tile in the preview or enter X and Y. Coordinates start at the top-left:
  0–31 within a dungeon floor, or 0–63 in towns and overworld maps. **World -
  any sector** additionally accepts world-sector X/Y in the range 0–15.

The initial position prefers nearby plain terrain. Teleportation does not
grant immunity or make a tile passable: walls, water, monsters, and traps can
still be dangerous. Cancel or Escape leaves the party in place. Teleport is
available only at the ordinary movement prompt, outside combat and other
game commands; it supports the Deathlord 2.0.1 HDV executable.

To reach Kawahara's Dungeon, search for **Kawahara**, choose a floor (1–8),
choose a clear tile, and press **Teleport**.

## Complete town and dungeon catalogue

The scenario contains 33 town/interior maps and 20 dungeons (138 dungeon
floors). Surface buildings and their underground maps are separate entries.
The dialog also includes the 20 land-bearing overworld sectors and an entry
for arbitrary world sectors, including open sea.

| Region | Towns and interiors | Dungeons |
| --- | --- | --- |
| Akmihr | Desert Flower; Oasis; Akhamun-Ra's Pyramid; Sultan's Palace | Akhamun-Ra's Pyramid - dungeon; Kobito Mines |
| Asagata | Towne Royal; Croyo | Fire Giants' Lair |
| Black Isles | Red Shogun's Castle | Red Shogun's Castle - dungeon; Doors Dungeon |
| Chigaku | Crystalmist; Fort Wintergreen | Tower of Shumi; Troll Hole |
| Giluin | Kobar; Shupan; Temple of Oceanus | Linear Dungeon |
| Hell Island | Skull Keep | Hell |
| Isle of the Dead | Pyramid of the Old Ones | Pyramid of the Old Ones - dungeon |
| Kodan | Tokugawa; Emperor's Palace; Kawa; Tokushima; Yokahama Ruins; Wakiza Ruins | Kawahara's Dungeon; Yakuza Guild; Caves east of Kawa; Pirate's Den |
| Lost Isles | — | Caves of the Four Elements |
| Narawn | Kashiwa; Malkanth; Fort Demonguard; Lost Lagoon | — |
| Nyuku | Twin Rivers; North Spindrift; South Spindrift | Sunken Temple |
| Osozaki | Deepingdale; Wakai Ruins (Vorn) | Telegrond |
| Sirion | Greenbanks Ruins; Clearview | Chessboard Dungeon; Staircase Dungeon |
| Tsumani | Morningfrost; Snow Raven | Chutes and Ladders |

Desert Island and Forest Island have overworld terrain but no town/dungeon
entrances.

## Implementation and verification

`MapTravel` reads `DEATHLORD.Z` from the active HDV without modifying it. Names
and world-sector addresses identify the 2.0.1 scenario; entrance records,
floor counts, compressed terrain, and dungeon-group addresses come from the
disk. Dungeon previews use floor-local coordinates, translated into the
game's four-floor 64×64 tile buffer on arrival.

The hook saves the departure map and invokes Deathlord's native loaders at
the idle movement checkpoint. Dungeons beneath towns first use the native
town entrance so their return map and stairs remain valid. It does not edit
the source template or rewrite a saved party location externally. Choosing
Teleport during play uses the active game and its normal map-save behavior;
use the game's normal save command to save party progress.

Fog is keyed by world sector, scenario address, and floor, so unrelated maps
do not share exploration. Existing fog markers migrate when first visited.

`hdv_map_travel` visits all 192 destination/floor combinations and checks
loaded terrain, map type, floor, and exact party coordinates.
`hdv_map_exits` walks out of all 20 dungeons after teleporting. Both boot a
temporary HDV copy. Native-window fixtures cover the teleport dialog at
1280×900 and 640×480, plus fog enabled/disabled on the same map.
