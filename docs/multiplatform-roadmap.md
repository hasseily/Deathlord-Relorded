# DLRL SDL3 multiplatform roadmap

## Goal

Ship Deathlord Relorded 3.0 from one CMake codebase on Windows, macOS, and Linux.
The application keeps its embedded Apple //e architecture and game-specific
hooks, but replaces the Win32/DirectX 12 frontend with the proven architecture
from Nox Archaist Companion (NAC):

```
DLRL application and game hooks
        |
SDL3 + ImGui + OpenGL platform layer
        |
portable, stripped AppleWin core + SmartPort HDV
```

The emulator is HDV-only. Disk II, floppy images, floppy menus, floppy backup
code, and floppy resources are outside the supported design.

## Non-negotiable constraints

- Preserve the behavior of the original DLRL CPU and memory hooks.
- Boot only the DLRL HDV through the SmartPort hard-disk card.
- Use SDL3 for windowing, events, timers, audio, dialogs, and platform paths.
- Use OpenGL for rendering and Dear ImGui for menus, panels, and tools.
- Keep a single top-level CMake build for all supported platforms.
- Treat NAC's current portable emulator and platform code as the reference
  implementation; do not invent a second portability layer.
- Make every milestone runnable or independently testable.
- Keep the possible late-game protection behavior (reported as missing major
  bosses) separate from the platform migration. The HDV itself boots and enters
  gameplay normally; protection is not a boot acceptance condition.

## Target source layout

```
CMakeLists.txt
docs/
src/
  main.cpp                 SDL callback lifecycle and application orchestration
  Frame.{cpp,h}            AppleWin FrameBase implementation and ROM loading
  Renderer.{cpp,h}         SDL window, OpenGL context, framebuffer, ImGui
  AudioOutput.{cpp,h}      SDL audio backend for AppleWin sound buffers
  AppState.{cpp,h}         runtime state without renderer or OS globals
  EmulatorHost.{cpp,h}     init, HDV insertion, reset, pause, stepping
  Input.{cpp,h}            SDL-to-Apple //e key mapping
  Settings.{cpp,h}         JSON persistence under SDL_GetPrefPath
  Texture.{cpp,h}          portable image/texture ownership
  SpriteRenderer.{cpp,h}   reference-canvas sprites, lines, and bitmap fonts
  DlrlHooks.{cpp,h}        frontend-facing DLRL game state and callbacks
  ui/                      ImGui menus and auxiliary panels
  game/                    automap, party, overlays, animations, inventory
Deathlord-Relorded/
  Emulator/                NAC-derived portable AppleWin core, HDV-only
  Assets/                  runtime game assets
third_party/               NAC-derived glad/glm/stb/json/ImGuiFileDialog
assets/pp/                 NAC postprocessor assets and shaders
tests/
  unit/                    pure logic and data-format tests
  integration/             emulator lifecycle and HDV boot tests
  visual/                  capture manifests and golden-image tooling
```

The old Win32/DX12 frontend remains compile-excluded while behavior is being
ported. It is deleted after its final feature has a portable replacement.

## Implementation status (July 2026)

- Milestones 0-3 are complete on macOS: portable NAC-derived AppleWin, HDV-only
  SmartPort boot, deterministic captures, SDL3 audio/input/windowing, OpenGL,
  ImGui, settings, and the NAC postprocessor all build and run.
- Milestone 4 is substantially complete: the emulator exposes a generic CPU
  hook ABI; the DLRL layer owns the address table, typed presentation events,
  16 persistent Relorded switches, auto-reroll state, gameplay mutations, and
  portable `InventoryList.csv` rules. Live-game HDV address reconciliation
  remains part of later save-game and boss-integrity investigation.
- Milestone 5 is substantially complete: portable textures, the original
  1904x1041 DLRL content canvas, background, optional Apple view, six
  RAM-driven party cards, cached 64x64 automap, minimap fog/pin, daytime/moon,
  and hook-fed text windows render at multiple host sizes. Exact legacy bitmap
  font metrics and the remaining marker controls are still pending.
