# Visual regression fixtures

The private DLRL HDV is not tracked. `hdv_boot_capture` copies the ignored
source image to a temporary working image, executes 600 emulated frames, and
writes `build-mac/Testing/dlrl-boot.bmp`.

For the 2.0.1 HDV identified in the multiplatform roadmap, the approved boot
checkpoint is:

```
PC:               $1D3E
framebuffer:      600x420 BGRA, bottom-up
framebuffer FNV:  972f16197c8c6331
BMP SHA-256:      373b703e994fe5b1d954998d156f55ae4d69b0aab2c7d19ab58a169ebc72780a
visible state:    original Deathlord title screen
```

The input checkpoint queues one printable Apple //e key at frame 800 and runs
to frame 1800:

```
PC:               $1C27
framebuffer FNV:  466f292c38add1a7
BMP SHA-256:      66ea5affd533ee4065e0f9d9bff43a5662c6d36a18ebfc5a430b1ac34c761af0
visible state:    Deathlord main menu
```

The test skips with CTest code 77 when the private HDV is absent. A visual
change is accepted only after opening the new capture and updating these values
and the CTest expectation together.

The real-window smoke gate is enabled with `-DDLRL_ENABLE_GUI_TESTS=ON`. It
creates a 1024x768 SDL/OpenGL window, composes the ImGui menu and Apple //e
window, captures the GL backbuffer, and exits after 600 deterministic emulator
frames. The hook-enabled DLRL 3.0 shell capture adds the Relorded menu. Its
approved unprocessed macOS capture has SHA-256:

```
7edafa49de5f90ebeecbbf97ac4c180f8f53a770841553257d45b98d853c1613
```

`--ui-fixture` freezes emulation, seeds a deterministic six-character RAM
fixture, and renders the portable modern UI without requiring a save game. It
validates asset loading, the 1904x1041 logical canvas, aspect-preserving
scaling, portrait source rectangles, party-stat formatting, cached 64x64 RAM
automap composition, minimap fog/pin rendering, daytime/moon sprites, and the
host text buffers. The approved 1024x768 macOS backbuffer is:

```
SHA-256:       1b9c9a12708ab45bb97ac678fa23706fc593b481fcdc88b42c904a2a74f575c6
visible state: 2x FollowPlayer map, numbered full party cards, minimap, clock, Deathlord bitmap text
```

`--ui-fixture-mode` selects presentations that would otherwise require a live
save at a very specific game state. All use the same runtime renderer and
hook-facing state as real play:

```
mode       SHA-256                                                        visible state
battle     07c878586e47cb74443921f150bdb0227c4eb4c9bf76378e0a34017afaab1191  actor formation, bars, active actor, disabled enemies, authentic rule glyph
inventory  ce32878cc9e16c2bc3efccd398c99a97c67647e8954cb3f6ea4b82eca56e96f6  pixel-aligned v2 SpriteFont/geometry, integer-centered headers, layered ownership markers
loading    b05c0449ca596e2eec9dee3b5d154e5d8743c08b0cf3687fc4a7e0ede573a81f  full loading artwork and Deathlord-font prompt
gameover   d3cb0495f71f6a913f6d7b0ec4044f25ad8b76986e74813cbed546990f0cdfeb  alpha-composited death artwork and reboot prompt
```

The standalone v2 tools also have deterministic captures (smoke mode ignores
the user's saved ImGui layout):

```
spells: 6fe8b43d3b897a9b246bbaac90700bc4b39da959380ed6dc4987c4beaee88852
log:    4132c66778e19c9618c4cb7d71abb95083633d9522b452733c2fb916498a3aee
```

The large-window command requests 1920x1080. On the development Mac SDL clamps
that to its 1800x1080 usable display area. The inspected current baseline is
`783937cc8acdbe43d3a9d5ac18c5f52cda26a3a034c611d21a354b0f48d1d263`.

`sdl_live_game_smoke` is the gameplay truth test. It uses a temporary copy of
the ignored release HDV, schedules the real title/menu keys, and reaches the
bundled ATEAM party in INDOO at PC `$4AEF`. The 1800x1080 host capture proves
the map uses real RAM rather than fixture terrain and that all six cards render
class/race, six attributes, status, and live inventory/equipment state:

```
SHA-256:       e5ce2ee0726657384aceb3a75f923b43f36d377406659576e773aa74abc2452e
visible state: real INDOO town at 2x FollowPlayer and numbered bundled ATEAM party using the original Deathlord font
```

`sdl_original_ui_postprocess_smoke` verifies that the F11 original-interface
overlay inside the modern canvas consumes the postprocessor output texture,
not the raw Apple framebuffer. Its deterministic fixture capture is
`63a5aa866fd317471bda0dea2059dbcec4cd49e70ce4509c68dce66b6af63ced`.

The inventory overlay was additionally inspected in a synchronized macOS
fullscreen capture (1800x1130 host, aspect-preserving 1904x1041 canvas). Its
SHA-256 is `bb1605d084bea2d7698f3d33126f1ba87c6936931e7949085c773f59785dce3e`.
A second deterministic hover capture verifies that both exchange lines pass
through the marker centers and render behind their dots; its SHA-256 is
`5d60e69a00fa749a41ae80e3eaa1ac4d6c047d9017ecdc75ac4a38771a2999ac`.
The shared `Tanto` fixture/reference sample is also checked manually at native
scale: all 122 lit glyph pixels match the v2 screenshot exactly.
