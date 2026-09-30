#pragma once

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Common.h>
#include <Emerald/Math/Vec2.h>
#include <Emerald/Math/Vec4.h>
#include <Emerald/Renderer/Renderer2D.h>

namespace Asteroids {

using Emerald::Vec2;
using Emerald::Vec4;

// What the player wants the ship to do during one fixed step.
struct ShipControls {
    f32 Rotate = 0.0f; // -1 = turn left, +1 = turn right
    bool Thrust = false;
};

// The player's ship: rotates in place, thrusts forward with inertia and a little drag.
struct Ship {
    static constexpr f32 kRadius = 12.0f; // for collisions (a bit smaller than the drawing)

    Vec2 Position;
    Vec2 Velocity;
    f32 Angle = -Emerald::HalfPi; // radians; 0 = facing right, -HalfPi = facing up (+Y is down)
    bool Thrusting = false;

    // Puts the ship at `position`, at rest, facing up.
    void Reset(const Vec2& position);
    void Update(const ShipControls& controls, f32 dt);

    [[nodiscard]] Vec2 Forward() const;
    [[nodiscard]] Vec2 NosePosition() const;

    // Draws the hull (and a flame of `flameLength` pixels when > 0) at `position`, which is
    // Position or one of its wrapped copies.
    void Draw(Emerald::Renderer2D& r, const Vec2& position, const Vec4& color,
              f32 flameLength) const;
};

// The ship outline alone, e.g. for the lives display.
void DrawShipShape(Emerald::Renderer2D& r, const Emerald::Transform2D& transform,
                   const Vec4& color);

} // namespace Asteroids
