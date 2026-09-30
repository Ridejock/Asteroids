#include "ParticleEffects.h"

#include "Asteroid.h"

namespace Asteroids {

namespace {

using Emerald::EmitterShape;
using Emerald::ParticleEmitterConfig;

// Every effect is one config: how particles start and how they fade (see ParticleSystem.h).
// Sizes are streak lengths in playfield pixels (the vector version), colors go from Start to End
// over each particle's life. Drawn additively, so they glow where they overlap.

// Hot sparks: fast, short-lived, white-yellow cooling to orange.
const ParticleEmitterConfig kSparks{.StartColor = {1.0f, 0.92f, 0.65f, 1.0f},
                                    .EndColor = {1.0f, 0.4f, 0.12f, 0.0f},
                                    .Speed = {90.0f, 280.0f},
                                    .Lifetime = {0.25f, 0.6f},
                                    .Drag = 3.0f,
                                    .StartSize = 9.0f,
                                    .EndSize = 2.0f};
// Rock dust: slow, faint, the rocks' blue-grey, spread over part of the rock.
const ParticleEmitterConfig kDust{.StartColor = {0.78f, 0.83f, 0.9f, 0.55f},
                                  .EndColor = {0.45f, 0.5f, 0.6f, 0.0f},
                                  .Shape = EmitterShape::Circle,
                                  .Speed = {15.0f, 70.0f},
                                  .Lifetime = {0.5f, 1.1f},
                                  .Drag = 1.5f,
                                  .StartSize = 3.0f,
                                  .EndSize = 1.0f};
// Ship explosion: a bright fireball...
const ParticleEmitterConfig kFireball{.StartColor = {1.0f, 1.0f, 0.9f, 1.0f},
                                      .EndColor = {1.0f, 0.35f, 0.1f, 0.0f},
                                      .Shape = EmitterShape::Circle,
                                      .Radius = 6.0f,
                                      .Speed = {40.0f, 340.0f},
                                      .Lifetime = {0.4f, 1.3f},
                                      .Drag = 1.8f,
                                      .StartSize = 10.0f,
                                      .EndSize = 2.0f};
// ...and a thin shock ring: all particles at nearly the same speed.
const ParticleEmitterConfig kShockRing{.StartColor = {0.6f, 0.85f, 1.0f, 0.9f},
                                       .EndColor = {0.3f, 0.5f, 1.0f, 0.0f},
                                       .Speed = {175.0f, 190.0f},
                                       .Lifetime = {0.45f, 0.55f},
                                       .Drag = 0.8f,
                                       .StartSize = 6.0f,
                                       .EndSize = 2.0f};
// Saucer debris: white-cyan shards thrown out of its hull.
const ParticleEmitterConfig kSaucerDebris{.StartColor = {0.85f, 1.0f, 1.0f, 1.0f},
                                          .EndColor = {0.25f, 0.55f, 1.0f, 0.0f},
                                          .Shape = EmitterShape::Circle,
                                          .Radius = 12.0f,
                                          .Speed = {40.0f, 220.0f},
                                          .Lifetime = {0.4f, 1.0f},
                                          .Drag = 1.2f,
                                          .StartSize = 8.0f,
                                          .EndSize = 2.0f};
// Engine exhaust: a narrow cone out of the back, cooling from yellow to dark red.
const ParticleEmitterConfig kExhaust{.StartColor = {1.0f, 0.8f, 0.4f, 0.9f},
                                     .EndColor = {0.9f, 0.25f, 0.1f, 0.0f},
                                     .Speed = {90.0f, 160.0f},
                                     .Angle = {-0.22f, 0.22f},
                                     .Lifetime = {0.25f, 0.45f},
                                     .Drag = 2.0f,
                                     .StartSize = 5.0f,
                                     .EndSize = 1.5f,
                                     .Rate = 110.0f};

} // namespace

void ParticleEffects::SetEnabled(bool enabled)
{
    m_Enabled = enabled;
    if (!enabled) {
        m_Particles.Clear();
        m_Exhaust.Reset();
    }
}

void ParticleEffects::Update(const Game& game, f32 dt)
{
    if (!m_Enabled)
        return;
    for (const GameSound& sound : game.GetSounds())
        OnSound(sound);

    if (game.IsThrusting()) {
        const Ship& ship = game.GetShip();
        // Backwards (angle + Pi), carried along with the ship so the trail stays behind it.
        m_Particles.EmitContinuous(kExhaust, m_Exhaust, ship.EnginePosition(), dt,
                                   ship.Angle + Emerald::Pi, ship.Velocity);
    } else {
        m_Exhaust.Reset();
    }
    m_Particles.Update(dt);
}

void ParticleEffects::OnSound(const GameSound& sound)
{
    if (!m_Enabled)
        return;
    // Rocks: more and wider for bigger ones; the pieces keep some of the rock's drift.
    const auto rock = [&](AsteroidSize size, u32 sparks, u32 dust) {
        ParticleEmitterConfig spread = kDust;
        spread.Radius = RadiusFor(size) * 0.5f;
        const Vec2 drift = sound.Velocity * 0.5f;
        m_Particles.Emit(kSparks, sound.Position, sparks, 0.0f, drift);
        m_Particles.Emit(spread, sound.Position, dust, 0.0f, drift);
    };
    switch (sound.Event) {
    case SoundEvent::ExplosionLarge:
        rock(AsteroidSize::Large, 22, 24);
        break;
    case SoundEvent::ExplosionMedium:
        rock(AsteroidSize::Medium, 14, 14);
        break;
    case SoundEvent::ExplosionSmall:
        rock(AsteroidSize::Small, 8, 8);
        break;
    case SoundEvent::ShipExplosion:
        m_Particles.Emit(kFireball, sound.Position, 90, 0.0f, sound.Velocity * 0.3f);
        m_Particles.Emit(kShockRing, sound.Position, 48, 0.0f, sound.Velocity * 0.3f);
        break;
    case SoundEvent::SaucerExplosion:
        m_Particles.Emit(kSaucerDebris, sound.Position, 40, 0.0f, sound.Velocity * 0.5f);
        m_Particles.Emit(kSparks, sound.Position, 16, 0.0f, sound.Velocity * 0.5f);
        break;
    default:
        break;
    }
}

} // namespace Asteroids
