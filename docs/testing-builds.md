# Deathlord Relorded 3.0 — test version

This is the complete playable game, not a time-limited demo. It is a prerelease
for testing; please back up important parties and report any problems. The
stable 2.x download remains available separately on itch.io.

Extract the entire archive before launching. Keep all the folders and libraries
beside the executable; copying only the executable will not work. `BUILD-INFO.txt`
identifies the exact source commit and clean game image included in the package.

## Windows x64

Run `dlrl.exe` from the extracted folder. SDL3 is included and the Visual C++
runtime is statically linked. These test builds are unsigned. A working OpenGL
graphics driver is required.

## Linux x64

Built on Ubuntu 24.04; Ubuntu 24.04 or a compatible newer distribution is the
target for testing. This is not a universal binary for older Linux systems.
SDL3 is included; a graphical desktop and working OpenGL graphics driver are
required. From the extracted directory, run:

```sh
./dlrl
```

If it will not start, launch it from a terminal and include the error output in
your report. `ldd ./dlrl` and `ldd ./libSDL3.so.0` help identify missing libraries.

## macOS — Intel and Apple Silicon

Requires macOS 11 or newer. Extract the ZIP and open `dlrl.app`; you can move the
app to Applications. SDL3 and all game resources are inside the app bundle.

The test app is ad-hoc signed for integrity, but is not Developer ID-signed or
Apple-notarized. macOS may block the first launch. If you trust this download,
follow [Apple's instructions for opening an unnotarized app](https://support.apple.com/102445)
using the per-app **Open Anyway** option in Privacy & Security. Do not disable
Gatekeeper globally.

## Game image and saves

The package includes the original clean HDV from Git history, not a developer's
saved party. Create a party through the game's normal menus before trying the
party editor or teleport tools.

Press a key at the title screen, then use the original game's **Character
Options** to create characters and assemble a six-character party. Return to
the main menu and choose **Play Game**; press Space when prompted. Everything
is on the included hard disk image; no separate game download or disk swapping
is required. The original game manual (commands, spells, races and classes) is
available in the documentation download on the
[itch.io game page](https://rikkles.itch.io/deathlord-relorded).

On first launch DLRL copies the template into per-user storage; normal play does
not modify the packaged image. An existing installation keeps its existing save.
To try a fresh game, use **File > Start a clean new game** and read the confirmation
before proceeding. Export any party you want to keep first.

## Features to test

- **Relorded > Map > Fog of War**: toggle visibility fog off and on.
- **Relorded > Map > Teleport to Map...**: choose a map/floor and X/Y coordinates.
  Available at the map movement prompt, outside combat.
- **Hacking > Party Editor...**: edit an active party.
- **Hacking > Invincible**: prevent HP loss.
- **Hacking > Automatic Battle Success**: win battles automatically.
- **Relorded > Relorded Changes**: existing gameplay fixes.

If the menu bar is hidden, move the pointer to the top of the window. These tools
change gameplay and can affect your saved party; use a test party or export a
backup. Please include `BUILD-INFO.txt`, your OS/GPU, reproduction steps, and a
screenshot or terminal output when reporting problems.
