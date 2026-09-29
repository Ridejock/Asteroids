#pragma once

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Vec2.h>
#include <Emerald/Math/Vec4.h>
#include <Emerald/Renderer/Renderer2D.h>

namespace Asteroids {

using Emerald::Vec2;
using Emerald::Vec4;

enum class SaucerSize : u8 { Large, Small };

// The flying saucer (UFO). It enters on the left or right edge, flies across (sometimes turning
// diagonally), wraps top <-> bottom, and leaves at the far edge. The large one is slow and shoots
// anywhere; the small one is faster and aims at the ship.
struct Saucer {
    static constexpr f32 kBulletSpeed = 420.0f;  // pixels per second
    static constexpr f32 kBulletLifetime = 1.2f; // seconds
    static constexpr u32 kMaxBullets = 2;        // alive at once

    Vec2 Position;
    Vec2 Velocity;
    SaucerSize Size = SaucerSize::Large;
    f32 FireTimer = 0.0f; // seconds until the next shot
    f32 TurnTimer = 0.0f; // seconds until it may change its vertical direction

    [[nodiscard]] f32 Radius() const { return Size == SaucerSize::Large ? 22.0f : 11.0f; }
    [[nodiscard]] f32 Speed() const { return Size == SaucerSize::Large ? 110.0f : 170.0f; }
    [[nodiscard]] u32 Score() const { return Size == SaucerSize::Large ? 200 : 1000; }
    // Seconds between shots.
    [[nodiscard]] f32 FireInterval() const { return Size == SaucerSize::Large ? 1.2f : 0.9f; }

    // Draws the saucer (and a copy on the other side while it crosses the top/bottom edge).
    void Draw(Emerald::Renderer2D& r, const Vec4& color) const;
};

// How far off the small saucer's aim may be (radians, +-): wild at first, deadly from 40000.
[[nodiscard]] f32 SmallSaucerAimError(u32 score);
// Chance that a new saucer is the small one: rare at first, certain from 40000 points.
[[nodiscard]] f32 SmallSaucerChance(u32 score);
// Unit vector from `from` towards `target` the short way around the wrapping playfield, turned by
// `errorAngle` radians (positive = clockwise on screen, since +Y is down).
[[nodiscard]] Vec2 AimDirection(const Vec2& from, const Vec2& target, f32 errorAngle);

} // namespace Asteroids
