// Tests for the pure game logic: the high score table (ordering, top 10, text format), the
// saucer's aiming math, the circle checks and rock bounce. No window, GPU or audio needed.

#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <span>
#include <string>
#include <vector>

#include "Check.h"
#include "HighScores.h"
#include "ParticleEffects.h"
#include "Playfield.h"
#include "RockBounce.h"
#include "Saucer.h"

namespace {

using Asteroids::HighScoreTable;

bool Near(f32 a, f32 b)
{
    return std::abs(a - b) < 1e-4f;
}

void TestInsertAndSort()
{
    HighScoreTable table;
    Check(!table.Qualifies(0), "a score of 0 never qualifies");
    Check(table.Qualifies(10), "anything qualifies for an empty table");
    Check(table.Insert("BBB", 500) == 0, "first entry is rank 0");
    Check(table.Insert("AAA", 900) == 0, "a better score goes on top");
    Check(table.Insert("CCC", 100) == 2, "a worse one at the bottom");
    Check(table.Insert("DDD", 500) == 2, "a tie goes below the older entry");
    const auto& e = table.GetEntries();
    Check(e.size() == 4 && e[0].Initials == "AAA" && e[1].Initials == "BBB" &&
              e[2].Initials == "DDD" && e[3].Initials == "CCC",
          "sorted best first");
    Check(table.GetBest() == 900, "GetBest is the top score");
}

void TestTruncate()
{
    HighScoreTable table;
    for (u32 i = 1; i <= 12; ++i)
        table.Insert("ABC", i * 100);
    Check(table.GetEntries().size() == HighScoreTable::kMaxEntries, "only 10 are kept");
    Check(table.GetEntries().back().Score == 300, "the lowest two fell off");
    Check(!table.Qualifies(300), "equal to the 10th place does not qualify");
    Check(table.Qualifies(301), "better than the 10th place qualifies");
    Check(table.Insert("XYZ", 50) == HighScoreTable::kMaxEntries, "too low: not inserted");
    Check(table.Insert("XYZ", 5000) == 0 && table.GetEntries().back().Score == 400,
          "a new best pushes the 10th out");
}

void TestInitials()
{
    Check(HighScoreTable::CleanInitials("abc") == "ABC", "lower case becomes upper case");
    Check(HighScoreTable::CleanInitials("A") == "A  ", "short initials are padded");
    Check(HighScoreTable::CleanInitials("ABCD") == "ABC", "long initials are cut");
    Check(HighScoreTable::CleanInitials("A#1") == "A  ", "other characters become spaces");
}

void TestSerializeAndParse()
{
    HighScoreTable table;
    table.Insert("ABC", 12340);
    table.Insert("E A", 9870);
    table.Insert("  Z", 10);
    const std::string text = table.Serialize();
    Check(text == "ABC 12340\nE A 9870\n  Z 10\n", "serialized one line per entry");

    const HighScoreTable back = HighScoreTable::Parse(text);
    Check(back.GetEntries().size() == 3 && back.GetEntries()[1].Initials == "E A" &&
              back.GetEntries()[2].Initials == "  Z" && back.GetEntries()[0].Score == 12340,
          "round trip keeps initials (with spaces) and scores");

    // Damaged or edited files: bad lines are skipped, the rest is sorted and cut to 10.
    const HighScoreTable messy = HighScoreTable::Parse("LOW 5\r\n"   // Windows line ending
                                                       "garbage\n"   // not an entry
                                                       "\n"          // empty line
                                                       "TOP 99999\n" // out of order
                                                       "BAD 12x\n"   // junk after the number
                                                       "NEG -5\n"    // not a number
                                                       "BIG 99999999999999999\n" // too big for u32
                                                       "abc 70\n" // lower case gets cleaned
                                                       "EMPTY\n"
                                                       "NO_SPACE123\n"
                                                       "END 42"); // no final newline
    const auto& e = messy.GetEntries();
    Check(e.size() == 4, "four valid lines survive");
    Check(e.size() == 4 && e[0].Initials == "TOP" && e[1].Initials == "ABC" &&
              e[2].Initials == "END" && e[3].Initials == "LOW" && e[3].Score == 5,
          "valid lines sorted, cleaned, CR stripped");

    std::string many;
    for (u32 i = 0; i < 15; ++i)
        many += "AAA " + std::to_string(i + 1) + "\n";
    Check(HighScoreTable::Parse(many).GetEntries().size() == 10, "a long file is cut to 10");
    Check(HighScoreTable::Parse("").GetEntries().empty(), "empty text: empty table");
}

void TestSaucerAim()
{
    using Asteroids::AimDirection;
    using Asteroids::kPlayfieldSize;
    using Emerald::Vec2;

    const Vec2 right = AimDirection({100.0f, 300.0f}, {400.0f, 300.0f}, 0.0f);
    Check(Near(right.x, 1.0f) && Near(right.y, 0.0f), "aims straight at the target");

    // Across the wrap: from near the left edge to near the right edge is a short hop left.
    const Vec2 left = AimDirection({10.0f, 300.0f}, {kPlayfieldSize.x - 10.0f, 300.0f}, 0.0f);
    Check(Near(left.x, -1.0f) && Near(left.y, 0.0f), "aims the short way around the edge");

    // +HalfPi turns right (+x) into down (+y, since +Y is down on screen).
    const Vec2 turned = AimDirection({100.0f, 300.0f}, {400.0f, 300.0f}, Emerald::HalfPi);
    Check(Near(turned.x, 0.0f) && Near(turned.y, 1.0f), "error angle rotates the aim");

    const f32 early = Asteroids::SmallSaucerAimError(0);
    const f32 mid = Asteroids::SmallSaucerAimError(20000);
    const f32 late = Asteroids::SmallSaucerAimError(40000);
    Check(early > mid && mid > late && late > 0.0f, "aim tightens as the score grows");
    Check(Near(late, Asteroids::SmallSaucerAimError(1000000)), "and stops tightening at 40000");

    Check(Asteroids::SmallSaucerChance(0) < 0.2f, "small saucers are rare at first");
    Check(Asteroids::SmallSaucerChance(20000) > Asteroids::SmallSaucerChance(0),
          "and more likely with a higher score");
    Check(Asteroids::SmallSaucerChance(40000) == 1.0f, "and certain from 40000");
}

void TestParticleEffects()
{
    using Asteroids::SoundEvent;
    Asteroids::ParticleEffects effects;
    const auto count = [&] { return effects.GetParticles().GetCount(); };
    effects.OnSound({.Event = SoundEvent::Fire});
    Check(count() == 0, "shots make no particles");
    effects.OnSound({.Event = SoundEvent::ExplosionSmall});
    const u32 small = count();
    effects.Clear();
    effects.OnSound({.Event = SoundEvent::ExplosionLarge});
    Check(small > 0 && count() > small, "bigger rocks make more sparks and dust");
    effects.Clear();
    effects.OnSound({.Event = SoundEvent::ShipExplosion});
    Check(count() > 100, "the ship explodes in a big burst");
    effects.Clear();
    effects.OnSound({.Event = SoundEvent::SaucerExplosion});
    Check(count() > 0, "a hit saucer throws debris");
    effects.SetEnabled(false);
    Check(count() == 0, "switching particles off clears them");
    effects.OnSound({.Event = SoundEvent::ShipExplosion});
    Check(count() == 0, "and no new ones appear");
}

// The circle check as it was before it moved onto Emerald's Collision.h.
bool OldCirclesOverlap(const Asteroids::Vec2& a, f32 radiusA, const Asteroids::Vec2& b, f32 radiusB)
{
    const f32 r = radiusA + radiusB;
    return Emerald::LengthSquared(Asteroids::WrappedDelta(a, b)) < r * r;
}

void TestCirclesOverlapUnchanged()
{
    using Asteroids::CirclesOverlap;
    using Asteroids::Vec2;
    std::mt19937 rng(2026);
    std::uniform_real_distribution<f32> x(-50.0f, 1330.0f), y(-50.0f, 770.0f), r(0.0f, 60.0f);
    u32 mismatches = 0;
    u32 hits = 0;
    for (u32 i = 0; i < 200000; ++i) {
        const Vec2 p{x(rng), y(rng)};
        const Vec2 b{x(rng), y(rng)};
        const f32 ra = r(rng);
        const f32 rb = i % 3 == 0 ? 0.0f : r(rng); // bullets are points
        const bool now = CirclesOverlap(p, ra, b, rb);
        mismatches += now != OldCirclesOverlap(p, ra, b, rb) ? 1 : 0;
        hits += now ? 1 : 0;
    }
    Check(mismatches == 0 && hits > 1000, "CirclesOverlap matches the old check on 200k samples");
    // Exactly touching (no overlap), just overlapping, and both across the wrap edges.
    const std::array<std::array<f32, 6>, 6> cases{{
        {100.0f, 100.0f, 10.0f, 130.0f, 100.0f, 20.0f},
        {100.0f, 100.0f, 10.0f, 129.9f, 100.0f, 20.0f},
        {5.0f, 300.0f, 5.0f, 1275.0f, 300.0f, 5.0f},
        {5.0f, 300.0f, 6.0f, 1275.0f, 300.0f, 5.0f},
        {640.0f, 2.0f, 3.0f, 640.0f, 718.0f, 1.0f},
        {640.0f, 2.0f, 0.0f, 640.0f, 2.0f, 0.0f},
    }};
    bool same = true;
    for (const auto& c : cases)
        same = same && CirclesOverlap({c[0], c[1]}, c[2], {c[3], c[4]}, c[5]) ==
                           OldCirclesOverlap({c[0], c[1]}, c[2], {c[3], c[4]}, c[5]);
    Check(same, "CirclesOverlap matches the old check when touching and across edges");
    Check(!CirclesOverlap({100.0f, 100.0f}, 10.0f, {130.0f, 100.0f}, 20.0f) &&
              CirclesOverlap({5.0f, 300.0f}, 6.0f, {1275.0f, 300.0f}, 5.0f),
          "touching circles don't overlap; circles overlap across the wrap edge");
}

Asteroids::Asteroid MakeRock(Asteroids::Vec2 position, Asteroids::Vec2 velocity, f32 radius)
{
    Asteroids::Asteroid rock;
    rock.Position = position;
    rock.Velocity = velocity;
    rock.Radius = radius;
    return rock;
}

// Moves the rocks like the game does (120 Hz) with rock bounce, for `seconds`.
void Simulate(std::span<Asteroids::Asteroid> rocks, f32 seconds)
{
    Asteroids::RockBounce bounce;
    std::vector<Asteroids::Asteroid*> pointers;
    for (Asteroids::Asteroid& rock : rocks)
        pointers.push_back(&rock);
    for (f32 t = 0.0f; t < seconds; t += 1.0f / 120.0f) {
        for (Asteroids::Asteroid& rock : rocks)
            rock.Update(1.0f / 120.0f);
        bounce.Step(pointers);
    }
}

void TestRockBounce()
{
    using Asteroids::Asteroid;
    using Asteroids::Vec2;
    // Head-on, equal size: they swap (most of) their velocities and end up apart.
    std::array<Asteroid, 2> pair{MakeRock({500.0f, 300.0f}, {50.0f, 0.0f}, 24.0f),
                                 MakeRock({600.0f, 300.0f}, {-50.0f, 0.0f}, 24.0f)};
    Simulate(pair, 2.0f);
    Check(pair[0].Velocity.x < -40.0f && pair[1].Velocity.x > 40.0f &&
              Near(pair[0].Velocity.x + pair[1].Velocity.x, 0.0f),
          "equal rocks bounce back head-on, momentum kept");
    Check(!Asteroids::CirclesOverlap(pair[0].Position, 24.0f, pair[1].Position, 24.0f),
          "and end up apart");

    // A small rock hitting a big one at rest bounces off; the big one hardly moves.
    std::array<Asteroid, 2> sizes{MakeRock({600.0f, 300.0f}, {0.0f, 0.0f}, 46.0f),
                                  MakeRock({500.0f, 300.0f}, {100.0f, 0.0f}, 12.0f)};
    Simulate(sizes, 1.5f);
    Check(sizes[1].Velocity.x < -50.0f && sizes[0].Velocity.x > 0.0f && sizes[0].Velocity.x < 20.0f,
          "mass ~ area: a small rock glances off a big one");

    // Across the left/right edge.
    std::array<Asteroid, 2> wrap{MakeRock({1260.0f, 360.0f}, {60.0f, 0.0f}, 24.0f),
                                 MakeRock({20.0f, 360.0f}, {-60.0f, 0.0f}, 24.0f)};
    Simulate(wrap, 1.5f);
    Check(wrap[0].Velocity.x < 0.0f && wrap[1].Velocity.x > 0.0f,
          "rocks bounce across the wrap edge");

    // Overlapping but already moving apart: pushed apart, velocities left alone.
    std::array<Asteroid, 2> apart{MakeRock({500.0f, 300.0f}, {-10.0f, 0.0f}, 24.0f),
                                  MakeRock({520.0f, 300.0f}, {10.0f, 0.0f}, 24.0f)};
    Simulate(apart, 0.5f);
    Check(Near(apart[0].Velocity.x, -10.0f) && Near(apart[1].Velocity.x, 10.0f) &&
              !Asteroids::CirclesOverlap(apart[0].Position, 24.0f, apart[1].Position, 24.0f),
          "separating rocks get no impulse, only pushed apart");

    // Fragments of one break start on the same spot: they ignore each other and fly apart.
    std::array<Asteroid, 2> family{MakeRock({500.0f, 300.0f}, {-80.0f, 30.0f}, 24.0f),
                                   MakeRock({500.0f, 300.0f}, {90.0f, -40.0f}, 24.0f)};
    for (Asteroid& rock : family) {
        rock.Family = 7;
        rock.FamilyTime = Asteroids::RockBounce::kFamilyTime;
    }
    Simulate(family, 1.0f);
    Check(family[0].Velocity == Vec2(-80.0f, 30.0f) && family[1].Velocity == Vec2(90.0f, -40.0f),
          "fragments of one break fly apart untouched");
    Check(family[0].FamilyTime == 0.0f, "the family time runs out");

    // A crowd never ends up stuck inside each other.
    std::vector<Asteroid> crowd;
    std::mt19937 rng(3);
    std::uniform_real_distribution<f32> px(0.0f, 1280.0f), py(0.0f, 720.0f), v(-120.0f, 120.0f);
    for (u32 i = 0; i < 40; ++i)
        crowd.push_back(MakeRock({px(rng), py(rng)}, {v(rng), v(rng)}, i % 3 == 0 ? 46.0f : 24.0f));
    Simulate(crowd, 10.0f);
    f32 worst = 0.0f;
    for (usize i = 0; i < crowd.size(); ++i)
        for (usize j = i + 1; j < crowd.size(); ++j) {
            const f32 d =
                Emerald::Length(Asteroids::WrappedDelta(crowd[i].Position, crowd[j].Position));
            worst = Emerald::Max(worst, crowd[i].Radius + crowd[j].Radius - d);
        }
    Check(worst < 8.0f, "a crowd of rocks never stays deep inside each other");
}

} // namespace

int main()
{
    TestInsertAndSort();
    TestTruncate();
    TestInitials();
    TestSerializeAndParse();
    TestSaucerAim();
    TestParticleEffects();
    TestCirclesOverlapUnchanged();
    TestRockBounce();
    std::printf("%d failed\n", g_Failures);
    return g_Failures == 0 ? 0 : 1;
}
