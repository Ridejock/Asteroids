#pragma once

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
};

[[nodiscard]] Sounds MakeSounds();

} // namespace Asteroids
