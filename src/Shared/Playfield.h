#pragma once

#include <cmath>

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Vec2.h>

namespace Asteroids {

using Emerald::Vec2;

// The game runs in a fixed logical playfield of this many "pixels", whatever the window size.
// Main.cpp scales it uniformly to fit the window (with black bars if the aspect ratio differs),
// so the game plays the same at any resolution. (0, 0) is the top-left corner, +Y is down.
inline constexpr Vec2 kPlayfieldSize{1280.0f, 720.0f};
inline constexpr Vec2 kPlayfieldCenter = kPlayfieldSize * 0.5f;

// Wraps a position back into the playfield: leaving on the right re-enters on the left, etc.
[[nodiscard]] inline Vec2 Wrap(Vec2 p)
{
    p.x -= std::floor(p.x / kPlayfieldSize.x) * kPlayfieldSize.x;
    p.y -= std::floor(p.y / kPlayfieldSize.y) * kPlayfieldSize.y;
    return p;
}

// Shortest vector from `from` to `to` on the wrapping playfield (two objects near opposite
// edges are actually close to each other).
[[nodiscard]] inline Vec2 WrappedDelta(const Vec2& from, const Vec2& to)
{
    Vec2 d = to - from;
    if (d.x > 0.5f * kPlayfieldSize.x)
        d.x -= kPlayfieldSize.x;
    else if (d.x < -0.5f * kPlayfieldSize.x)
        d.x += kPlayfieldSize.x;
    if (d.y > 0.5f * kPlayfieldSize.y)
        d.y -= kPlayfieldSize.y;
    else if (d.y < -0.5f * kPlayfieldSize.y)
        d.y += kPlayfieldSize.y;
    return d;
}

// Circle vs circle on the wrapping playfield.
[[nodiscard]] inline bool CirclesOverlap(const Vec2& a, f32 radiusA, const Vec2& b, f32 radiusB)
{
    const f32 r = radiusA + radiusB;
    return Emerald::LengthSquared(WrappedDelta(a, b)) < r * r;
}

// Calls draw(position) once for an object, plus once more for every "ghost" copy on the
// opposite side when it overlaps an edge - so it slides smoothly off one edge and onto the other.
template <typename DrawFn> void ForEachWrappedCopy(const Vec2& position, f32 radius, DrawFn&& draw)
{
    f32 xs[2] = {0.0f, 0.0f};
    f32 ys[2] = {0.0f, 0.0f};
    i32 xCount = 1;
    i32 yCount = 1;
    if (position.x < radius)
        xs[xCount++] = kPlayfieldSize.x;
    else if (position.x > kPlayfieldSize.x - radius)
        xs[xCount++] = -kPlayfieldSize.x;
    if (position.y < radius)
        ys[yCount++] = kPlayfieldSize.y;
    else if (position.y > kPlayfieldSize.y - radius)
        ys[yCount++] = -kPlayfieldSize.y;

    for (i32 y = 0; y < yCount; ++y)
        for (i32 x = 0; x < xCount; ++x)
            draw(position + Vec2(xs[x], ys[y]));
}

} // namespace Asteroids
