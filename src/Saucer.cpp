#include "Saucer.h"

#include <array>
#include <cmath>

#include <Emerald/Math/Common.h>

#include "Playfield.h"

namespace Asteroids {

namespace {

constexpr u32 kAlwaysSmallScore = 40000;

// The classic outline for the large saucer (the small one is the same at half size): a hull
// with a line across its middle, and a dome on top. Pixels around the center, +Y down.
constexpr std::array<Vec2, 6> kHull = {
    Vec2{-24.0f, 0.0f}, Vec2{-10.0f, -7.0f}, Vec2{10.0f, -7.0f},
    Vec2{24.0f, 0.0f},  Vec2{10.0f, 8.0f},   Vec2{-10.0f, 8.0f},
};
constexpr std::array<Vec2, 4> kDome = {Vec2{-10.0f, -7.0f}, Vec2{-5.0f, -14.0f}, Vec2{5.0f, -14.0f},
                                       Vec2{10.0f, -7.0f}};

void DrawShape(Emerald::Renderer2D& r, const Vec2& center, f32 scale, const Vec4& color)
{
    for (usize i = 0; i < kHull.size(); ++i)
        r.DrawLine(center + kHull[i] * scale, center + kHull[(i + 1) % kHull.size()] * scale,
                   color);
    r.DrawLine(center + kHull[0] * scale, center + kHull[3] * scale, color); // the middle line
    for (usize i = 0; i + 1 < kDome.size(); ++i)
        r.DrawLine(center + kDome[i] * scale, center + kDome[i + 1] * scale, color);
}

} // namespace

void Saucer::Draw(Emerald::Renderer2D& r, const Vec4& color) const
{
    const f32 scale = Size == SaucerSize::Large ? 1.0f : 0.5f;
    DrawShape(r, Position, scale, color);
    // Only vertical wrapping: it enters and leaves through the side edges.
    const f32 reach = 16.0f * scale; // top of the dome / bottom of the hull
    if (Position.y < reach)
        DrawShape(r, Position + Vec2(0.0f, kPlayfieldSize.y), scale, color);
    else if (Position.y > kPlayfieldSize.y - reach)
        DrawShape(r, Position - Vec2(0.0f, kPlayfieldSize.y), scale, color);
}

f32 SmallSaucerAimError(u32 score)
{
    // Linear from +-20 degrees at 0 points to +-2 degrees at 40000 and beyond.
    const f32 t = Emerald::Min(static_cast<f32>(score) / static_cast<f32>(kAlwaysSmallScore), 1.0f);
    return 0.35f + (0.035f - 0.35f) * t;
}

f32 SmallSaucerChance(u32 score)
{
    if (score >= kAlwaysSmallScore)
        return 1.0f;
    // 10% at the start, rising steadily.
    return 0.1f + 0.9f * static_cast<f32>(score) / static_cast<f32>(kAlwaysSmallScore);
}

Vec2 AimDirection(const Vec2& from, const Vec2& target, f32 errorAngle)
{
    const Vec2 d = WrappedDelta(from, target);
    const f32 angle = std::atan2(d.y, d.x) + errorAngle;
    return {std::cos(angle), std::sin(angle)};
}

} // namespace Asteroids
