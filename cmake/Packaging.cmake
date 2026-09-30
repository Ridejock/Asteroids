# Release packages, one zip per game (each is published on its own):
#
#   cmake --preset release
#   cmake --build --preset release --target package
#   -> build/release/<GAME_FILE_NAME>-<version>-windows-x64.zip   (vector game, component Game)
#   -> build/release/<PIXEL_FILE_NAME>-<version>-windows-x64.zip  (pixel roguelike, component Rogue)
#
# Each contains only what a player needs: the .exe, its compiled shaders, README.txt,
# LICENSE.txt and THIRD_PARTY_LICENSES.txt; the roguelike also its sprites (assets/pixel/).
# Nothing else from assets/ is installed: player-supplied sound overrides (assets/sounds/, often
# copyrighted) must never ship.
# (`cmake --install build/release --component Game --prefix <dir>` gives the same files unzipped.)

set(package_dir "${CMAKE_BINARY_DIR}/package")

# README.txt with the title, version and controls (one per game).
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/packaging/README.txt.in" "${package_dir}/README.txt"
               @ONLY NEWLINE_STYLE CRLF)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/packaging/README-rogue.txt.in"
               "${package_dir}/rogue/README.txt" @ONLY NEWLINE_STYLE CRLF)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${package_dir}/LICENSE.txt" COPYONLY)

# THIRD_PARTY_LICENSES.txt: the license texts of everything linked into the .exe, taken from the
# fetched sources, so they always match the pinned versions.
set(notices "${package_dir}/THIRD_PARTY_LICENSES.txt")
file(WRITE "${notices}"
     "This game uses the following libraries. Their licenses follow.\n\n")
function(_add_notice title file)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "License file not found: ${file}")
    endif()
    file(READ "${file}" text)
    file(APPEND "${notices}"
         "==============================================================================\n"
         "${title}\n"
         "==============================================================================\n\n"
         "${text}\n\n")
endfunction()

foreach(dep Emerald SDL3 spdlog stb dr_libs nlohmann_json)
    FetchContent_GetProperties(${dep})
endforeach()
_add_notice("Emerald engine (https://github.com/Ridejock/Emerald)" "${emerald_SOURCE_DIR}/LICENSE")
_add_notice("SDL3 (https://libsdl.org)" "${sdl3_SOURCE_DIR}/LICENSE.txt")
_add_notice("HIDAPI, part of SDL3 (used under its BSD-style license)"
            "${sdl3_SOURCE_DIR}/src/hidapi/LICENSE-bsd.txt")
_add_notice("spdlog (https://github.com/gabime/spdlog)" "${spdlog_SOURCE_DIR}/LICENSE")
_add_notice("{fmt}, bundled with spdlog (https://github.com/fmtlib/fmt)"
            "${spdlog_SOURCE_DIR}/include/spdlog/fmt/bundled/fmt.license.rst")
_add_notice("stb (https://github.com/nothings/stb)" "${stb_SOURCE_DIR}/LICENSE")
_add_notice("nlohmann/json (https://github.com/nlohmann/json)"
            "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT")
# dr_mp3 keeps its license at the end of the header.
file(READ "${dr_libs_SOURCE_DIR}/dr_mp3.h" dr_mp3)
string(FIND "${dr_mp3}" "This software is available as a choice of the following licenses"
       license_start)
if(license_start LESS 0)
    message(FATAL_ERROR "Could not find the license text in dr_mp3.h")
endif()
string(SUBSTRING "${dr_mp3}" ${license_start} -1 dr_mp3_license)
# (It is followed by the notice of minimp3, which dr_mp3 is based on; keep that, drop the
# comment markers.)
string(REGEX REPLACE "(^|\n)[ \t]*(/\\*|\\*/)[ \t]*" "\\1" dr_mp3_license "${dr_mp3_license}")
file(WRITE "${package_dir}/dr_mp3-license.txt" "${dr_mp3_license}")
_add_notice("dr_mp3 (https://github.com/mackron/dr_libs)" "${package_dir}/dr_mp3-license.txt")

# What goes into the package ("Game" component; the dependencies' own install rules, if any,
# are in other components and left out).
install(TARGETS Asteroids RUNTIME DESTINATION . COMPONENT Game)
install(DIRECTORY "$<TARGET_FILE_DIR:Asteroids>/shaders/" DESTINATION shaders COMPONENT Game)
install(FILES "${package_dir}/README.txt" "${package_dir}/LICENSE.txt" "${notices}"
        DESTINATION . COMPONENT Game)

# The roguelike ("Rogue" component): the same, plus its sprites.
install(TARGETS AsteroidsPixel RUNTIME DESTINATION . COMPONENT Rogue)
install(DIRECTORY "$<TARGET_FILE_DIR:AsteroidsPixel>/shaders/" DESTINATION shaders
        COMPONENT Rogue)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/pixel/atlas.png"
              "${CMAKE_CURRENT_SOURCE_DIR}/assets/pixel/atlas.json"
        DESTINATION assets/pixel COMPONENT Rogue)
install(FILES "${package_dir}/rogue/README.txt" "${package_dir}/LICENSE.txt" "${notices}"
        DESTINATION . COMPONENT Rogue)

if(WIN32)
    set(package_platform windows)
elseif(APPLE)
    set(package_platform macos)
else()
    set(package_platform linux)
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    string(APPEND package_platform "-x64")
endif()

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "${GAME_FILE_NAME}")
set(CPACK_PACKAGE_VENDOR "Ervin Ashley")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${GAME_FILE_NAME}-${PROJECT_VERSION}-${package_platform}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF) # the .exe at the top of the zip
set(CPACK_COMPONENTS_ALL Game Rogue)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON) # one zip per component, named:
set(CPACK_ARCHIVE_GAME_FILE_NAME "${GAME_FILE_NAME}-${PROJECT_VERSION}-${package_platform}")
set(CPACK_ARCHIVE_ROGUE_FILE_NAME "${PIXEL_FILE_NAME}-${PROJECT_VERSION}-${package_platform}")
include(CPack)
