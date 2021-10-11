# Deathlord Companion

Welcome to Deathlord Companion!
It is a fully self-contained Windows 10 application for running Deathlord, the infamous Apple 2 RPG of 1987.
It goes beyond simple emulation, and provides you with configurable sidebars to display all sorts of useful in-game information, as well as an auto-log which continuously copies conversations into a window that you can edit, translate, save...

Deathlord  Companion also provides GameLink support for Grid Cartographer, a commercial program that helps you map as you play (or use existing maps).

Deathlord Companion's emulation layer is based on AppleWin, the great Apple 2 emulator. This program would never have seen the light of day without the amazing work of the AppleWin developers.

This app requires DirectX 12 and Windows 10 x64, with working sound.
Fork on github or contact me for other builds, but the DirectX 12 requirement is set in stone, sorry.

## Installation

- Copy the Deathlord folder anywhere you want, ideally in your Program Files directory.
- Run the companion .exe. The first time you run it, it will ask you to select the Deathlord boot disk image you want to use. It will remember it for future sessions.

## Running the program

The companion embeds a heavily modified version of AppleWin 1.29.16, and will automatically load Deathlord after you select the boot image file.
You can optionally load the profile of your choice (they're located in the Profiles directory), and play the game.

Ther are a number of emulation options, all directly accessible from the main menu bar. Playing at max speed automatically disables sound, and it will generally play so fast that you won't see battle animations. This is highly discouraged unless you've become very adept at the game.

## Profiles

The key feature of the Companion is its profiles. A profile is a JSON document that specifies what the Companion should display, and where. The Companion has support for many types of data in memory, including being able to translate numeric identifiers into strings (something very useful when you want to show "Short Sword +1" instead of 0x0b).

The documentation for profiles is sorely lacking, but I've included a base profile that will autoload upon launch. Feel free to experiment and ping me for more info.

## Logging

LOGGING ISN'T YET FUNCTIONAL FOR DEATHLORD
The Companion automatically logs all conversations.
You can show the log file from the menu. You can copy, paste, or otherwise modify the text within the log window. You can (also from the menu) load and save the log.

WARNING: Save or copy the log window content somewhere else before you quit the app or it is lost forever!

## Other unique things

There's a hack window where you can modify a byte of memory, and save the current map to disk in the Maps/ folder.
Also the current map will save if you hit PAGEDOWN.

Finally, there's a tileset generator that will grab visible tiles that haven't yet been seen by the program and will append them to a tileset that is saved on disk inside the Maps/ folder. Contact me for more info.
The tileset generator is started with NUMPAD 1. It acquires tiles with NUMPAD 0. Save the file with NUMPAD 9. Reset the whole file with NUMPAD 5.

Happy retro RPG gaming, and see you on the Lost Sectors Discord server.

Rikkles, Lebanon, 2021.


@rikretro on twitter
https://github.com/hasseily/Deathlord-Companion
