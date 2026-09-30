cmake_minimum_required(VERSION 3.24)

project(Asteroids
    VERSION 0.1.0
    DESCRIPTION "Classic Asteroids (vector and pixel-art versions) built with the Emerald engine"
    LANGUAGES C CXX)

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
    GIT_TAG        00a78ecab243ddbd7c0ec87962e335e8ad062327
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(Emerald) # also brings SDL3, spdlog, stb (and ImGui if enabled)

# ---------------------------------------------------------------------------
# The game: one library with everything both versions share (rules, collisions, scoring, high
# scores, sounds, controls, the app loop and HUD), and one small executable per look:
#   Asteroids       vector lines, like the arcade original (src/Vector)
#   AsteroidsPixel  pixel-art sprites from assets/pixel (src/Pixel)
# ---------------------------------------------------------------------------
add_library(AsteroidsShared STATIC
    src/Shared/AsteroidsApp.cpp
    src/Shared/AsteroidsApp.h
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
target_include_directories(AsteroidsShared PUBLIC src/Shared)
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
    foreach(test_target AsteroidsTests AsteroidsLogicTests)
        target_link_libraries(${test_target} PRIVATE AsteroidsShared)
        emerald_set_warnings(${test_target})
    endforeach()
    add_test(NAME SoundOverrides COMMAND AsteroidsTests)
    add_test(NAME GameLogic COMMAND AsteroidsLogicTests)
endif()
