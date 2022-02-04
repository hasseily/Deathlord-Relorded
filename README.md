# Deathlord Relorded

Deathlord... the infamous Apple 2 RPG of 1987.
Too tough for many, just the right level of sadistic extremism for others. But with brilliant dungeon design, and always interesting!
It is time to relord Deathlord, make it more accessible, better looking, and in the process fix a few bugs.
And make sure you never need to "save scum". Play, save, backup obviously. But never retry a dozen times something to get a good result.
This means that magic pools always give you stats increase, search never fails, and monsters never level drain you.
It'll still be tough as nails, it's Deathlord after all. But it will never force you to reload to get a better outcome, something every player had to constantly do in the original game.

This app requires a 1920x1080 display, DirectX 12, Windows 10 x64, with working sound.
Fork on GitHub or contact me for other builds, but the DirectX 12 requirement is set in stone, sorry.

## How I'm doing it

The architecture of Deathlord Relorded is relatively unique. Fundamentally, you'll be running the original Deathlord game in an emulator.
No change has been made whatsoever to the original Deathlord codebase. Even the original copy protection is kept.
In fact, you can take the .woz images of the original game and run them in your favorite emulator, the game is untouched.

What has changed, and immensely so, is the emulation layer that is running the game. I took the AppleWin codebase and transformed it.
First, I ripped out everything except for the core emulation code of an Apple //e Enhanced. I rewrote the graphics layer in pure DX12.
Then I heavily tweaked the CPU emulation to hook into the original code as necessary and fire off either display events or changes
in the code behavior. So sometimes the emulated CPU will do something different, and sometimes it's the GUI layer that will update itself.

This results in a game that looks retro-modern, but whose logic is encapsulated in a 6502 assembly codebase from 1987.

## Installation

Grab the latest release from GitHub. Unzip it anywhere, and double click the app.

## Features

* The game runs in a fixed 1920x1080 resolution, windowed or full screen.
* The map is expanded from 9x9 to 32x32
* There's a world mini-map to not get lost at sea
* Many, many more colors
* Tilesets redone with the help from @BillG.
* Log is 30 lines long instead of 3
* Fixed known Deathlord bugs such as the ninja/monk AC reset at level 32, and stat increase ceiling
* Magic water effect is always a random stat increase
* Search always works (that was evil!)
* No char HP loss from starvation, but no automatic healing when starving
* No level drain
* Certain races and classes have bonuses
* Rear rank can use ranged weapons
* No autosave when char dies in battle
* Equipment use has improved. Peasants can use light bows and longbows, ninjas can wield katanas
* Battle XP allocation is properly distributed across all characters
* Every character gets battle XP. No more having to "cast-cancel" to get XP for mages that don't want to cast spells
* Additional XP does not reset on level up
* Can check missing required XP when trying to level up

Many thanks to @a2_qkumba for code analysis and @BillG for graphics.

Happy retro RPG gaming, and see you on the Lost Sectors Discord server, or on the discussions here in GitHub.

Rikkles, Lebanon, 2022.
@RikRetro on Twitter.

https://github.com/hasseily/Deathlord-Relorded
