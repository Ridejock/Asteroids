#include "Asteroid.h"

#include <cmath>

#include <Emerald/Math/Common.h>

#include "Playfield.h"

namespace Asteroids {

namespace {

// Speed range (pixels per second) per size: smaller rocks are faster.
struct SpeedRange {
    f32 Min;
    f32 Max;
};

SpeedRange SpeedFor(AsteroidSize size)
{
    switch (size) {
    case AsteroidSize::Large:
        return {30.0f, 70.0f};
    case AsteroidSize::Medium:
        return {60.0f, 115.0f};
    case AsteroidSize::Small:
        return {90.0f, 170.0f};
    }
    return {50.0f, 50.0f};
}

} // namespace

f32 RadiusFor(AsteroidSize size)
{
    switch (size) {
    case AsteroidSize::Large:
        return 46.0f;
    case AsteroidSize::Medium:
        return 24.0f;
    case AsteroidSize::Small:
        return 12.0f;
    }
    return 0.0f;
}

u32 ScoreFor(AsteroidSize size)
{
    switch (size) {
    case AsteroidSize::Large:
        return 20;
    case AsteroidSize::Medium:
        return 50;
    case AsteroidSize::Small:
        return 100;
    }
    return 0;
}

std::optional<AsteroidSize> SmallerSize(AsteroidSize size)
{
    switch (size) {
    case AsteroidSize::Large:
        return AsteroidSize::Medium;
    case AsteroidSize::Medium:
        return AsteroidSize::Small;
    case AsteroidSize::Small:
        break;
    }
    return std::nullopt;
}

Asteroid MakeAsteroid(Random& random, AsteroidSize size, const Vec2& position)
{
    Asteroid a;
    a.Position = position;
    a.Size = size;
    a.Radius = RadiusFor(size);

    const SpeedRange speed = SpeedFor(size);
    a.Velocity = random.Direction() * random.Float(speed.Min, speed.Max);
    a.Angle = random.Float(0.0f, Emerald::TwoPi);
    a.Spin = random.Float(-1.2f, 1.2f);

    // Jagged outline: points evenly spaced around a circle, each at a random distance (and with
    // a little angular jitter), so every rock looks different.
    for (usize i = 0; i < Asteroid::kPointCount; ++i) {
        const f32 step = Emerald::TwoPi / static_cast<f32>(Asteroid::kPointCount);
        const f32 angle = step * static_cast<f32>(i) + random.Float(-0.25f, 0.25f) * step;
        // Now and then a point is pulled in deep, which gives the classic dented look.
        const f32 distance = a.Radius * (random.Chance(0.25f) ? random.Float(0.5f, 0.7f)
                                                              : random.Float(0.85f, 1.15f));
        a.Outline[i] = Vec2(std::cos(angle), std::sin(angle)) * distance;
    }
    return a;
}

void Asteroid::Update(f32 dt)
{
    Position = Wrap(Position + Velocity * dt);
    Angle += Spin * dt;
    FamilyTime = Emerald::Max(FamilyTime - dt, 0.0f);
}

void Asteroid::Draw(Emerald::Renderer2D& r, const Vec2& position, const Vec4& color) const
{
    r.DrawPolygon(Outline, color, {.Position = position, .Rotation = Angle});
}

} // namespace Asteroids