- Milestones 6-7 are in progress: emulator/audio/video/interface menus and the
  v2 File/Emulator/Relorded/Help surface and full Relorded switches are ported.
  Inventory, spell reference, and long-form log are accessible through their
  original menu concepts and shortcuts. Deterministic fixtures now cover the
  base UI plus battle, loading, game-over, inventory, spell, and log
  presentations. Inventory currently has a
  live-RAM, host-input-consuming read-only presentation; item
  movement/stash/discard behavior, log file load/save, floating combat text,
  and complete fog/marker controls remain.
- The NAC-shaped CI/package workflow is now present for Linux, Windows, and
  macOS, with HDV-free core/hook tests and draft releases on version tags. Its
  first remote matrix run, legacy cleanup, Gamelink decision, signing, and
  release validation remain pending in Milestones 8-9.

## Milestone 0 - foundation and audit

Deliverables:

- This roadmap and an explicit feature inventory.
- Record every DLRL modification currently embedded in AppleWin's CPU, memory,
  video, disk, keyboard, and audio code before replacing that core.
- Identify runtime assets and distinguish distributable data from local/private
  game images.
- Preserve the original code in Git history; do not maintain a second live copy.

Acceptance:

- No DLRL hook can disappear merely because the emulator donor tree replaces
  the old files.
- The intended HDV path and local test-data policy are documented.

## Milestone 1 - portable AppleWin and build system

Deliverables:

- Replace the placeholder `SDLOpenGLCross` CMake file with the NAC target
  structure.
- Vendor NAC's portable AppleWin core, libwindows POSIX shim, yaml, zlib,
  minizip, ROM loader, Linux sound ring buffer, and platform keyboard/joystick
  implementations.
- Remove Disk II source files and exclude `DISK2.rom` from runtime staging.
- Build `dlrl_emulator` as a static library on macOS first.
- Add a tiny `dlrl_core_smoke` executable that initializes and destroys the
  machine without SDL video.

Acceptance:

- `cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Debug` configures.
- `cmake --build build-mac --target dlrl_emulator dlrl_core_smoke` succeeds.
- `ctest --test-dir build-mac -R core_smoke` succeeds.
- No target compiles `Disk.cpp`, `DiskImage` floppy paths, or any legacy DX12
  frontend file.

## Milestone 2 - HDV boot and deterministic capture

Deliverables:

- Add an `EmulatorHost` that configures an Enhanced Apple //e, language card,
  Mockingboard, No-Slot Clock, and SmartPort card in the same slots as NAC.
- Insert the supplied DLRL HDV read/write through `HarddiskInterfaceCard`.
- Add a bounded headless runner: initialize, execute a requested number of
  cycles/frames, capture diagnostics, then exit without hanging.
- Export the raw Apple //e framebuffer to a simple deterministic image format.
- Record CPU registers, selected memory signatures, active card configuration,
  and a framebuffer checksum with each capture.

Acceptance:

- Missing or invalid HDVs fail with a clear nonzero result.
- A valid DLRL HDV reaches a stable visible state without a crash.
- Re-running the same capture produces the same framebuffer checksum once
  timing inputs are fixed.
- The captured image can be opened and inspected during development.
- The release HDV reaches real gameplay and a live modern-UI capture without
  modifying the pristine source image.

## Milestone 3 - SDL3/OpenGL/ImGui shell

Deliverables:

- Port NAC's SDL callback lifecycle, renderer, Frame implementation, OpenGL
  framebuffer upload, ImGui backends, and postprocessor.
- Display the Apple //e framebuffer in an ImGui viewport.
- Port SDL keyboard handling, pause/reboot/speed controls, fullscreen behavior,
  and HDV open dialog.
- Port SDL audio output for speaker and Mockingboard.
- Persist window, audio, speed, video, and HDV settings under
  `SDL_GetPrefPath`.
- Add `--smoke-frames N --capture PATH --quit` for bounded real-window tests.

Acceptance:

