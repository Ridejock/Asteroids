#include "RockBounce.h"

#include <optional>

#include <Emerald/Physics/Collision.h>

#include "Playfield.h"

namespace Asteroids {

RockBounce::RockBounce() : m_Hash(64.0f, kPlayfieldSize)
{
}

u32 RockBounce::Step(std::span<Asteroid* const> rocks)
{
    using Emerald::Circle;

    // Ids are indices into `rocks`; drop the ones past the end (rocks that were destroyed).
    const u32 count = static_cast<u32>(rocks.size());
    for (u32 i = count; i < m_Count; ++i)
        m_Hash.Remove(i);
    m_Count = count;
    for (u32 i = 0; i < count; ++i)
        m_Hash.Update(i, Emerald::Aabb::FromCenter(rocks[i]->Position, Vec2(rocks[i]->Radius)));

    u32 contacts = 0;
    m_Hash.GetPairs(m_Pairs);
    for (const auto& [i, j] : m_Pairs) {
        Asteroid& a = *rocks[i];
        Asteroid& b = *rocks[j];
        if (a.Family != 0 && a.Family == b.Family && (a.FamilyTime > 0.0f || b.FamilyTime > 0.0f))
            continue;
        // `a` at the origin, `b` at its nearest wrapped copy.
        const std::optional<Emerald::Contact> c = Emerald::Collide(
            Circle{{0.0f, 0.0f}, a.Radius}, Circle{WrappedDelta(a.Position, b.Position), b.Radius});
        if (!c)
            continue;
        ++contacts;

        const f32 inverseA = 1.0f / (a.Radius * a.Radius);
        const f32 inverseB = 1.0f / (b.Radius * b.Radius);
        const f32 inverseSum = inverseA + inverseB;

        // Push apart, so they don't stay stuck in each other.
        const f32 push = Emerald::Max(c->Depth - kSlop, 0.0f) * kCorrection / inverseSum;
        a.Position = Wrap(a.Position - c->Normal * (push * inverseA));
        b.Position = Wrap(b.Position + c->Normal * (push * inverseB));

        // Bounce only if they are approaching (not when already moving apart).
        const f32 approach = Emerald::Dot(b.Velocity - a.Velocity, c->Normal);
        if (approach < 0.0f) {
            const f32 impulse = -(1.0f + kRestitution) * approach / inverseSum;
            a.Velocity -= c->Normal * (impulse * inverseA);
            b.Velocity += c->Normal * (impulse * inverseB);
        }
    }
    return contacts;
}

} // namespace Asteroids
