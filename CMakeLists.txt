cmake_minimum_required(VERSION 3.20)
project(DeathlordRelorded VERSION 3.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

include(CTest)
option(DLRL_ENABLE_GUI_TESTS "Run bounded native SDL window tests" OFF)

# SDL3 is the single platform API. Prefer an installed package and use the
# same pinned FetchContent fallback as NAC for clean machines and CI.
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
    include(FetchContent)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.20
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

find_package(OpenGL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.7-docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3 OpenGL::GL)

add_library(dlrl_pp STATIC
    Deathlord-Relorded/Emulator/pp/postprocessor.cpp
    Deathlord-Relorded/Emulator/pp/shader.cpp
    third_party/glad/glad.cpp
    third_party/extras/ImGuiFileDialog.cpp
)
target_include_directories(dlrl_pp PUBLIC
    Deathlord-Relorded/Emulator/pp
    third_party
    third_party/stb
)
target_link_libraries(dlrl_pp PUBLIC SDL3::SDL3 OpenGL::GL imgui)
target_compile_features(dlrl_pp PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(dlrl_pp PRIVATE /wd4244 /wd4267 /wd4305 /wd4458)
    target_compile_definitions(dlrl_pp PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        _USE_MATH_DEFINES
    )
endif()

# NAC-derived portable AppleWin core. This target contains SmartPort HDV
# support and deliberately contains no Disk II card implementation.
add_subdirectory(Deathlord-Relorded/Emulator)

add_library(dlrl_platform STATIC
    src/Frame.cpp
    src/AudioOutput.cpp
    src/DlrlHooks.cpp
    src/InventoryRules.cpp
    src/InventoryState.cpp
    src/PartyTransfer.cpp
    src/FileUtil.cpp
)
target_include_directories(dlrl_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/Deathlord-Relorded
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(dlrl_platform PUBLIC
    dlrl_emulator
    SDL3::SDL3
)

if(APPLE)
    target_compile_definitions(dlrl_platform PRIVATE GL_SILENCE_DEPRECATION)
endif()

add_executable(dlrl WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/Renderer.cpp
    src/AppleStyle.cpp
    src/Texture.cpp
    src/SpriteFont.cpp
    src/ModernUI.cpp
)
target_include_directories(dlrl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/Deathlord-Relorded
)
target_link_libraries(dlrl PRIVATE
    dlrl_platform
    dlrl_pp
    imgui
    SDL3::SDL3
    OpenGL::GL
)

if(APPLE)
    target_compile_definitions(dlrl PRIVATE GL_SILENCE_DEPRECATION)
    set_target_properties(dlrl PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Deathlord Relorded"
        MACOSX_BUNDLE_GUI_IDENTIFIER com.rikkles.deathlordrelorded
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
elseif(NOT WIN32)
    target_link_libraries(dlrl PRIVATE pthread)
endif()

# SDL's shared target emits SDL3.dll in its own FetchContent build directory.
# Keep a copy next to every Windows executable so the app and CTest binaries
# run directly from the multi-config Release directory (same arrangement as
# the NAC portable build).
if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET dlrl POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:dlrl>
        COMMENT "Staging SDL3 runtime"
        VERBATIM
    )
endif()

if(APPLE)
    set(DLRL_RESOURCE_DIR $<TARGET_BUNDLE_DIR:dlrl>/Contents/Resources)
else()
    set(DLRL_RESOURCE_DIR $<TARGET_FILE_DIR:dlrl>)
endif()

add_custom_command(TARGET dlrl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/pp/assets ${DLRL_RESOURCE_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/pp/presets ${DLRL_RESOURCE_DIR}/presets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/pp/shaders ${DLRL_RESOURCE_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Deathlord-Relorded/Emulator/Resources ${DLRL_RESOURCE_DIR}/Resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Deathlord-Relorded/Assets ${DLRL_RESOURCE_DIR}/DLRLAssets
    COMMENT "Staging DLRL runtime data"
    VERBATIM
)

# The downloaded 2.0.1 release supplies the immutable clean-game template.
# It is never tracked, but local/release builds stage it beside the portable
# assets so the active per-user HDV can be replaced safely on request.
set(DLRL_CLEAN_HDV_SOURCE
    "${CMAKE_SOURCE_DIR}/extras/deathlord-relorded-win-201/Images/Deathlord PRODOS.hdv")
if(EXISTS "${DLRL_CLEAN_HDV_SOURCE}")
    add_custom_command(TARGET dlrl POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DLRL_RESOURCE_DIR}/Images"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLRL_CLEAN_HDV_SOURCE}"
            "${DLRL_RESOURCE_DIR}/Images/Deathlord PRODOS.hdv"
        COMMENT "Staging clean DLRL HDV template"
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    add_executable(dlrl_core_smoke
        tests/integration/CoreSmoke.cpp
    )
    target_link_libraries(dlrl_core_smoke PRIVATE dlrl_platform)
    target_compile_definitions(dlrl_core_smoke PRIVATE
        DLRL_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Deathlord-Relorded/Emulator/Resources"
    )
    add_test(NAME core_smoke COMMAND dlrl_core_smoke)

    add_executable(dlrl_hooks_tests
        tests/integration/DlrlHooksTests.cpp
    )
    target_link_libraries(dlrl_hooks_tests PRIVATE dlrl_platform)
    target_compile_definitions(dlrl_hooks_tests PRIVATE
        DLRL_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Deathlord-Relorded/Emulator/Resources"
    )
    add_test(NAME relorded_hooks COMMAND dlrl_hooks_tests)

    add_executable(dlrl_capture
        tests/integration/DlrlCapture.cpp
    )
    target_link_libraries(dlrl_capture PRIVATE dlrl_platform)
    target_compile_definitions(dlrl_capture PRIVATE
        DLRL_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Deathlord-Relorded/Emulator/Resources"
    )

    set(DLRL_PRIVATE_HDV
        "${CMAKE_CURRENT_SOURCE_DIR}/extras/deathlord-relorded-win-201/Images/Deathlord PRODOS.hdv"
        CACHE FILEPATH "Private DLRL HDV used by local integration tests")
    add_test(NAME hdv_boot_capture
        COMMAND dlrl_capture
            --hdv "${DLRL_PRIVATE_HDV}"
            --frames 600
            --output "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-boot.bmp"
    )
    set_tests_properties(hdv_boot_capture PROPERTIES
        SKIP_RETURN_CODE 77
        PASS_REGULAR_EXPRESSION "pc=\\$1D3E.*fnv1a64=972f16197c8c6331"
    )

    add_test(NAME hdv_input_capture
        COMMAND dlrl_capture
            --hdv "${DLRL_PRIVATE_HDV}"
            --frames 1800
            --key-at 800
            --key X
            --output "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-menu.bmp"
    )
    set_tests_properties(hdv_input_capture PROPERTIES
        SKIP_RETURN_CODE 77
        PASS_REGULAR_EXPRESSION "pc=\\$1C27.*fnv1a64=466f292c38add1a7"
    )

    if(DLRL_ENABLE_GUI_TESTS AND EXISTS "${DLRL_PRIVATE_HDV}")
        add_test(NAME sdl_window_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --hdv "${DLRL_PRIVATE_HDV}"
                --smoke-frames 600
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-host.bmp"
        )
        set_tests_properties(sdl_window_smoke PROPERTIES
            LABELS "gui;visual"
            PASS_REGULAR_EXPRESSION "frames=600 pc=\\$1D3E"
        )
        add_test(NAME sdl_live_game_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --hdv "${DLRL_PRIVATE_HDV}"
                --smoke-frames 5000
                --key-event 800:X
                --key-event 1900:P
                --key-event 2300:P
                --key-event 2800:P
                --key-event 3500:SPACE
                --key-event 4000:SPACE
                --key-event 4500:SPACE
                --window-size 1920x1080
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-live-game.bmp"
        )
        set_tests_properties(sdl_live_game_smoke PROPERTIES
            LABELS "gui;visual;hdv"
            PASS_REGULAR_EXPRESSION "frames=5000 pc=\\$4AEF"
        )
        add_test(NAME sdl_startup_splash_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --hdv "${DLRL_PRIVATE_HDV}"
                --smoke-frames 4000
                --key-event 800:X
                --key-event 1900:P
                --key-event 2300:P
                --key-event 2800:P
                --window-size 1920x1080
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-startup-splash.bmp"
        )
        set_tests_properties(sdl_startup_splash_smoke PROPERTIES
            LABELS "gui;visual;hdv"
            PASS_REGULAR_EXPRESSION "DLRL startup splash: waiting for Space"
        )
    endif()
    if(DLRL_ENABLE_GUI_TESTS)
        add_test(NAME sdl_modern_ui_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-modern-ui.bmp"
        )
        set_tests_properties(sdl_modern_ui_smoke PROPERTIES
            LABELS "gui;visual"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        add_test(NAME sdl_original_ui_postprocess_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture
                --show-original-interface
                --postprocess
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-original-ui-postprocess.bmp"
        )
        set_tests_properties(sdl_original_ui_postprocess_smoke PROPERTIES
            LABELS "gui;visual"
            PASS_REGULAR_EXPRESSION "postprocessor: modern original interface texture active"
        )
        add_test(NAME sdl_spell_window_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture
                --show-spells
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-spell-window.bmp"
        )
        set_tests_properties(sdl_spell_window_smoke PROPERTIES
            LABELS "gui;visual"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        add_test(NAME sdl_log_window_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture
                --show-log
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-log-window.bmp"
        )
        set_tests_properties(sdl_log_window_smoke PROPERTIES
            LABELS "gui;visual"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        add_test(NAME sdl_loading_ui_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture-mode loading
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-loading-ui.bmp"
        )
        set_tests_properties(sdl_loading_ui_smoke PROPERTIES
            LABELS "gui;visual"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        add_test(NAME sdl_battle_ui_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture-mode battle
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-battle-ui.bmp"
        )
        set_tests_properties(sdl_battle_ui_smoke PROPERTIES
            LABELS "gui;visual"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        add_test(NAME sdl_inventory_ui_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture-mode inventory
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-inventory-ui.bmp"
        )
        set_tests_properties(sdl_inventory_ui_smoke PROPERTIES
            LABELS "gui;visual"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        add_test(NAME sdl_gameover_ui_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture-mode gameover
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-gameover-ui.bmp"
        )
        set_tests_properties(sdl_gameover_ui_smoke PROPERTIES
            LABELS "gui;visual"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        add_test(NAME sdl_party_editor_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture
                --show-party-editor
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-party-editor.bmp"
        )
        set_tests_properties(sdl_party_editor_smoke PROPERTIES
            LABELS "gui;visual;party"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        add_test(NAME sdl_new_game_confirmation_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture
                --show-new-game-confirm
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-new-game-confirmation.bmp"
        )
        set_tests_properties(sdl_new_game_confirmation_smoke PROPERTIES
            LABELS "gui;visual;party"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        add_test(NAME sdl_party_import_confirmation_smoke
            COMMAND $<TARGET_FILE:dlrl>
                --smoke-frames 3
                --ui-fixture
                --show-import-confirmation
                --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-party-import-confirmation.bmp"
        )
        set_tests_properties(sdl_party_import_confirmation_smoke PROPERTIES
            LABELS "gui;visual;party"
            PASS_REGULAR_EXPRESSION "frames=3"
        )
        if(EXISTS "${DLRL_CLEAN_HDV_SOURCE}")
            add_test(NAME sdl_clean_new_game_reset
                COMMAND $<TARGET_FILE:dlrl>
                    --smoke-frames 600
                    --test-clean-reset
                    --capture "${CMAKE_CURRENT_BINARY_DIR}/Testing/dlrl-clean-reset.bmp"
            )
            set_tests_properties(sdl_clean_new_game_reset PROPERTIES
                LABELS "gui;hdv;party"
                PASS_REGULAR_EXPRESSION "active HDV replaced and rebooted"
            )
        endif()
    endif()
endif()