- The app opens a real SDL window, displays the same image as the headless
  capture, accepts Apple //e keyboard input, produces audio, and exits cleanly.
- The bounded smoke mode can run unattended and saves a screenshot.
- At least one native macOS run is visually inspected after each renderer
  change; Linux CI later repeats the window smoke test under a virtual display.

## Milestone 4 - DLRL emulator hooks

Deliverables:

- Reapply the original DLRL program-counter and memory hooks to the portable
  CPU core.
- Move frontend notifications behind narrow callbacks or an event queue so the
  emulator never includes UI headers.
- Reapply memory-layout assumptions, Deathlord charset translation, map state,
  battle state, party state, reroll behavior, gameplay fixes, and configurable
  Relorded changes.
- Reconcile hooks against the HDV version of the game. Keep all addresses in a
  versioned `DlrlConstants` structure rather than scattered preprocessor
  definitions where practical.

Tests:

- Pure tests for character decoding, class/race rules, XP allocation, item
  restrictions, map coordinate conversion, and every configurable gameplay
  switch that can be isolated from the CPU.
- CPU integration tests that seed RAM/registers, enter representative trap
  points, and assert the resulting RAM/register/event changes.
- Regression tests for hooks that intentionally suppress or replace original
  6502 behavior.

Acceptance:

- Every hook in the inventory has a ported implementation, an explicit removal
  decision, or a deferred late-game-integrity ticket.
- Hook tests run without SDL or OpenGL.

## Milestone 5 - portable modern renderer

Deliverables:

- Establish the original 1904x1041 content canvas within the 1920x1080 host
  design, with aspect-preserving host scaling and coordinate conversion.
- Add portable texture loading for existing PNG/JPG/BMP assets.
- Replace DirectXTK SpriteBatch and PrimitiveBatch calls with a small OpenGL or
  ImGui draw-list renderer supporting sprites, source rectangles, tint,
  opacity, rotation, lines, triangles, and render ordering.
- Decode the existing DirectXTK `.spritefont` atlases or regenerate equivalent
  portable bitmap-font data; visual metrics must match before deleting them.
- Replace the HLSL blur/composite/interference paths with the NAC
  postprocessor or focused GLSL equivalents only where the effect is still
  visible and intentional.

Acceptance:

- Background, Apple //e video, party layout, minimap, daytime display, and text
  output render in their correct reference-canvas locations.
- Mouse hit-testing remains aligned at multiple window sizes and fullscreen.
- Golden captures cover 1920x1080 and at least one non-16:9 host size.

## Milestone 6 - application UI and tools

Deliverables:

- Recreate the Win32 menu resource as an ImGui main menu.
- Port the hacks, log, and spell windows to ImGui panels.
- Port About/error/confirmation flows to ImGui or SDL dialogs.
- Port JSON settings and map-marker persistence using portable filesystem APIs.
- Replace scenario-floppy backup/restore with HDV backup/restore if still
  desired; do not carry floppy terminology or two-disk assumptions forward.
- Port English-name toggle, original-interface opacity, speed/video/audio
  controls, and visibility toggles.

Acceptance:

- Every currently documented menu command has an equivalent or an explicit
  HDV-era removal decision.
- Settings survive restart on all three path conventions.
- No application-layer source includes `Windows.h`.

## Milestone 7 - game presentation features

Deliverables:

- Port inventory management and inventory overlay.
- Port automap, fog, footsteps, hidden markers, quadrant/follow modes, and map
  export.
- Port battle, loading, and game-over overlays.
- Port sprite and floating-text animation managers.
- Port party portraits/stats, minimap, daytime/moon phases, long-form log, and
  spell reference presentation.

Acceptance:

- Each feature has at least one deterministic state fixture and visual capture.
- A scripted RAM fixture can render every overlay without booting the game.
- Interactive tests confirm overlays consume input without double-feeding the
  Apple //e keyboard.

## Milestone 8 - Gamelink, robustness, and cleanup

Deliverables:

- Reuse NAC's platform-neutral Gamelink protocol and Win32/POSIX/disabled
  backends if DLRL still exposes the integration.
