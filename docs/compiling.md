# Compiling DLRL 3.0

DLRL uses CMake 3.20+, C++17, SDL3, OpenGL, and Dear ImGui. CMake uses an
installed SDL3 when available and otherwise fetches the same pinned SDL release
as NAC. ImGui is fetched at configure time.

The canonical clean HDV and original master images are tracked in
`assets/Images`. CMake stages that complete directory into every runnable
build and package. CI verifies the checked-in image hashes before building;
normal play copies the clean HDV to per-user storage and never mutates the
source or packaged template.

## Windows

Visual Studio 2022 or 2026 with the Desktop C++ workload is supported. From a
Developer PowerShell:

```powershell
cmake -S . -B build-win -G "Visual Studio 18 2026" -A x64
cmake --build build-win --config Release --parallel
ctest --test-dir build-win -C Release --output-on-failure
build-win\Release\dlrl.exe
```

Use `Visual Studio 17 2022` as the generator when building with VS 2022. CMake
copies `SDL3.dll` and all runtime data into `build-win/Release`, so that
directory is directly runnable and portable.

## Linux

Install a C++ compiler, CMake, Ninja, OpenGL development files, and SDL's X11,
Wayland, audio, input, and D-Bus development dependencies. On Ubuntu 24.04:

```sh
sudo apt-get install ninja-build libgl1-mesa-dev libegl1-mesa-dev \
  libgles2-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxfixes-dev libxi-dev libxss-dev libxkbcommon-dev libwayland-dev \
  libdecor-0-dev libdrm-dev libgbm-dev libasound2-dev libpulse-dev \
  libdbus-1-dev libudev-dev libibus-1.0-dev
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
./build-linux/dlrl
```

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

The HDV tests use `assets/Images/Deathlord PRODOS.hdv` by default or accept
`-DDLRL_TEST_HDV=/path/to/Deathlord.hdv`. A clean clone therefore runs them
without any separate game download.

Native visual tests are opt-in because they open a real window:

```sh
cmake -S . -B build-mac -DDLRL_ENABLE_GUI_TESTS=ON
ctest --test-dir build-mac -R sdl_window_smoke --output-on-failure
```

To run the bounded real-game modern-UI validation:

```sh
ctest --test-dir build-mac -R sdl_clean_game_flow_smoke --output-on-failure
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
macOS. It builds SDL3 from the pinned FetchContent source, runs core, hook, and
real-HDV boot/input tests, launches and captures the packaged Linux SDL app,
and uploads a directly testable portable archive for each platform on every
run. A version tag such as `3.0.0-rc1` also collects those archives into a draft
prerelease. Each archive includes a verified clean HDV; the application copies
it to per-user storage before normal play and never mutates the packaged
template.
