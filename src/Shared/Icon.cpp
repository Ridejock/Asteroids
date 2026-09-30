#include "Icon.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <Emerald/Math/Common.h>
#include <Emerald/Math/Vec2.h>

namespace Asteroids {

namespace {

using Emerald::Vec2;

// Distance from p to the segment a-b.
f32 SegmentDistance(const Vec2& p, const Vec2& a, const Vec2& b)
{
    const Vec2 ab = b - a;
    const f32 t = std::clamp(Emerald::Dot(p - a, ab) / Emerald::Dot(ab, ab), 0.0f, 1.0f);
    return Emerald::Length(p - (a + ab * t));
}

// Coverage of an antialiased rounded rectangle [0, 1]^2 with corner radius `radius`.
f32 RoundedSquare(const Vec2& p, f32 radius, f32 pixel)
{
    const Vec2 q{std::abs(p.x - 0.5f) - (0.5f - radius), std::abs(p.y - 0.5f) - (0.5f - radius)};
    const f32 outside = Emerald::Length(Vec2(std::max(q.x, 0.0f), std::max(q.y, 0.0f))) +
                        std::min(std::max(q.x, q.y), 0.0f) - radius;
    return std::clamp(0.5f - outside / pixel, 0.0f, 1.0f);
}

} // namespace

Emerald::Image MakeIcon(u32 size)
{
    // The ship's outline (Ship.cpp's hull and cross bar, facing +X), in icon space: 0..1,
    // rotated to point up-right and centered.
    const std::array<Vec2, 5> model = {Vec2{-10.0f, -10.0f}, Vec2{16.0f, 0.0f}, Vec2{-10.0f, 10.0f},
                                       Vec2{-6.0f, -8.4f}, Vec2{-6.0f, 8.4f}};
    const f32 angle = -Emerald::HalfPi * 0.5f; // up-right (+Y is down)
    const f32 c = std::cos(angle);
    const f32 s = std::sin(angle);
    std::array<Vec2, 5> points{};
    for (usize i = 0; i < model.size(); ++i) {
        const Vec2 m = model[i] - Vec2(3.0f, 0.0f); // center the hull's bounding box
        points[i] = Vec2(m.x * c - m.y * s, m.x * s + m.y * c) * (0.62f / 26.0f) + Vec2(0.5f);
    }
    const std::array<std::array<usize, 2>, 3> segments = {{{0, 1}, {1, 2}, {3, 4}}};

    Emerald::Image image;
    image.Width = static_cast<i32>(size);
    image.Height = static_cast<i32>(size);
    image.Pixels.resize(static_cast<usize>(size) * size * 4);
    const f32 pixel = 1.0f / static_cast<f32>(size);
    // Lines stay at least ~1.3 pixels wide, so small icons (16 x 16) remain readable.
    const f32 lineHalfWidth = std::max(0.022f, 0.65f * pixel);

    for (u32 y = 0; y < size; ++y) {
        for (u32 x = 0; x < size; ++x) {
            const Vec2 p{(static_cast<f32>(x) + 0.5f) * pixel,
                         (static_cast<f32>(y) + 0.5f) * pixel};
            f32 distance = 1e9f;
            for (const auto& [a, b] : segments)
                distance = std::min(distance, SegmentDistance(p, points[a], points[b]));

            const f32 line = std::clamp((lineHalfWidth - distance) / pixel + 0.5f, 0.0f, 1.0f);
            const f32 glow = 0.55f * std::exp(-distance * distance / (0.06f * 0.06f));
            const f32 inside = RoundedSquare(p, 0.18f, pixel);

            // Dark blue-black background with a slightly lighter center, cyan-white glow, white
            // line.
            const f32 vignette = 1.0f - 0.6f * Emerald::Length(p - Vec2(0.5f));
            f32 r = 0.02f + 0.03f * vignette + 0.25f * glow;
            f32 g = 0.03f + 0.05f * vignette + 0.55f * glow;
            f32 b = 0.07f + 0.08f * vignette + 0.75f * glow;
            r += (1.0f - r) * line;
            g += (1.0f - g) * line;
            b += (1.0f - b) * line;

            u8* out = &image.Pixels[(static_cast<usize>(y) * size + x) * 4];
            out[0] = static_cast<u8>(std::lround(std::clamp(r, 0.0f, 1.0f) * 255.0f));
            out[1] = static_cast<u8>(std::lround(std::clamp(g, 0.0f, 1.0f) * 255.0f));
            out[2] = static_cast<u8>(std::lround(std::clamp(b, 0.0f, 1.0f) * 255.0f));
            out[3] = static_cast<u8>(std::lround(inside * 255.0f));
        }
    }
    return image;
}

} // namespace Asteroids