- Keep the Gamelink wire struct byte-identical and test its size/offsets.
- Add structured startup diagnostics for ROMs, HDV, renderer, audio, and hooks.
- Delete the DX12 renderer, Win32 dialogs, `.sln`/`.vcxproj`, HLSL shaders,
  NuGet packages, floppy sources/resources, and obsolete resource scripts after
  all replacements are accepted.

Acceptance:

- A repository search finds no live DirectX 12 or legacy Win32 frontend path.
- The portable build is the only supported build.
- A clean clone needs only CMake, a compiler, and platform development headers.

## Milestone 9 - CI, packaging, and release validation

Deliverables:

- GitHub Actions builds and tests macOS, Windows, and Linux.
- Linux runs unit/integration tests and the SDL smoke test under Xvfb.
- macOS produces a signed-ready `.app` layout with bundled resources.
- Windows stages SDL3 and runtime resources next to the executable.
- Linux produces a relocatable archive initially; AppImage can follow if
  distribution demand justifies it.
- Release artifacts never contain the private DLRL HDV unless distribution
  rights and policy explicitly permit it.

Acceptance matrix:

| Gate | macOS | Windows | Linux |
|---|:---:|:---:|:---:|
| Configure and compile | required | required | required |
| Unit tests | required | required | required |
| Emulator lifecycle | required | required | required |
| HDV boot/capture | local/private | local/private | local/private |
| SDL bounded smoke | required | required | required |
| Visual golden comparison | required | required | required |
| Manual playthrough checkpoint | required | required | required |

The manual checkpoint covers boot, new/load game, movement, combat, inventory,
automap, save, reboot/reload, audio, pause, fullscreen, and clean shutdown.

## Visual verification workflow

The port must be inspectable without relying on an indefinitely running GUI:

1. `dlrl_capture` renders the emulator framebuffer after a bounded run.
2. UI fixtures render known RAM/application states without the HDV.
3. `dlrl --smoke-frames ... --capture ... --quit` verifies the real SDL/OpenGL
   presentation path.
4. Captures are opened during development and compared with approved goldens.
5. Pixel-perfect regions use exact comparison; postprocessed regions use a
   small per-channel tolerance and a changed-pixel threshold.
6. Any intentional visual change replaces its golden in the same commit with a
   short explanation.

## Known risks and isolated decisions

- **Possible late-game protection:** boot and normal gameplay are confirmed.
  Separately verify the reported possibility that major bosses disappear; do
  not infer a boot-time protection failure from that unconfirmed symptom.
- **DLRL hook density:** inventory before replacing CPU files; hook tests before
  visual feature work.
- **DirectXTK bitmap fonts:** decode or regenerate once, then lock metrics with
  text-layout captures.
- **Legacy renderer coupling:** port behavior by feature rather than trying to
  make DirectX types compile on other platforms.
- **OpenGL on macOS:** follow NAC's working 4.1-core path despite deprecation;
  changing graphics APIs is outside this migration.
- **Private game data:** tests must skip cleanly when the HDV is absent, while
  public emulator/unit tests always run.

## Local private test fixture

The author's Windows 2.0.1 release is available only in the ignored directory
`extras/deathlord-relorded-win-201/`. The development HDV is:

```
extras/deathlord-relorded-win-201/Images/Deathlord PRODOS.hdv
size:   819200 bytes (1600 ProDOS blocks)
volume: /DEATHLORD
sha256: f25f6174554c3341739f7b6d3026d56b375255cfab98ddd0ba4dcac70a723a6d
```

Tests discover it through `DLRL_HDV`, with that path as a source-tree-local
developer fallback. The fallback is never staged or packaged.

## Definition of done

DLRL configures, builds, tests, launches, renders, accepts input, produces audio,
loads and saves through its HDV, and packages successfully on Windows, macOS,
and Linux. The complete modern UI and all accepted Relorded gameplay changes
match the legacy build closely enough to pass approved functional and visual
regressions. No floppy or DirectX 12 frontend remains in the supported tree.
