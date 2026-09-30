cmake_minimum_required(VERSION 3.24)

project(Asteroids
    VERSION 1.0.0
    DESCRIPTION "A vector arcade shooter (and a pixel-art variant) built with the Emerald engine"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# The game's public name: set it here (or with -DGAME_TITLE="...") and nothing else. It is used
# for the window title, the title screen logo, the .exe name and version resource, the per-user
# folder and the release zip. GAME_FILE_NAME is derived from it ("ROCK DRIFT" -> "RockDrift")
# unless set explicitly.
# ---------------------------------------------------------------------------
set(GAME_TITLE "ROCK DRIFT" CACHE STRING "The game's public name (upper case looks best)")
set(GAME_FILE_NAME "" CACHE STRING "File/folder name of the game (empty = from GAME_TITLE)")
set(game_file_name "${GAME_FILE_NAME}")
if(NOT game_file_name)
    string(REGEX MATCHALL "[A-Za-z0-9]+" title_words "${GAME_TITLE}")
    foreach(word IN LISTS title_words)
        string(SUBSTRING "${word}" 0 1 first)
        string(SUBSTRING "${word}" 1 -1 rest)
        string(TOUPPER "${first}" first)
        string(TOLOWER "${rest}" rest)
        string(APPEND game_file_name "${first}${rest}")
    endforeach()
endif()
set(GAME_FILE_NAME "${game_file_name}")
message(STATUS "Game: '${GAME_TITLE}' ${PROJECT_VERSION} (files: ${GAME_FILE_NAME})")
configure_file(src/Shared/GameInfo.h.in "${CMAKE_BINARY_DIR}/generated/GameInfo.h" @ONLY)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# The game executables, their compiled shaders/ and assets/ end up in build/<preset>/bin.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Emerald engine, fetched from GitHub and pinned to a commit.
#
# To work on the engine and the game at the same time, point this at a local Emerald checkout:
#   cmake --preset debug -DASTEROIDS_EMERALD_SOURCE_DIR=C:/dev/Emerald
# (the same as CMake's own -DFETCHCONTENT_SOURCE_DIR_EMERALD=...). Engine edits are then picked
# up by the next game build, and the checkout's build/_shadercross is reused, so the shader
# compiler is not built a second time.
# ---------------------------------------------------------------------------
set(ASTEROIDS_EMERALD_SOURCE_DIR "" CACHE PATH
    "Local Emerald checkout to use instead of the pinned commit (empty = fetch from GitHub)")
if(ASTEROIDS_EMERALD_SOURCE_DIR)
    get_filename_component(emerald_dir "${ASTEROIDS_EMERALD_SOURCE_DIR}" ABSOLUTE)
    set(FETCHCONTENT_SOURCE_DIR_EMERALD "${emerald_dir}")
    set(EMERALD_SHADERCROSS_BUILD_DIR "${emerald_dir}/build/_shadercross" CACHE PATH
        "Build directory for the shadercross host tool")
    message(STATUS "Asteroids: using local Emerald at ${emerald_dir}")
endif()

include(FetchContent)
FetchContent_Declare(Emerald
    GIT_REPOSITORY https://github.com/Ridejock/Emerald.git
    GIT_TAG        91bf21321d2c3aa74e1d9671699b8904d7d3fed2
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(Emerald) # also brings SDL3, spdlog, stb (and ImGui if enabled)

# ---------------------------------------------------------------------------
# The game: one library with everything both versions share (rules, collisions, scoring, high
# scores, sounds, controls, the app loop and HUD), and one small executable per look:
#   Asteroids       glowing vector lines (src/Vector); the released game
#   AsteroidsPixel  pixel-art sprites from assets/pixel (src/Pixel)
# ---------------------------------------------------------------------------
add_library(AsteroidsShared STATIC
    src/Shared/AsteroidsApp.cpp
    src/Shared/AsteroidsApp.h
    src/Shared/GameInfo.h.in
    src/Shared/Icon.cpp
    src/Shared/Icon.h
    src/Shared/Menu.cpp
    src/Shared/Menu.h
    src/Shared/Screens.cpp
    src/Shared/Screens.h
    src/Shared/Settings.cpp
    src/Shared/Settings.h
    src/Shared/Game.cpp
    src/Shared/Game.h
    src/Shared/Ship.cpp
    src/Shared/Ship.h
    src/Shared/Asteroid.cpp
    src/Shared/Asteroid.h
    src/Shared/Bullet.h
    src/Shared/HighScores.cpp
    src/Shared/HighScores.h
    src/Shared/Saucer.cpp
    src/Shared/Saucer.h
    src/Shared/VectorFont.cpp
    src/Shared/VectorFont.h
    src/Shared/Playfield.h
    src/Shared/Random.h
    src/Shared/Sounds.cpp
    src/Shared/Sounds.h
)
target_include_directories(AsteroidsShared PUBLIC src/Shared "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(AsteroidsShared PUBLIC Emerald::Emerald)
emerald_set_warnings(AsteroidsShared) # /W4 on MSVC, -Wall -Wextra -Wpedantic elsewhere

add_executable(Asteroids src/Vector/Main.cpp)
add_executable(AsteroidsPixel src/Pixel/Main.cpp)

# assets/ (the pixel sprites, and the optional player-supplied sounds in assets/sounds, which are
# not in git) is copied next to the executables. A custom target instead of a POST_BUILD step, so
# files added later are copied on the next build even when nothing needed relinking. Both
# executables share one folder (bin/, or bin/<config>/ in Visual Studio), so one copy serves both.
add_custom_target(AsteroidsAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Asteroids>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:Asteroids>/assets"
    COMMENT "Copying assets/ next to the executables"
    VERBATIM
)

foreach(game Asteroids AsteroidsPixel)
    target_link_libraries(${game} PRIVATE AsteroidsShared)
    # The games have no shaders of their own; this compiles the engine's (Renderer2D) into
    # bin/shaders/ and tells the code which formats exist (EMERALD_SHADER_FORMATS).
    emerald_add_shaders(${game})
    emerald_set_warnings(${game})
    add_dependencies(${game} AsteroidsAssets)
endforeach()

# The .exe files are named after the game: e.g. RockDrift.exe and RockDriftPixel.exe (the targets
# keep their names).
set_target_properties(Asteroids PROPERTIES OUTPUT_NAME "${GAME_FILE_NAME}")
set_target_properties(AsteroidsPixel PROPERTIES OUTPUT_NAME "${GAME_FILE_NAME}Pixel")

if(WIN32)
    # Icon and version details of the .exe (Explorer, taskbar, file properties).
    enable_language(RC)
    set(GAME_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/icon.ico")
    configure_file(packaging/windows/Game.rc.in "${CMAKE_BINARY_DIR}/generated/Game.rc" @ONLY)
    target_sources(Asteroids PRIVATE "${CMAKE_BINARY_DIR}/generated/Game.rc")
    set_source_files_properties("${CMAKE_BINARY_DIR}/generated/Game.rc" PROPERTIES
        OBJECT_DEPENDS "${GAME_ICON_FILE}")
endif()

# Regenerates assets/icon/icon.ico and icon.png from src/Shared/Icon.cpp (they are committed, so
# only needed after changing the icon):  cmake --build --preset release --target UpdateIcon
add_executable(AsteroidsIconGen tools/IconGen/Main.cpp)
target_link_libraries(AsteroidsIconGen PRIVATE AsteroidsShared)
emerald_set_warnings(AsteroidsIconGen)
set_target_properties(AsteroidsIconGen PROPERTIES EXCLUDE_FROM_ALL ON)
add_custom_target(UpdateIcon
    COMMAND AsteroidsIconGen "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon"
    COMMENT "Writing assets/icon/icon.ico and icon.png"
    VERBATIM)

# Make Asteroids the default target to run/debug in Visual Studio.
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Asteroids)

# ---------------------------------------------------------------------------
# Tests (ctest): the sound override loader (with generated files), and the pure game logic
# (high score table, saucer aiming)
# ---------------------------------------------------------------------------
option(ASTEROIDS_BUILD_TESTS "Build the Asteroids tests" ON)
if(ASTEROIDS_BUILD_TESTS)
    enable_testing()
    add_executable(AsteroidsTests tests/SoundOverrideTests.cpp tests/Check.h)
    add_executable(AsteroidsLogicTests tests/GameLogicTests.cpp tests/Check.h)
    add_executable(AsteroidsMenuTests tests/MenuSettingsTests.cpp tests/Check.h)
    foreach(test_target AsteroidsTests AsteroidsLogicTests AsteroidsMenuTests)
        target_link_libraries(${test_target} PRIVATE AsteroidsShared)
        emerald_set_warnings(${test_target})
    endforeach()
    add_test(NAME SoundOverrides COMMAND AsteroidsTests)
    add_test(NAME GameLogic COMMAND AsteroidsLogicTests)
    add_test(NAME MenusAndSettings COMMAND AsteroidsMenuTests)
endif()
