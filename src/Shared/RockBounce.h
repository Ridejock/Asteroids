#pragma once

#include <span>
#include <utility>
#include <vector>

#include <Emerald/Physics/SpatialHash.h>

#include "Asteroid.h"

namespace Asteroids {

// The ROCK BOUNCE option: rocks bounce off one another instead of passing through.
//
// Rocks are circles with mass ~ area (radius squared). Overlapping pairs come from a spatial hash
// that wraps like the playfield, so rocks touch across the screen edges too. Each overlapping pair
// is pushed apart (split by mass, so a small rock moves more) and, if the two are moving towards
// each other, gets a slightly inelastic impulse. Fragments of one break (same Family) ignore each
// other while their FamilyTime runs.
class RockBounce {
public:
    static constexpr f32 kRestitution = 0.9f; // 1 = perfectly elastic
    static constexpr f32 kFamilyTime = 0.75f; // seconds fragments ignore their siblings
    static constexpr f32 kSlop = 0.5f;        // overlap left alone (pixels), against jitter
    static constexpr f32 kCorrection = 0.6f;  // part of the overlap removed per step

    RockBounce();

    // Resolves all rock contacts for this step; returns how many pairs touched.
    u32 Step(std::span<Asteroid* const> rocks);

private:
    Emerald::SpatialHash m_Hash;
    u32 m_Count = 0; // ids in the hash: 0 .. m_Count - 1
    std::vector<std::pair<u32, u32>> m_Pairs;
};

// Gives fragments of one break a shared new family (ids count up from 1 and wrap past 0).
[[nodiscard]] inline u32 NextRockFamily(u32& counter)
{
    counter = counter == ~0u ? 1u : counter + 1u;
    return counter;
}

} // namespace Asteroids
