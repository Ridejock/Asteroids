#include "Game.h"

#include <cmath>
#include <string>
#include <string_view>

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
// Input is ignored this long after the game ends / initials are entered, so hammering fire or
// Enter at that moment doesn't type a letter or start a new game by accident.
constexpr f32 kInitialsInputDelay = 0.5f;
constexpr f32 kRestartDelay = 1.0f;
// Letters for the initials, in the order Up steps through them.
constexpr std::string_view kInitialsLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

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
const Vec4 kSaucerColor{0.95f, 0.97f, 1.0f, 1.0f};
const Vec4 kDimTextColor{0.95f, 0.97f, 1.0f, 0.35f};

// Right-aligns `text` in `width` characters (the font is monospaced, so columns line up).
std::string PadLeft(std::string text, usize width)
{
    return text.size() < width ? std::string(width - text.size(), ' ') + text : text;
}

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
    m_Saucer.reset();
    m_SaucerBullets.clear();
    m_NewRank = HighScoreTable::kMaxEntries;

    m_Ship.Reset(kPlayfieldCenter);
    m_ShipAlive = true;
    m_InvulnerableTimer = kInvulnerableTime;
    m_RespawnTimer = 0.0f;
    StartWave();
    m_SaucerTimer = NextSaucerDelay();
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

    if (m_State != State::Playing)
        m_StateTime += dt;
    if (m_State == State::EnterInitials && m_StateTime >= kInitialsInputDelay)
        UpdateInitials(input);
    else if (m_State == State::GameOver && m_StateTime >= kRestartDelay && input.StartPressed)
        NewGame();

    if (m_State == State::Playing)
        UpdateShip(input, dt);
    UpdateObjects(dt); // asteroids keep drifting on the game over screen
    if (m_State == State::Playing) {
        UpdateSaucer(dt);
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
    for (std::vector<Bullet>* bullets : {&m_Bullets, &m_SaucerBullets}) {
        for (usize i = 0; i < bullets->size();) {
            Bullet& b = (*bullets)[i];
            b.Position = Wrap(b.Position + b.Velocity * dt);
            b.TimeLeft -= dt;
            if (b.TimeLeft <= 0.0f) {
                b = bullets->back();
                bullets->pop_back();
            } else {
                ++i;
            }
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

    // Removes the first bullet within `radius` of `position`; true if there was one.
    auto hitByBullet = [](std::vector<Bullet>& bullets, const Vec2& position, f32 radius) {
        for (usize b = 0; b < bullets.size(); ++b) {
            if (CirclesOverlap(bullets[b].Position, 0.0f, position, radius)) {
                bullets[b] = bullets.back();
                bullets.pop_back();
                return true;
            }
        }
        return false;
    };

    for (usize i = 0; i < m_Asteroids.size();) {
        const Asteroid& asteroid = m_Asteroids[i];
        bool destroyed = false;

        // Bullets are points, the asteroid a circle. Only the player's shots score.
        if (hitByBullet(m_Bullets, asteroid.Position, asteroid.Radius)) {
            AddScore(ScoreFor(asteroid.Size));
            destroyed = true;
        } else if (hitByBullet(m_SaucerBullets, asteroid.Position, asteroid.Radius)) {
            destroyed = true;
        }
        // Ship vs asteroid: both destroyed (a slightly smaller asteroid circle feels fairer).
        if (!destroyed && shipCanCrash && m_ShipAlive &&
            CirclesOverlap(m_Ship.Position, Ship::kRadius, asteroid.Position,
                           asteroid.Radius * 0.85f)) {
            AddScore(ScoreFor(asteroid.Size));
            DestroyShip();
            destroyed = true;
        }
        // Saucer vs asteroid: both destroyed, no points.
        if (!destroyed && m_Saucer &&
            CirclesOverlap(m_Saucer->Position, m_Saucer->Radius(), asteroid.Position,
                           asteroid.Radius * 0.85f)) {
            DestroySaucer();
            destroyed = true;
        }

        if (!destroyed) {
            ++i;
            continue;
        }
        BreakAsteroid(asteroid, fragments);
        m_Asteroids[i] = m_Asteroids.back();
        m_Asteroids.pop_back();
    }
    m_Asteroids.insert(m_Asteroids.end(), fragments.begin(), fragments.end());

    // The player shoots the saucer, or rams it (and dies too, but still gets the points).
    if (m_Saucer && hitByBullet(m_Bullets, m_Saucer->Position, m_Saucer->Radius())) {
        AddScore(m_Saucer->Score());
        DestroySaucer();
    }
    if (m_Saucer && shipCanCrash && m_ShipAlive &&
        CirclesOverlap(m_Ship.Position, Ship::kRadius, m_Saucer->Position, m_Saucer->Radius())) {
        AddScore(m_Saucer->Score());
        DestroySaucer();
        DestroyShip();
    }
    // The saucer shoots the ship.
    if (shipCanCrash && m_ShipAlive && hitByBullet(m_SaucerBullets, m_Ship.Position, Ship::kRadius))
        DestroyShip();
}

void Game::BreakAsteroid(const Asteroid& asteroid, std::vector<Asteroid>& fragments)
{
    // Large -> 2 medium -> 2 small -> gone.
    if (const auto smaller = SmallerSize(asteroid.Size)) {
        for (i32 k = 0; k < 2; ++k) {
            Asteroid fragment = MakeAsteroid(m_Random, *smaller, asteroid.Position);
            fragment.Velocity += asteroid.Velocity * 0.5f; // inherit some of the momentum
            fragments.push_back(fragment);
        }
    }
    SpawnExplosion(asteroid.Position, 6 + 4 * (2 - static_cast<u32>(asteroid.Size)), 90.0f);
    const SoundEvent boom = asteroid.Size == AsteroidSize::Large    ? SoundEvent::ExplosionLarge
                            : asteroid.Size == AsteroidSize::Medium ? SoundEvent::ExplosionMedium
                                                                    : SoundEvent::ExplosionSmall;
    PlaySound(boom, asteroid.Position);
}

f32 Game::NextSaucerDelay()
{
    // 10-16 seconds, a second less every wave down to 5-11.
    const f32 waves = static_cast<f32>(Emerald::Min(m_Wave, 6u));
    return m_Random.Float(10.0f, 16.0f) - Emerald::Max(waves - 1.0f, 0.0f);
}

void Game::SpawnSaucer(SaucerSize size)
{
    if (m_State != State::Playing)
        return;
    Saucer saucer;
    saucer.Size = size;
    // Enter just outside the left or right edge, flying across.
    const bool fromLeft = m_Random.Chance(0.5f);
    saucer.Position = {fromLeft ? -saucer.Radius() : kPlayfieldSize.x + saucer.Radius(),
                       m_Random.Float(0.15f, 0.85f) * kPlayfieldSize.y};
    saucer.Velocity = {fromLeft ? saucer.Speed() : -saucer.Speed(), 0.0f};
    saucer.FireTimer = 0.6f; // first shot soon after it appears
    saucer.TurnTimer = m_Random.Float(0.8f, 2.0f);
    m_Saucer = saucer;
    EM_INFO("{} saucer", size == SaucerSize::Large ? "Large" : "Small");
}

void Game::UpdateSaucer(f32 dt)
{
    if (!m_Saucer) {
        // Count down only while there is a fight going on (not between waves or respawns).
        if (m_ShipAlive && !m_Asteroids.empty()) {
            m_SaucerTimer -= dt;
            if (m_SaucerTimer <= 0.0f)
                SpawnSaucer(m_Random.Chance(SmallSaucerChance(m_Score)) ? SaucerSize::Small
                                                                        : SaucerSize::Large);
        }
        return;
    }

    Saucer& saucer = *m_Saucer;
    saucer.Position += saucer.Velocity * dt;
    // Wrap top <-> bottom only.
    saucer.Position.y -= std::floor(saucer.Position.y / kPlayfieldSize.y) * kPlayfieldSize.y;

    // Every now and then: fly straight, or diagonally up or down.
    saucer.TurnTimer -= dt;
    if (saucer.TurnTimer <= 0.0f) {
        saucer.TurnTimer = m_Random.Float(0.8f, 2.0f);
        saucer.Velocity.y = static_cast<f32>(m_Random.Int(-1, 1)) * saucer.Speed() * 0.6f;
    }

    // Gone once it is past the far edge.
    const bool gone = saucer.Velocity.x > 0.0f
                          ? saucer.Position.x > kPlayfieldSize.x + saucer.Radius()
                          : saucer.Position.x < -saucer.Radius();
    if (gone) {
        m_Saucer.reset();
        m_SaucerTimer = NextSaucerDelay();
        return;
    }

    saucer.FireTimer -= dt;
    if (saucer.FireTimer <= 0.0f) {
        saucer.FireTimer += saucer.FireInterval();
        SaucerFire();
    }
}

void Game::SaucerFire()
{
    const Saucer& saucer = *m_Saucer;
    if (m_SaucerBullets.size() >= Saucer::kMaxBullets)
        return;
    // The small saucer aims (better as the score grows); the large one shoots anywhere.
    Vec2 direction;
    if (saucer.Size == SaucerSize::Small && m_ShipAlive) {
        const f32 error = SmallSaucerAimError(m_Score);
        direction = AimDirection(saucer.Position, m_Ship.Position, m_Random.Float(-error, error));
    } else {
        direction = m_Random.Direction();
    }
    Bullet bullet;
    bullet.Position = saucer.Position + direction * saucer.Radius(); // from its rim
    bullet.Velocity = direction * Saucer::kBulletSpeed;
    bullet.TimeLeft = Saucer::kBulletLifetime;
    m_SaucerBullets.push_back(bullet);
    PlaySound(SoundEvent::SaucerFire, saucer.Position);
}

void Game::DestroySaucer()
{
    SpawnDebris(m_Saucer->Position, m_Saucer->Velocity, 4);
    SpawnExplosion(m_Saucer->Position, 10, 110.0f);
    PlaySound(SoundEvent::SaucerExplosion, m_Saucer->Position);
    m_Saucer.reset();
    m_SaucerTimer = NextSaucerDelay();
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
    SpawnDebris(m_Ship.Position, m_Ship.Velocity, 5);
    SpawnExplosion(m_Ship.Position, 12, 120.0f);
    PlaySound(SoundEvent::ShipExplosion, m_Ship.Position);

    --m_Lives;
    ++m_ShipsLost;
    EM_INFO("Ship destroyed, {} left (score {})", m_Lives, m_Score);
    if (m_Lives == 0)
        EndGame();
    else
        m_RespawnTimer = kRespawnDelay;
}

void Game::EndGame()
{
    EM_INFO("Game over: score {}, wave {}", m_Score, m_Wave);
    m_Saucer.reset(); // (Main fades out its sound)
    m_SaucerBullets.clear();
    m_StateTime = 0.0f;
    m_NewRank = HighScoreTable::kMaxEntries;
    if (m_HighScores.Qualifies(m_Score)) {
        // Like the arcade: the first letter starts at A, the others are still blank.
        m_State = State::EnterInitials;
        m_Initials = "A  ";
        m_InitialsCursor = 0;
    } else {
        m_State = State::GameOver;
    }
}

void Game::ForceGameOver(u32 score)
{
    if (m_State != State::Playing)
        return;
    m_Score = score;
    m_Lives = 0;
    m_ShipAlive = false;
    m_Ship.Thrusting = false;
    EndGame();
}

void Game::UpdateInitials(const GameInput& input)
{
    // Up/Down step through A..Z and space (wrapping around).
    char& letter = m_Initials[m_InitialsCursor];
    if (input.MenuUpPressed != input.MenuDownPressed) {
        const usize count = kInitialsLetters.size();
        usize index = kInitialsLetters.find(letter);
        if (index == std::string_view::npos)
            index = 0;
        index = input.MenuUpPressed ? (index + 1) % count : (index + count - 1) % count;
        letter = kInitialsLetters[index];
    }

    if (input.ConfirmPressed) {
        // Next letter (starting at A), or done after the third.
        if (++m_InitialsCursor < HighScoreTable::kInitialsLength) {
            m_Initials[m_InitialsCursor] = 'A';
        } else {
            m_NewRank = m_HighScores.Insert(m_Initials, m_Score);
            m_HighScoresChanged = true;
            m_State = State::GameOver;
            m_StateTime = 0.0f;
            EM_INFO("New high score #{}: '{}' {}", m_NewRank + 1, m_Initials, m_Score);
        }
    } else if (input.BackPressed && m_InitialsCursor > 0) {
        m_Initials[m_InitialsCursor] = ' '; // blank again, like before we got here
        --m_InitialsCursor;
    }
}

void Game::SpawnDebris(const Vec2& position, const Vec2& velocity, u32 lines)
{
    // Spinning line pieces that drift apart, a bit of the object's momentum included.
    for (u32 i = 0; i < lines; ++i) {
        Particle p;
        p.Position = position + m_Random.Direction() * m_Random.Float(0.0f, 8.0f);
        p.Velocity = velocity * 0.3f + m_Random.Direction() * m_Random.Float(20.0f, 70.0f);
        p.Angle = m_Random.Float(0.0f, Emerald::TwoPi);
        p.Spin = m_Random.Float(-3.0f, 3.0f);
        p.Length = m_Random.Float(8.0f, 16.0f);
        p.Lifetime = p.TimeLeft = m_Random.Float(1.2f, 2.0f);
        m_Particles.push_back(p);
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
    // Once the game is over the rocks keep drifting, dimmed so the text stays readable.
    Vec4 asteroidColor = kAsteroidColor;
    if (m_State != State::Playing)
        asteroidColor.w = 0.35f;
    for (const Asteroid& asteroid : m_Asteroids) {
        ForEachWrappedCopy(asteroid.Position, asteroid.Radius * 1.2f,
                           [&](const Vec2& p) { asteroid.Draw(r, p, asteroidColor); });
    }

    for (const Bullet& bullet : m_Bullets)
        r.DrawCircle(bullet.Position, 1.5f, kBulletColor, 4);
    for (const Bullet& bullet : m_SaucerBullets)
        r.DrawCircle(bullet.Position, 1.5f, kBulletColor, 4);
    if (m_Saucer)
        m_Saucer->Draw(r, kSaucerColor);

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
    // The best score so far, small, at the top in the middle.
    if (m_HighScores.GetBest() > 0)
        VectorFont::DrawTextCentered(r, std::to_string(m_HighScores.GetBest()), centerX, 30.0f,
                                     18.0f, kTextColor);

    if (m_State == State::EnterInitials) {
        DrawInitialsEntry(r);
    } else if (m_State == State::GameOver) {
        const bool hasTable = !m_HighScores.GetEntries().empty();
        VectorFont::DrawTextCentered(r, "GAME OVER", centerX, hasTable ? 110.0f : 280.0f,
                                     hasTable ? 48.0f : 54.0f, kTextColor);
        if (hasTable)
            DrawHighScoreTable(r, 200.0f);
        // The start prompt ("PRESS ENTER") blinks slowly, once a new game can be started.
        if (m_StateTime >= kRestartDelay && std::fmod(m_Time, 1.2f) < 0.8f)
            VectorFont::DrawTextCentered(r, m_Prompts.Start, centerX, hasTable ? 630.0f : 380.0f,
                                         24.0f, kTextColor);
    } else if (m_WaveBannerTimer > 0.0f) {
        VectorFont::DrawTextCentered(r, "WAVE " + std::to_string(m_Wave), centerX, 200.0f, 30.0f,
                                     kTextColor);
    }
}

void Game::DrawInitialsEntry(Emerald::Renderer2D& r) const
{
    const f32 centerX = kPlayfieldCenter.x;
    VectorFont::DrawTextCentered(r, "YOUR SCORE IS ONE OF THE TEN BEST", centerX, 140.0f, 24.0f,
                                 kTextColor);
    VectorFont::DrawTextCentered(r, "PLEASE ENTER YOUR INITIALS", centerX, 185.0f, 24.0f,
                                 kTextColor);

    // Three big letters over underlines; the one being changed blinks its underline.
    constexpr f32 kLetterHeight = 60.0f;
    constexpr f32 kSlotSpacing = 80.0f;
    const f32 letterWidth = VectorFont::TextWidth("A", kLetterHeight);
    for (usize i = 0; i < HighScoreTable::kInitialsLength; ++i) {
        const f32 x = centerX + (static_cast<f32>(i) - 1.0f) * kSlotSpacing - 0.5f * letterWidth;
        if (i <= m_InitialsCursor)
            VectorFont::DrawText(r, std::string(1, m_Initials[i]), {x, 290.0f}, kLetterHeight,
                                 kTextColor);
        const bool current = i == m_InitialsCursor;
        if (!current || std::fmod(m_Time * 3.0f, 2.0f) < 1.4f)
            r.DrawLine({x, 368.0f}, {x + letterWidth, 368.0f},
                       current ? kTextColor : kDimTextColor);
    }

    VectorFont::DrawTextCentered(r, "UP / DOWN: CHANGE LETTER", centerX, 450.0f, 20.0f, kTextColor);
    VectorFont::DrawTextCentered(r, m_Prompts.Confirm + ": NEXT", centerX, 490.0f, 20.0f,
                                 kTextColor);
    VectorFont::DrawTextCentered(r, m_Prompts.Back + ": BACK", centerX, 530.0f, 20.0f, kTextColor);
}

void Game::DrawHighScoreTable(Emerald::Renderer2D& r, f32 top) const
{
    const f32 centerX = kPlayfieldCenter.x;
    VectorFont::DrawTextCentered(r, "HIGH SCORES", centerX, top, 24.0f, kTextColor);

    // " 1.  ABC   12340": every row has the same length, so the columns line up when centered.
    const std::vector<HighScore>& entries = m_HighScores.GetEntries();
    for (usize i = 0; i < entries.size(); ++i) {
        const std::string row = PadLeft(std::to_string(i + 1) + ".", 3) + "  " +
                                entries[i].Initials + "  " +
                                PadLeft(std::to_string(entries[i].Score), 6);
        // The entry just made blinks.
        if (i == m_NewRank && std::fmod(m_Time * 3.0f, 2.0f) >= 1.4f)
            continue;
        VectorFont::DrawTextCentered(r, row, centerX, top + 50.0f + 32.0f * static_cast<f32>(i),
                                     20.0f, kTextColor);
    }
}

} // namespace Asteroids
