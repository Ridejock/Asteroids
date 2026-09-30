#pragma once

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Vec2.h>

namespace Asteroids {

// A shot from the ship. Bullets fly straight (wrapping around the edges) until they hit an
// asteroid or their time runs out.
struct Bullet {
    static constexpr f32 kSpeed = 640.0f;  // pixels per second, added to the ship's velocity
    static constexpr f32 kLifetime = 1.0f; // seconds
    static constexpr u32 kMaxAlive = 4;    // like the arcade: at most 4 shots on screen

    Emerald::Vec2 Position;
    Emerald::Vec2 Velocity;
    f32 TimeLeft = kLifetime;
};

} // namespace Asteroids
