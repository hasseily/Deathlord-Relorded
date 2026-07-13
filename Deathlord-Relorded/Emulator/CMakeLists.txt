# dlrl_emulator: stripped, portable AppleWin core.

set(EMU_CORE
    StdAfx.cpp
    StrFormat.cpp
    Utilities.cpp
    SynchronousEventManager.cpp
    YamlHelper.cpp
    Log.cpp
    Core.cpp
    FrameBase.cpp
    CmdLine.cpp
    SaveState.cpp
    CPU.cpp
    Memory.cpp
    Card.cpp
    CardManager.cpp
)
if(WIN32)
    list(APPEND EMU_CORE Registry.cpp)
else()
    list(APPEND EMU_CORE Registry_posix.cpp)
endif()

set(EMU_VIDEO
    Video.cpp
    NTSC.cpp
    NTSC_CharSet.cpp
    RGBMonitor.cpp
    VidHD.cpp
)

set(EMU_AUDIO
    AY8910.cpp
    Mockingboard.cpp
    MockingboardCardManager.cpp
    SoundCore.cpp
    Speaker.cpp
    SSI263.cpp
    SAM.cpp
    6522.cpp
    6821.cpp
    Riff.cpp
    linuxsoundbuffer.cpp   # platform-neutral ring buffer (vendored from upstream linux/)
)

set(EMU_DISK_IO
    Harddisk.cpp
    DiskImage.cpp
    DiskImageHelper.cpp
    LanguageCard.cpp
    NoSlotClock.cpp
)
if(WIN32)
    list(APPEND EMU_DISK_IO Keyboard.cpp Joystick.cpp)
else()
    list(APPEND EMU_DISK_IO Keyboard_posix.cpp Joystick_posix.cpp)
endif()

# ---------------------------------------------------------------------------
# libyaml — vendored under Emulator/libyaml/, built as a small static lib.
# ---------------------------------------------------------------------------
set(YAML_SOURCES
    libyaml/src/api.c
    libyaml/src/dumper.c
    libyaml/src/emitter.c
    libyaml/src/loader.c
    libyaml/src/parser.c
    libyaml/src/reader.c
    libyaml/src/scanner.c
    libyaml/src/writer.c
)

add_library(yaml STATIC ${YAML_SOURCES})
target_include_directories(yaml PUBLIC libyaml/include)
target_include_directories(yaml PRIVATE libyaml/win32)
target_compile_definitions(yaml PRIVATE HAVE_CONFIG_H)
# Consumers also need YAML_DECLARE_STATIC so yaml.h doesn't emit dllimport
# decorations against this static lib on Windows.
target_compile_definitions(yaml PUBLIC YAML_DECLARE_STATIC)

# ---------------------------------------------------------------------------
# zlib — vendored under Emulator/zlib/, used by minizip for ZIP/GZ disk images.
# ---------------------------------------------------------------------------
set(ZLIB_SOURCES
    zlib/adler32.c
    zlib/compress.c
    zlib/crc32.c
    zlib/deflate.c
    zlib/gzclose.c
    zlib/gzlib.c
    zlib/gzread.c
    zlib/gzwrite.c
    zlib/infback.c
    zlib/inffast.c
    zlib/inflate.c
    zlib/inftrees.c
    zlib/trees.c
    zlib/uncompr.c
    zlib/zutil.c
)

add_library(dlrl_zlib STATIC ${ZLIB_SOURCES})
target_include_directories(dlrl_zlib PUBLIC zlib)
if(MSVC)
    target_compile_definitions(dlrl_zlib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(APPLE)
    # gzguts.h sets _POSIX_C_SOURCE before including <fcntl.h>, which on
    # macOS hides the read()/close() prototypes that gz{read,write}.c
    # call. Apple Clang now errors on implicit declarations. Force
    # <unistd.h> in for the zlib TUs so the prototypes are visible.
    target_compile_options(dlrl_zlib PRIVATE -include unistd.h)
endif()

# ---------------------------------------------------------------------------
# minizip — ZIP archive read/write on top of zlib.
# ---------------------------------------------------------------------------
set(MINIZIP_SOURCES
    minizip/ioapi.c
    minizip/mztools.c
    minizip/unzip.c
    minizip/zip.c
)
if(WIN32)
    list(APPEND MINIZIP_SOURCES minizip/iowin32.c)
endif()

add_library(dlrl_minizip STATIC ${MINIZIP_SOURCES})
target_include_directories(dlrl_minizip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dlrl_minizip PUBLIC dlrl_zlib)
if(MSVC)
    target_compile_definitions(dlrl_minizip PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------------------
# libwindows: Win32 API shim for non-Windows targets. AppleWin's StdAfx.h
# does `#include "windows.h"` on !_WIN32 expecting this local shim. Vendored
# verbatim from upstream AppleWin's source/linux/libwindows/.
# ---------------------------------------------------------------------------
if(NOT WIN32)
    set(LIBWIN_SOURCES
        libwindows/dmusicc.cpp
        libwindows/fileapi.cpp
        libwindows/guiddef.cpp
        libwindows/winhandles.cpp
        libwindows/misc.cpp
        libwindows/strsafe.cpp
        libwindows/winstrings.cpp
        libwindows/timeapi.cpp
        libwindows/winbase.cpp
        libwindows/winnls.cpp
        libwindows/winuser.cpp
    )
    add_library(libwindows STATIC ${LIBWIN_SOURCES})
    target_include_directories(libwindows PUBLIC libwindows)
endif()

# ---------------------------------------------------------------------------
# Portable emulator static library.
# ---------------------------------------------------------------------------
add_library(dlrl_emulator STATIC
    ${EMU_CORE}
    ${EMU_VIDEO}
    ${EMU_AUDIO}
    ${EMU_DISK_IO}
)

target_include_directories(dlrl_emulator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(dlrl_emulator PUBLIC yaml dlrl_minizip dlrl_zlib)

if(WIN32)
    # AppleWin's public headers include windows.h. Prevent its min/max macros
    # from breaking std::min/std::max in the portable host and hook layers.
    target_compile_definitions(dlrl_emulator PUBLIC NOMINMAX)
    target_link_libraries(dlrl_emulator PUBLIC
        winmm
        dsound
        dxguid
        strmiids   # CLSID_SystemClock used by SoundCore.cpp
    )
else()
    target_link_libraries(dlrl_emulator PUBLIC libwindows)
endif()
