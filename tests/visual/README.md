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
creates the default 1280x900 SDL/OpenGL window, composes the ImGui menu and
centered Apple //e window, captures the GL backbuffer, and exits after 600
deterministic emulator frames. The Apple image is the 560x384 borderless
display at exact 2x size (1120x768), with a permanent two-line hint reserve.
The approved unprocessed macOS capture has SHA-256:

```
f2bc416580e72dd0b7ce95528a729a253a97ad509fc865a02cc548f928b6b340
```

The capture also verifies the v2 context hint below the Apple //e image. At
the title it reads `Press any key`; after the input checkpoint the menu hint
reads `Choose 'U', 'C', or 'P' to continue`. Character-creation and autoroll
states use the corresponding original v2 strings.

`--ui-fixture` freezes emulation, seeds a deterministic six-character RAM
fixture, and renders the portable modern UI without requiring a save game. It
validates asset loading, the 1904x1041 logical canvas, aspect-preserving
scaling, portrait source rectangles, party-stat formatting, cached 64x64 RAM
automap composition, minimap fog/pin rendering, daytime/moon sprites, and the
host text buffers. The approved 1280x900 macOS backbuffer is:

```
SHA-256:       59baa7f37a2138f913dc7fe7f7e3b0e61c114d9aa95525cb36aa9ec97af0cf94
visible state: 2x FollowPlayer map with v2 pulsing corner cursor, numbered full party cards with native-pixel shadowed portrait statuses and all eight inventory rows, 32-row log, centered billboard, minimap, clock, Deathlord bitmap text
```

`--ui-fixture-mode` selects presentations that would otherwise require a live
save at a very specific game state. All use the same runtime renderer and
hook-facing state as real play:

```
mode       SHA-256                                                        visible state
battle     52af8022e5d0f218f921a86023a3d625ae522d16bd60e9f22b450360a77bb852  actor formation, bars, active actor, disabled enemies, authentic rule glyph
inventory  7c381b5207ea3d045dc0cab4c67f875db027b35b8a6db498d4198edbdaf1d98d  pixel-aligned v2 SpriteFont/geometry, integer-centered headers, layered ownership markers
loading    4603dbd283c6eaef8701e2a3f7b7f371ae6df24cf73ff34372a11a00162ede44  full loading artwork and Deathlord-font prompt
gameover   6cf3ab71981b21d3dd48f4d741c6d16bf266e48926788f96cb67a17069ecfd41  alpha-composited death artwork and reboot prompt
```

The standalone v2 tools also have deterministic captures (smoke mode ignores
the user's saved ImGui layout):

```
spells: f34c16a5e4cc67244734a56c3d00ff18ed5e2ba12b8bbb7728919c9f7fddd756
log:    03f89d172383ccdfdba1834a9cd8b4c889dd42f80546f11b114f0f299da21db3
```

The large-window command requests 1920x1080. On the development Mac SDL clamps
that to its 1800x1080 usable display area. The inspected current baseline is
`a5d256ef5068bf9d027171ecfe17061075371636268776807cd4799ea6a0cfb0`.

`sdl_live_game_smoke` is the gameplay truth test. It uses a temporary copy of
the ignored release HDV, schedules the real title/menu keys, and reaches the
bundled ATEAM party in INDOO at PC `$4AEF`. The 1800x1080 host capture proves
the map uses real RAM rather than fixture terrain and that all six cards render
class/race, six attributes, status, and all eight live inventory/equipment
rows (empty slots are retained as dotted rows, matching v2):

```
SHA-256:       642164180aa4fd8cbec85d8c5cf390e533bb2ad46c7dd1983b6832b5a1e5f655
visible state: real INDOO town at 2x FollowPlayer and numbered bundled ATEAM party using the original Deathlord font
```

`sdl_startup_splash_smoke` follows the same accelerated HDV path but omits
Space. It verifies that the emulator pauses only once the main map is ready,
leaving the v2 Relorded credits artwork and its `PRESS SPACE` prompt visible:

```
SHA-256:       bbe4ac28dee0bd00f909c5eccfc96969c86d1063569d48b9c78d04fe51a9ec0b
visible state: Relorded credits splash, ready for Space, with no pause overlay
```

The prompt alternates once per second between the regular and inverse
Deathlord glyph atlases, matching v2. A separately inspected inverse-phase
capture has SHA-256
`5e8f60acbe5282ef4b0315e7fc2ddd437d10531313775639426240f0db23552f`.

`sdl_original_ui_postprocess_smoke` verifies that the F11 original-interface
overlay consumes the postprocessor output texture, not the raw Apple
framebuffer. It also verifies v2 layering: the 2x borderless screen and amber
frame are above an 85%-black curtain covering the complete modern interface.
The curtain darkness is configurable from 0-100% and persisted. The capture is
`3f435b522d60dcbe6ef920148d6838e51fa1b72303b946c017727fd1eb752a2d`.

The ordinary gameplay fixture was also inspected fullscreen at 1800x1130.
Near the 1904x1041 native canvas size, card glyphs use an unscaled 1x pixel
grid while their anchors follow the fitted layout. The capture
`3a4b4c7e1a4b5533127465c39ee771aa9b088aa3d804478e6911521f415a0689`
verifies complete top rows on character names and complete bottom rows on the
`P` lines. The same native grid is used by the log, billboard, and keypress
buffers so their top and bottom glyph rows remain intact as well. The fixture
fills all 32 available log rows, and the lower billboard strings are centered.
An extended-frame fixture capture also verifies that the original four-corner
avatar cursor changes size over its v2 ten-step pulse. Portrait status labels
remain 14x16 native pixels at every window scale and carry a one-pixel
lower-right black shadow.

The inventory overlay was additionally inspected in a synchronized macOS
fullscreen capture (1800x1130 host, aspect-preserving 1904x1041 canvas). Its
SHA-256 is `bb1605d084bea2d7698f3d33126f1ba87c6936931e7949085c773f59785dce3e`.
A second deterministic hover capture verifies that both exchange lines pass
through the marker centers and render behind their dots; its SHA-256 is
`5d60e69a00fa749a41ae80e3eaa1ac4d6c047d9017ecdc75ac4a38771a2999ac`.
The shared `Tanto` fixture/reference sample is also checked manually at native
scale: all 122 lit glyph pixels match the v2 screenshot exactly.

The File-menu party tools have three bounded UI fixtures. They render the live
editor over the same six-member memory fixture, the clean-HDV destructive
confirmation, and a validated import confirmation containing the decoded party
name and fixed UTC timestamp. Their inspected SHA-256 baselines are:

```
party editor:        10a1b9395d750d4baaaf7c5f16d710acdcf827f73f15c97894d2f6b22e127251
clean confirmation:  afc3ad5fe3afc1b40f45726dde4a450cf745086a2ab1f1b52e162fcc10aab936
import confirmation: 684203862f21d110d3382c3c791d16eed9aac9dd608dcb58ad3ebec2ac012a85
```

The nonvisual hook test also round-trips all `$FD00-$FFFF` party bytes through
the versioned JSON format, rejects unrelated JSON, and replaces a synthetic
active HDV from a clean template without modifying the template.
