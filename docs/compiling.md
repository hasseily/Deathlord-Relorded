# Compiling DLRL 3.0

DLRL uses CMake 3.20+, C++17, SDL3, OpenGL, and Dear ImGui. CMake uses an
installed SDL3 when available and otherwise fetches the same pinned SDL release
as NAC. ImGui is fetched at configure time.

## macOS

```sh
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mac -j8
ctest --test-dir build-mac --output-on-failure
open build-mac/dlrl.app
```

When a sibling NAC build already populated ImGui, local development can avoid a
network fetch:

```sh
cmake -S . -B build-mac \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$PWD/../NoxArchaistCompanion/build-mac/_deps/imgui-src"
```

The private HDV tests discover
`extras/deathlord-relorded-win-201/Images/Deathlord PRODOS.hdv` by default or
accept `-DDLRL_PRIVATE_HDV=/path/to/Deathlord.hdv`. They skip when it is absent.

Native visual tests are opt-in because they open a real window:

```sh
cmake -S . -B build-mac -DDLRL_ENABLE_GUI_TESTS=ON
ctest --test-dir build-mac -R sdl_window_smoke --output-on-failure
```

To run the bounded real-game modern-UI validation:

```sh
ctest --test-dir build-mac -R sdl_live_game_smoke --output-on-failure
```

The app itself also provides a bounded visual-development mode:

```sh
build-mac/dlrl.app/Contents/MacOS/dlrl \
  --smoke-frames 600 --capture build-mac/Testing/dlrl-host.bmp
```

The modern renderer has a deterministic RAM fixture for asset/layout work:

```sh
build-mac/dlrl.app/Contents/MacOS/dlrl \
  --smoke-frames 3 --ui-fixture --window-size 1920x1080 \
  --capture build-mac/Testing/dlrl-modern-ui.bmp
```

Battle, loading, and game-over presentations have equally bounded fixtures:

```sh
build-mac/dlrl.app/Contents/MacOS/dlrl \
  --smoke-frames 3 --ui-fixture-mode battle \
  --capture build-mac/Testing/dlrl-battle-ui.bmp
```

Replace `battle` with `inventory`, `loading`, or `gameover` for the other states.

## v2-compatible controls

- `Insert`: inventory
- `F1`: toggle 2x FollowPlayer / full map
- `F2`–`F5`: fixed 2x map quadrants
- `F10`: original/English item names
- `F11`: original Apple //e interface
- `Alt-S`: spell reference
- `Alt-L`: game log

Emulator speed is deliberately not user-selectable. Normal interaction is
locked to 1x; DLRL hooks request temporary acceleration for boot, Play Game,
map loading, and automatic attribute rerolls, then restore 1x.

## Continuous integration and packages

`.github/workflows/cmake.yml` follows the NAC matrix on Linux, Windows, and
macOS. It builds SDL3 from the pinned FetchContent source, runs the HDV-free
core/hook suite (private-HDV tests skip with code 77), and creates a portable
archive for each platform. A version tag such as `3.0.0-rc1` uploads those
archives into a draft prerelease; no HDV is included.
