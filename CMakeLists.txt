cmake_minimum_required(VERSION 3.24)

project(Asteroids
    VERSION 0.1.0
    DESCRIPTION "Classic vector Asteroids built with the Emerald engine"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# The game executable and its compiled shaders/ folder end up in build/<preset>/bin.
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
    GIT_TAG        017ed104fd30d137b7fb86f90f5c81b562840b0b
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(Emerald) # also brings SDL3, spdlog, stb (and ImGui if enabled)

# ---------------------------------------------------------------------------
# The game
# ---------------------------------------------------------------------------
add_executable(Asteroids
    src/Main.cpp
    src/Game.cpp
    src/Game.h
    src/Ship.cpp
    src/Ship.h
    src/Asteroid.cpp
    src/Asteroid.h
    src/Bullet.h
    src/HighScores.cpp
    src/HighScores.h
    src/Saucer.cpp
    src/Saucer.h
    src/VectorFont.cpp
    src/VectorFont.h
    src/Playfield.h
    src/Random.h
    src/Sounds.cpp
    src/Sounds.h
)
target_link_libraries(Asteroids PRIVATE Emerald::Emerald)

# The game has no shaders of its own; this compiles the engine's (Renderer2D) into bin/shaders/
# and tells the code which formats exist (EMERALD_SHADER_FORMATS).
emerald_add_shaders(Asteroids)

emerald_set_warnings(Asteroids) # /W4 on MSVC, -Wall -Wextra -Wpedantic elsewhere

# Make Asteroids the default target to run/debug in Visual Studio.
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Asteroids)

# Optional sound overrides: copies assets/sounds/ (player-supplied files, not in git) next to the
# executable. A custom target instead of a POST_BUILD step, so files added later are copied on the
# next build even when the game itself did not need relinking.
add_custom_target(AsteroidsAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Asteroids>/assets/sounds"
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/sounds" "$<TARGET_FILE_DIR:Asteroids>/assets/sounds"
    COMMENT "Copying assets/sounds next to the executable"
    VERBATIM
)
add_dependencies(Asteroids AsteroidsAssets)

# ---------------------------------------------------------------------------
# Tests (ctest): the sound override loader (with generated files), and the pure game logic
# (high score table, saucer aiming)
# ---------------------------------------------------------------------------
option(ASTEROIDS_BUILD_TESTS "Build the Asteroids tests" ON)
if(ASTEROIDS_BUILD_TESTS)
    enable_testing()
    add_executable(AsteroidsTests tests/SoundOverrideTests.cpp tests/Check.h src/Sounds.cpp)
    add_executable(AsteroidsLogicTests tests/GameLogicTests.cpp tests/Check.h
                   src/HighScores.cpp src/Saucer.cpp)
    foreach(test_target AsteroidsTests AsteroidsLogicTests)
        target_include_directories(${test_target} PRIVATE src)
        target_link_libraries(${test_target} PRIVATE Emerald::Emerald)
        emerald_set_warnings(${test_target})
    endforeach()
    add_test(NAME SoundOverrides COMMAND AsteroidsTests)
    add_test(NAME GameLogic COMMAND AsteroidsLogicTests)
endif()
