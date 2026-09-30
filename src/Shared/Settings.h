#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <Emerald/Core/Defines.h>

namespace Asteroids {

// The player's options, saved as settings.txt in the per-user folder, one "key = value" per line:
//
//   master_volume = 80
//   sfx_volume = 100
//   fullscreen = off
//   vsync = on
//   screen_shake = on
//   particles = on
//
// Unknown keys and bad values are ignored (the default stays), so an old or hand-edited file
// never breaks the game.
struct Settings {
    static constexpr u32 kVolumeStep = 10; // percent per press in the options menu

    u32 MasterVolume = 80; // percent, 0..100
    u32 SfxVolume = 100;   // percent, 0..100 (sound effects; not the optional music/ambience)
    bool Fullscreen = false;
    bool VSync = true;
    bool ScreenShake = true;
    bool Particles = true; // sparks, dust and exhaust

    [[nodiscard]] std::string Serialize() const;
    [[nodiscard]] static Settings Parse(std::string_view text);

    // <per-user folder>/settings.txt, or empty if there is no such folder.
    [[nodiscard]] static std::filesystem::path DefaultPath();
    // A missing or unreadable file gives the defaults. Does not log (it runs before the log
    // exists, to create the window in the right mode).
    [[nodiscard]] static Settings Load(const std::filesystem::path& file);
    bool Save(const std::filesystem::path& file) const; // logs failures
};

} // namespace Asteroids
