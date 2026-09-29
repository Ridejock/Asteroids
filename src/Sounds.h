#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <Emerald/Audio/Sound.h>

namespace Asteroids {

// Every sound effect, generated at startup with Emerald's Synth (no audio files).
struct Sounds {
    Emerald::Sound Fire;           // short falling "pew"
    Emerald::Sound Thrust;         // rumbling noise, loops seamlessly
    Emerald::Sound ExplosionLarge; // noise bursts: bigger rock = lower and longer
    Emerald::Sound ExplosionMedium;
    Emerald::Sound ExplosionSmall;
    Emerald::Sound ShipExplosion; // long, deep crash
    Emerald::Sound ExtraLife;     // a few quick beeps
    Emerald::Sound Hyperspace;    // rising whoosh
    Emerald::Sound BeatHigh;      // the heartbeat's two low thumps
    Emerald::Sound BeatLow;
    // No generated versions of these two: empty unless the player supplies a file.
    Emerald::Sound Music;    // loops on the game over screen
    Emerald::Sound Ambience; // loops quietly under the gameplay
};

[[nodiscard]] Sounds MakeSounds();

// Optional replacements supplied by the player: for every known name (fire, thrust, bang_large,
// bang_medium, bang_small, ship_explode, extra_life, beat1, beat2, hyperspace, music, ambience),
// loads
// `<folder>/<name>.mp3` or `<name>.wav` if it exists and uses it instead of the generated sound.
// Each file is scaled to the peak level of the sound it replaces (music and ambience to 0.8), so
// the mix stays balanced however loud the files are. Returns the names that were replaced (files
// that fail to load keep the generated sound).
std::vector<std::string> LoadOverrides(Sounds& sounds, const std::filesystem::path& folder);

} // namespace Asteroids
