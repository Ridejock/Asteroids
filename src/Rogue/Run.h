#pragma once

#include <array>
#include <optional>
#include <string>

#include <Emerald/Core/Defines.h>

#include "Random.h"

namespace Asteroids::Rogue {

inline constexpr u32 kSectors = 3;
inline constexpr u32 kWavesPerSector = 4; // then the sector's boss

// Where a run is: sector 1..3, wave 1..4, and wave 5 is the boss.
struct Stage {
    u32 Sector = 1;
    u32 Wave = 1;

    [[nodiscard]] bool IsBoss() const { return Wave > kWavesPerSector; }
    // Stages cleared before this one (0 at the start of a run, 15 = the whole run).
    [[nodiscard]] u32 Index() const { return (Sector - 1) * (kWavesPerSector + 1) + Wave - 1; }
    // "SECTOR 2 - WAVE 3" / "SECTOR 2 - BOSS".
    [[nodiscard]] std::string Label() const;
    // "S2 W3" / "S2 BOSS", for the high score table.
    [[nodiscard]] std::string ShortLabel() const;
    bool operator==(const Stage&) const = default;
};
// The stage after `stage`; none after the last boss (the run is won).
[[nodiscard]] std::optional<Stage> NextStage(const Stage& stage);

enum class RockKind : u8 {
    Normal,
    Explosive, // blows up nearby things when destroyed
    Metal,     // takes several hits
    Splitter,  // breaks into three fast pieces
    Count
};
// Hits a rock of `kind` takes (size 0 = large .. 2 = small).
[[nodiscard]] i32 RockHealth(RockKind kind, u32 sizeIndex);

enum class BossKind : u8 { GiantRock, Mothership, Station };
[[nodiscard]] BossKind BossFor(u32 sector);

// What a wave throws at the player.
struct WavePlan {
    std::array<RockKind, 12> Rocks{}; // the large rocks' kinds...
    u32 RockCount = 0;                // ...the first RockCount are used
    f32 SaucerDelayMin = 12.0f;       // seconds between saucers
    f32 SaucerDelayMax = 18.0f;
    f32 SmallSaucerChance = 0.1f;
    f32 AimError = 0.4f;          // radians, small saucers
    f32 SaucerFireScale = 1.0f;   // < 1: shoots more often
    f32 SaucerBulletSpeed = 1.0f; // multiplier
    u32 SaucerHealth = 1;         // hits a saucer takes
};
// The plan for a (non-boss) wave: more rocks and more special kinds deeper in, and tougher
// saucers in later sectors. Boss stages get a few escort rocks.
[[nodiscard]] WavePlan PlanWave(Random& random, const Stage& stage);

} // namespace Asteroids::Rogue
