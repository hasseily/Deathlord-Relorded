# DLRL 3.0 architecture

DLRL 3.0 embeds the same stripped, portable AppleWin core used by Nox Archaist
Companion. The emulated machine is an Enhanced Apple //e with 128 KiB RAM,
SmartPort hard disk in slot 7, and Mockingboard in slot 4. There is no Disk II
card or floppy frontend.

```
DLRL UI, renderer, and game-specific hooks
                    |
SDL3 window/input/audio/timing + ImGui + OpenGL 4.1
                    |
portable AppleWin core + SmartPort HDV
```

## Current targets

- `dlrl_emulator`: AppleWin CPU, memory, video, sound, SmartPort, ROM loading,
  and its small POSIX Win32-type shim.
- `dlrl_platform`: FrameBase implementation, SDL audio ring-buffer adapter,
  DLRL hook dispatcher, and portable inventory rules.
- `dlrl_pp`: NAC's OpenGL CRT/postprocessing pipeline.
- `dlrl`: SDL callback application, ImGui shell, OpenGL texture ownership, and
  the aspect-preserving modern DLRL reference canvas.
- `dlrl_core_smoke`: ROM/RAM/video lifecycle test.
- `dlrl_hooks_tests`: CPU-boundary and deterministic gameplay-fix tests.
- `dlrl_capture`: bounded HDV boot/input runner and deterministic framebuffer
  capture tool.

## Runtime data

CMake stages Apple //e ROMs, GLSL shaders, postprocessor presets/assets, DLRL
presentation assets, and the complete tracked `assets/Images` tree beside the
executable or into the macOS bundle's `Contents/Resources`.

On first launch, the bundled clean HDV is copied to the directory returned by
`SDL_GetPrefPath("Rikkles", "DeathlordRelorded")`. Normal play therefore never
mutates the pristine tracked or packaged template. Explicitly opened HDVs are
used in place, because they are user-selected save images.

## Visual test path

`dlrl_capture` validates the raw 600x420 bottom-up BGRA Apple //e framebuffer.
The `dlrl` executable's bounded smoke mode also reads back the complete OpenGL
backbuffer, including ImGui, so platform and composition failures are visible.
Approved checkpoints live in `tests/visual/README.md`.

The `--ui-fixture` path seeds representative party RAM and freezes the CPU. Its
`--ui-fixture-mode battle|inventory|loading|gameover` variants drive the same runtime
overlay paths from deterministic state. Together they exercise modern
asset/layout rendering independently of the clean HDV and save-game state.
The automap reads Deathlord's 64x64 RAM map and composes native 28x32 sprites
into one cached 1792x2048 RGBA texture only when the map signature changes.
Linear filtering scales it into the legacy 896x1024 viewport, matching the old
DX12 path; ImGui submits the result as one image rather than thousands of
per-frame tile primitives.
Battle actor state is carried by typed hook events; loading and death use the
hook runtime flags. The application layer never calls renderer code from the
6502 interpreter.

Inventory remains host-owned, matching DLRL 2.0.1: the Relorded ImGui menu or
Cmd/Ctrl-I toggles it, the overlay reads party inventory RAM, and the host
consumes input while it is open. Its catalog names and slot metadata come from
the existing `InventoryList.csv` parser.

## Hook boundary

The old interpreter called frontend singletons directly. DLRL 3.0 instead uses
a narrow hook dispatcher owned by the DLRL layer. Emulator traps may inspect
registers and memory, apply deterministic gameplay mutations, and emit typed
presentation events; they may not include ImGui or renderer headers.
