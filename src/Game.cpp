#include "Game.h"

#include <cmath>
#include <string>

#include <Emerald/Core/Log.h>
#include <Emerald/Math/Common.h>

#include "Playfield.h"
#include "VectorFont.h"

namespace Asteroids {

namespace {

constexpr u32 kStartLives = 3;
constexpr u32 kExtraLifeEvery = 10000; // points
constexpr u32 kMaxAsteroidsPerWave = 11;
constexpr f32 kRespawnDelay = 2.0f;     // seconds after a crash
constexpr f32 kInvulnerableTime = 3.0f; // seconds after (re)spawning
constexpr f32 kHyperspaceCooldown = 1.0f;
constexpr f32 kNextWaveDelay = 2.0f;
constexpr f32 kWaveBannerTime = 2.0f;
constexpr f32 kSafeSpawnDistance = 220.0f; // new asteroids never appear this close to the ship
constexpr f32 kSlowestBeat = 1.0f;         // seconds between beats at the start of a wave
constexpr f32 kFastestBeat = 0.25f;        // ... with (almost) nothing left
constexpr f32 kFirstBeatDelay = 0.6f;

// A large rock is itself plus 2 medium plus 4 small: 7 "rocks of mass" left to destroy.
u32 MassOf(AsteroidSize size)
{
    switch (size) {
    case AsteroidSize::Large:
        return 7;
    case AsteroidSize::Medium:
        return 3;
    default:
        return 1;
    }
}

const Vec4 kShipColor{0.95f, 0.97f, 1.0f, 1.0f};
const Vec4 kAsteroidColor{0.78f, 0.83f, 0.9f, 1.0f};
const Vec4 kBulletColor{1.0f, 1.0f, 1.0f, 1.0f};
const Vec4 kTextColor{0.95f, 0.97f, 1.0f, 1.0f};

} // namespace

Game::Game(u32 seed) : m_Random(seed)
{
    NewGame();
}

void Game::NewGame()
{
    m_State = State::Playing;
    m_Score = 0;
    m_Lives = kStartLives;
    m_NextExtraLife = kExtraLifeEvery;
    m_Wave = 0;
    m_Bullets.clear();
    m_Particles.clear();
    m_Asteroids.clear();

    m_Ship.Reset(kPlayfieldCenter);
    m_ShipAlive = true;
    m_InvulnerableTimer = kInvulnerableTime;
    m_RespawnTimer = 0.0f;
    StartWave();
}

void Game::StartWave()
{
    ++m_Wave;
    m_WaveBannerTimer = kWaveBannerTime;
    const u32 count = Emerald::Min(3u + m_Wave, kMaxAsteroidsPerWave);

    // Large asteroids along the edges, never right on top of the ship.
    for (u32 i = 0; i < count; ++i) {
        Vec2 position;
        do {
            const bool onVerticalEdge = m_Random.Chance(0.5f);
            position = onVerticalEdge ? Vec2(m_Random.Chance(0.5f) ? 0.0f : kPlayfieldSize.x,
                                             m_Random.Float(0.0f, kPlayfieldSize.y))
                                      : Vec2(m_Random.Float(0.0f, kPlayfieldSize.x),
                                             m_Random.Chance(0.5f) ? 0.0f : kPlayfieldSize.y);
        } while (Emerald::Length(WrappedDelta(position, m_Ship.Position)) < kSafeSpawnDistance);
        m_Asteroids.push_back(MakeAsteroid(m_Random, AsteroidSize::Large, Wrap(position)));
    }
    // The heartbeat starts slow again with every wave.
    m_WaveMass = count * MassOf(AsteroidSize::Large);
    m_BeatTimer = kFirstBeatDelay;
    m_BeatHigh = true;
    EM_INFO("Wave {}: {} asteroids", m_Wave, count);
}

void Game::Update(const GameInput& input, f32 dt)
{
    m_Time += dt;

    if (m_State == State::GameOver && input.StartPressed)
        NewGame();

    if (m_State == State::Playing)
        UpdateShip(input, dt);
    UpdateObjects(dt); // asteroids keep drifting on the game over screen
    if (m_State == State::Playing) {
        HandleCollisions();
        UpdateHeartbeat(dt);
    }

    // Next wave once the field is clear.
    m_WaveBannerTimer = Emerald::Max(m_WaveBannerTimer - dt, 0.0f);
    if (m_State == State::Playing && m_Asteroids.empty()) {
        if (m_NextWaveTimer <= 0.0f) {
            m_NextWaveTimer = kNextWaveDelay;
        } else {
            m_NextWaveTimer -= dt;
            if (m_NextWaveTimer <= 0.0f)
                StartWave();
        }
    }
}

void Game::UpdateShip(const GameInput& input, f32 dt)
{
    if (!m_ShipAlive) {
        // Waiting to respawn (lives were already checked when the ship crashed).
        m_RespawnTimer -= dt;
        if (m_RespawnTimer <= 0.0f) {
            m_Ship.Reset(kPlayfieldCenter);
            m_ShipAlive = true;
            m_InvulnerableTimer = kInvulnerableTime;
        }
        return;
    }

    m_Ship.Update(input.Ship, dt);
    m_InvulnerableTimer = Emerald::Max(m_InvulnerableTimer - dt, 0.0f);
    m_HyperspaceCooldown = Emerald::Max(m_HyperspaceCooldown - dt, 0.0f);
    // A new random flame length every step, and sometimes none: that is the flicker.
    m_FlameLength = m_Ship.Thrusting && m_Random.Chance(0.8f) ? m_Random.Float(6.0f, 16.0f) : 0.0f;

    if (input.FirePressed)
        FireBullet();
    if (input.HyperspacePressed)
        Hyperspace();
}

void Game::FireBullet()
{
    if (m_Bullets.size() >= Bullet::kMaxAlive)
        return;
    Bullet bullet;
    bullet.Position = m_Ship.NosePosition();
    bullet.Velocity = m_Ship.Velocity + m_Ship.Forward() * Bullet::kSpeed;
    m_Bullets.push_back(bullet);
    PlaySound(SoundEvent::Fire, bullet.Position);
}

void Game::Hyperspace()
{
    if (m_HyperspaceCooldown > 0.0f)
        return;
    // Vanish in a puff and reappear somewhere random, at rest. Might land next to a rock!
    SpawnExplosion(m_Ship.Position, 8, 60.0f);
    PlaySound(SoundEvent::Hyperspace, m_Ship.Position);
    m_Ship.Position = {m_Random.Float(0.0f, kPlayfieldSize.x),
                       m_Random.Float(0.0f, kPlayfieldSize.y)};
    m_Ship.Velocity = {};
    m_HyperspaceCooldown = kHyperspaceCooldown;
}

void Game::UpdateObjects(f32 dt)
{
    for (Asteroid& asteroid : m_Asteroids)
        asteroid.Update(dt);

    // Bullets and particles expire; remove them by swapping with the last element (order does
    // not matter, and nothing has to shift).
    for (usize i = 0; i < m_Bullets.size();) {
        Bullet& b = m_Bullets[i];
        b.Position = Wrap(b.Position + b.Velocity * dt);
        b.TimeLeft -= dt;
        if (b.TimeLeft <= 0.0f) {
            b = m_Bullets.back();
            m_Bullets.pop_back();
        } else {
            ++i;
        }
    }
    for (usize i = 0; i < m_Particles.size();) {
        Particle& p = m_Particles[i];
        p.Position = Wrap(p.Position + p.Velocity * dt);
        p.Angle += p.Spin * dt;
        p.TimeLeft -= dt;
        if (p.TimeLeft <= 0.0f) {
            p = m_Particles.back();
            m_Particles.pop_back();
        } else {
            ++i;
        }
    }
}

void Game::HandleCollisions()
{
    const bool shipCanCrash = m_ShipAlive && m_InvulnerableTimer <= 0.0f;
    std::vector<Asteroid> fragments; // added after the loop, so they are not hit this step

    for (usize i = 0; i < m_Asteroids.size();) {
        const Asteroid& asteroid = m_Asteroids[i];
        bool destroyed = false;

        // Bullet vs asteroid: the bullet is a point, the asteroid a circle.
        for (usize b = 0; b < m_Bullets.size(); ++b) {
            if (CirclesOverlap(m_Bullets[b].Position, 0.0f, asteroid.Position, asteroid.Radius)) {
                m_Bullets[b] = m_Bullets.back();
                m_Bullets.pop_back();
                AddScore(ScoreFor(asteroid.Size));
                destroyed = true;
                break;
            }
        }
        // Ship vs asteroid: both destroyed (a slightly smaller asteroid circle feels fairer).
        if (!destroyed && shipCanCrash && m_ShipAlive &&
            CirclesOverlap(m_Ship.Position, Ship::kRadius, asteroid.Position,
                           asteroid.Radius * 0.85f)) {
            AddScore(ScoreFor(asteroid.Size));
            DestroyShip();
            destroyed = true;
        }

        if (!destroyed) {
            ++i;
            continue;
        }

        // Large -> 2 medium -> 2 small -> gone.
        if (const auto smaller = SmallerSize(asteroid.Size)) {
            for (i32 k = 0; k < 2; ++k) {
                Asteroid fragment = MakeAsteroid(m_Random, *smaller, asteroid.Position);
                fragment.Velocity += asteroid.Velocity * 0.5f; // inherit some of the momentum
                fragments.push_back(fragment);
            }
        }
        SpawnExplosion(asteroid.Position, 6 + 4 * (2 - static_cast<u32>(asteroid.Size)), 90.0f);
        const SoundEvent boom = asteroid.Size == AsteroidSize::Large ? SoundEvent::ExplosionLarge
                                : asteroid.Size == AsteroidSize::Medium
                                    ? SoundEvent::ExplosionMedium
                                    : SoundEvent::ExplosionSmall;
        PlaySound(boom, asteroid.Position);
        m_Asteroids[i] = m_Asteroids.back();
        m_Asteroids.pop_back();
    }

    m_Asteroids.insert(m_Asteroids.end(), fragments.begin(), fragments.end());
}

void Game::AddScore(u32 points)
{
    m_Score += points;
    if (m_Score >= m_NextExtraLife) {
        ++m_Lives;
        m_NextExtraLife += kExtraLifeEvery;
        PlaySound(SoundEvent::ExtraLife, kPlayfieldCenter);
        EM_INFO("Extra ship at {} points", m_Score);
    }
}

void Game::DestroyShip()
{
    m_ShipAlive = false;
    m_Ship.Thrusting = false;
    m_FlameLength = 0.0f;

    // The hull breaks into a few spinning lines, plus some dots.
    for (i32 i = 0; i < 5; ++i) {
        Particle p;
        p.Position = m_Ship.Position + m_Random.Direction() * m_Random.Float(0.0f, 8.0f);
        p.Velocity = m_Ship.Velocity * 0.3f + m_Random.Direction() * m_Random.Float(20.0f, 70.0f);
        p.Angle = m_Random.Float(0.0f, Emerald::TwoPi);
        p.Spin = m_Random.Float(-3.0f, 3.0f);
        p.Length = m_Random.Float(8.0f, 16.0f);
        p.Lifetime = p.TimeLeft = m_Random.Float(1.2f, 2.0f);
        m_Particles.push_back(p);
    }
    SpawnExplosion(m_Ship.Position, 12, 120.0f);
    PlaySound(SoundEvent::ShipExplosion, m_Ship.Position);

    --m_Lives;
    ++m_ShipsLost;
    EM_INFO("Ship destroyed, {} left (score {})", m_Lives, m_Score);
    if (m_Lives == 0) {
        m_State = State::GameOver;
        EM_INFO("Game over: score {}, wave {}", m_Score, m_Wave);
    } else {
        m_RespawnTimer = kRespawnDelay;
    }
}

void Game::PlaySound(SoundEvent event, const Vec2& position)
{
    // Pan by the horizontal position, but never fully to one side.
    const f32 pan = (position.x / kPlayfieldSize.x * 2.0f - 1.0f) * 0.6f;
    m_Sounds.push_back({event, pan});
}

void Game::UpdateHeartbeat(f32 dt)
{
    if (m_State != State::Playing || m_Asteroids.empty())
        return; // quiet between waves and on the game over screen (also in the step the
                // last ship died)
    m_BeatTimer -= dt;
    if (m_BeatTimer > 0.0f)
        return;
    PlaySound(m_BeatHigh ? SoundEvent::BeatHigh : SoundEvent::BeatLow, kPlayfieldCenter);
    m_BeatHigh = !m_BeatHigh;
    m_BeatTimer += GetBeatInterval();
}

f32 Game::GetBeatInterval() const
{
    // From kSlowestBeat with the whole wave left down to kFastestBeat with nothing left.
    u32 mass = 0;
    for (const Asteroid& asteroid : m_Asteroids)
        mass += MassOf(asteroid.Size);
    const f32 left = Emerald::Min(static_cast<f32>(mass) / static_cast<f32>(m_WaveMass), 1.0f);
    return kFastestBeat + (kSlowestBeat - kFastestBeat) * left;
}

void Game::SpawnExplosion(const Vec2& position, u32 dots, f32 speed)
{
    for (u32 i = 0; i < dots; ++i) {
        Particle p;
        p.Position = position;
        p.Velocity = m_Random.Direction() * m_Random.Float(0.2f * speed, speed);
        p.Lifetime = p.TimeLeft = m_Random.Float(0.4f, 0.9f);
        m_Particles.push_back(p);
    }
}

bool Game::IsShipVisible() const
{
    if (!m_ShipAlive || m_State != State::Playing)
        return false;
    // Blink at 8 Hz while invulnerable.
    return m_InvulnerableTimer <= 0.0f || std::fmod(m_Time * 8.0f, 2.0f) < 1.0f;
}

void Game::Draw(Emerald::Renderer2D& r) const
{
    for (const Asteroid& asteroid : m_Asteroids) {
        ForEachWrappedCopy(asteroid.Position, asteroid.Radius * 1.2f,
                           [&](const Vec2& p) { asteroid.Draw(r, p, kAsteroidColor); });
    }

    for (const Bullet& bullet : m_Bullets)
        r.DrawCircle(bullet.Position, 1.5f, kBulletColor, 4);

    for (const Particle& p : m_Particles) {
        const f32 fade = p.TimeLeft / p.Lifetime; // 1 -> 0
        const Vec4 color{1.0f, 1.0f, 1.0f, fade};
        if (p.Length > 0.0f) {
            const Vec2 half = Vec2(std::cos(p.Angle), std::sin(p.Angle)) * (0.5f * p.Length);
            r.DrawLine(p.Position - half, p.Position + half, color);
        } else {
            r.DrawCircle(p.Position, 1.0f, color, 4);
        }
    }

    if (IsShipVisible()) {
        ForEachWrappedCopy(m_Ship.Position, 20.0f,
                           [&](const Vec2& p) { m_Ship.Draw(r, p, kShipColor, m_FlameLength); });
    }

    DrawHud(r);
}

void Game::DrawHud(Emerald::Renderer2D& r) const
{
    // Score in the top-left corner (at least two digits, like the arcade's "00").
    const std::string score =
        m_Score < 10 ? "0" + std::to_string(m_Score) : std::to_string(m_Score);
    VectorFont::DrawText(r, score, {40.0f, 24.0f}, 30.0f, kTextColor);

    // Remaining ships below it, pointing up.
    for (u32 i = 0; i < m_Lives; ++i) {
        const Emerald::Transform2D icon{.Position = {52.0f + 26.0f * static_cast<f32>(i), 84.0f},
                                        .Rotation = -Emerald::HalfPi,
                                        .Scale = Vec2(0.85f)};
        DrawShipShape(r, icon, kTextColor);
    }

    const f32 centerX = kPlayfieldCenter.x;
    if (m_State == State::GameOver) {
        VectorFont::DrawTextCentered(r, "GAME OVER", centerX, 280.0f, 54.0f, kTextColor);
        // The start prompt ("PRESS ENTER") blinks slowly.
        if (std::fmod(m_Time, 1.2f) < 0.8f)
            VectorFont::DrawTextCentered(r, m_StartPrompt, centerX, 380.0f, 24.0f, kTextColor);
    } else if (m_WaveBannerTimer > 0.0f) {
        VectorFont::DrawTextCentered(r, "WAVE " + std::to_string(m_Wave), centerX, 200.0f, 30.0f,
                                     kTextColor);
    }
}

} // namespace Asteroids
