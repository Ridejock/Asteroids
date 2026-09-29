#pragma once

#include <cmath>
#include <random>

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Common.h>
#include <Emerald/Math/Vec2.h>

namespace Asteroids {

// Small convenience wrapper around std::mt19937.
class Random {
public:
    Random() : m_Engine(std::random_device{}()) {}
    explicit Random(u32 seed) : m_Engine(seed) {}

    // Uniform in [min, max).
    [[nodiscard]] f32 Float(f32 min, f32 max)
    {
        return std::uniform_real_distribution<f32>(min, max)(m_Engine);
    }
    // Uniform in [min, max] (both included).
    [[nodiscard]] i32 Int(i32 min, i32 max)
    {
        return std::uniform_int_distribution<i32>(min, max)(m_Engine);
    }
    // True with probability p (0..1).
    [[nodiscard]] bool Chance(f32 p) { return Float(0.0f, 1.0f) < p; }
    // A unit vector pointing in a random direction.
    [[nodiscard]] Emerald::Vec2 Direction()
    {
        const f32 angle = Float(0.0f, Emerald::TwoPi);
        return {std::cos(angle), std::sin(angle)};
    }

private:
    std::mt19937 m_Engine;
};

} // namespace Asteroids
