#include "Ship.h"

#include <cmath>

#include "Playfield.h"

namespace Asteroids {

namespace {

constexpr f32 kTurnSpeed = 4.8f;  // radians per second
constexpr f32 kThrust = 440.0f;   // acceleration, pixels per second^2
constexpr f32 kDrag = 0.4f;       // velocity decays by e^(-kDrag * t): slow, "space-like" drift
constexpr f32 kMaxSpeed = 540.0f; // pixels per second
constexpr f32 kNoseDistance = 16.0f;

// Hull in model space (pixels, facing +X): two long sides meeting at the nose...
constexpr Vec2 kHull[] = {{-10.0f, -10.0f}, {kNoseDistance, 0.0f}, {-10.0f, 10.0f}};
// ...and a cross bar near the back, where the flame comes out.
constexpr f32 kBarX = -6.0f;
constexpr f32 kBarHalfWidth = 8.4f;

} // namespace

void Ship::Reset(const Vec2& position)
{
    Position = position;
    Velocity = {};
    Angle = -Emerald::HalfPi;
    Thrusting = false;
}

Vec2 Ship::Forward() const
{
    return {std::cos(Angle), std::sin(Angle)};
}

Vec2 Ship::NosePosition() const
{
    return Position + Forward() * kNoseDistance;
}

Vec2 Ship::EnginePosition() const
{
    return Position + Forward() * kBarX;
}

void Ship::Update(const ShipControls& controls, f32 dt)
{
    // With +Y down, a growing angle turns clockwise on screen, i.e. to the right.
    Angle += controls.Rotate * kTurnSpeed * dt;

    Thrusting = controls.Thrust;
    if (Thrusting)
        Velocity += Forward() * (kThrust * dt);

    // Drag and a speed limit keep the ship controllable.
    Velocity *= std::exp(-kDrag * dt);
    const f32 speed = Emerald::Length(Velocity);
    if (speed > kMaxSpeed)
        Velocity *= kMaxSpeed / speed;

    Position = Wrap(Position + Velocity * dt);
}

void DrawShipShape(Emerald::Renderer2D& r, const Emerald::Transform2D& transform, const Vec4& color)
{
    r.DrawPolyline(kHull, color, transform);
    r.DrawLine(transform.Apply({kBarX, -kBarHalfWidth}), transform.Apply({kBarX, kBarHalfWidth}),
               color);
}

void Ship::Draw(Emerald::Renderer2D& r, const Vec2& position, const Vec4& color,
                f32 flameLength) const
{
    const Emerald::Transform2D transform{.Position = position, .Rotation = Angle};
    DrawShipShape(r, transform, color);
    if (flameLength > 0.0f) {
        const Vec2 flame[] = {{kBarX, -4.0f}, {kBarX - flameLength, 0.0f}, {kBarX, 4.0f}};
        r.DrawPolyline(flame, {1.0f, 0.65f, 0.25f, 1.0f}, transform);
    }
}

} // namespace Asteroids
