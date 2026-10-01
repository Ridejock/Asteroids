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
//   crt_effect = off
//   rock_bounce = off
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
    bool Particles = true;   // sparks, dust and exhaust
    bool Crt = false;        // CRT monitor post-process (only the vector game offers it)
    bool RockBounce = false; // rocks bounce off one another (each game picks its default)

    [[nodiscard]] std::string Serialize() const;
    // Keys missing from `text` keep their value from `defaults` (or the defaults above).
    [[nodiscard]] static Settings Parse(std::string_view text);
    [[nodiscard]] static Settings Parse(std::string_view text, const Settings& defaults);

    // <per-user folder of the game `fileName`>/settings.txt, or empty if there is no such folder.
    [[nodiscard]] static std::filesystem::path DefaultPath(std::string_view fileName);
    // A missing or unreadable file gives `defaults`. Does not log (it runs before the log
    // exists, to create the window in the right mode).
    [[nodiscard]] static Settings Load(const std::filesystem::path& file);
    [[nodiscard]] static Settings Load(const std::filesystem::path& file, const Settings& defaults);
    bool Save(const std::filesystem::path& file) const; // logs failures
};

} // namespace Asteroids
