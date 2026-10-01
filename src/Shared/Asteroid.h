#pragma once

#include <array>
#include <optional>

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Vec2.h>
#include <Emerald/Math/Vec4.h>
#include <Emerald/Renderer/Renderer2D.h>

#include "Random.h"

namespace Asteroids {

using Emerald::Vec2;
using Emerald::Vec4;

enum class AsteroidSize : u8 { Large, Medium, Small };

// A drifting, spinning rock with a random jagged outline.
struct Asteroid {
    static constexpr usize kPointCount = 11;

    Vec2 Position;
    Vec2 Velocity;
    f32 Angle = 0.0f; // radians
    f32 Spin = 0.0f;  // radians per second
    AsteroidSize Size = AsteroidSize::Large;
    f32 Radius = 0.0f;                     // for collisions
    std::array<Vec2, kPointCount> Outline; // model space, pixels around the center
    // Rock bounce: fragments of one break share a family and ignore each other for FamilyTime
    // seconds, so they fly apart instead of being pushed out of one another (0 = no family).
    u32 Family = 0;
    f32 FamilyTime = 0.0f;

    void Update(f32 dt);
    // Draws the outline at `position` (Position or one of its wrapped copies).
    void Draw(Emerald::Renderer2D& r, const Vec2& position, const Vec4& color) const;
};

// A new asteroid of `size` at `position` with a random shape, direction, speed and spin.
[[nodiscard]] Asteroid MakeAsteroid(Random& random, AsteroidSize size, const Vec2& position);

// The size an asteroid breaks into when shot (none for small ones: they just vanish).
[[nodiscard]] std::optional<AsteroidSize> SmallerSize(AsteroidSize size);
// Points for shooting an asteroid of `size` (smaller ones are harder to hit).
[[nodiscard]] u32 ScoreFor(AsteroidSize size);
[[nodiscard]] f32 RadiusFor(AsteroidSize size);

} // namespace Asteroids
