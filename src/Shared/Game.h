#pragma once

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Vec2.h>
#include <Emerald/Renderer/Renderer2D.h>

#include "Asteroid.h"
#include "Bullet.h"
#include "GameMode.h"
#include "HighScores.h"
#include "Random.h"
#include "RockBounce.h"
#include "Saucer.h"
#include "ScoreScreens.h"
#include "Ship.h"

namespace Asteroids {

// A short-lived bit of an explosion: a dot, or a spinning line when Length > 0.
struct Particle {
    Vec2 Position;
    Vec2 Velocity;
    f32 Angle = 0.0f;
    f32 Spin = 0.0f;
    f32 Length = 0.0f;
    f32 TimeLeft = 0.0f;
    f32 Lifetime = 1.0f;
};

// All of the game's state and rules. It knows nothing about windows or GPUs: AsteroidsApp feeds
// it input at a fixed rate (Update). How the world looks is up to the executable (vector lines or
// pixel sprites), which reads the state through the getters below; the HUD (vector font) is
// shared and drawn by DrawHud.
class Game final : public GameMode {
public:
    // Starts on the title screen (see ShowTitle).
    explicit Game(u32 seed);

    // The title screen's background: a few rocks drifting, no ship, no score. The app draws
    // the logo and menus on top.
    void ShowTitle() override;
    // A new game: 3 ships, score 0, wave 1.
    void StartGame() override;
    [[nodiscard]] bool IsOnTitle() const override { return m_State == State::Attract; }

    void Update(const GameInput& input, f32 dt) override;
    // Score, lives, high score, banners, initials entry and the high score table, in playfield
    // coordinates. `drawLives` = false leaves the remaining-ships icons to the caller.
    void DrawHud(Emerald::Renderer2D& r, bool drawLives) const;
    void DrawHud(Emerald::Renderer2D& r) const override { DrawHud(r, true); }
    // Where the HUD shows the i-th remaining ship (center, playfield coordinates).
    [[nodiscard]] static Vec2 GetLifeIconPosition(u32 index)
    {
        return {52.0f + 26.0f * static_cast<f32>(index), 84.0f};
    }

    // --- What there is to draw ---
    [[nodiscard]] const Ship& GetShip() const override { return m_Ship; }
    // False while the ship is exploded/respawning, and during the invulnerability blink's off
    // phases.
    [[nodiscard]] bool IsShipVisible() const;
    // Flickering flame length while thrusting (pixels, 0 = no flame this step).
    [[nodiscard]] f32 GetFlameLength() const { return m_FlameLength; }
    [[nodiscard]] std::span<const Asteroid> GetAsteroids() const { return m_Asteroids; }
    [[nodiscard]] std::span<const Bullet> GetBullets() const { return m_Bullets; }
    [[nodiscard]] std::span<const Bullet> GetSaucerBullets() const { return m_SaucerBullets; }
    [[nodiscard]] const std::optional<Saucer>& GetSaucer() const { return m_Saucer; }
    [[nodiscard]] std::span<const Particle> GetParticles() const { return m_Particles; }
    // Seconds since the game object was created (for blinking and other animation).
    [[nodiscard]] f32 GetTime() const { return m_Time; }

    [[nodiscard]] u32 GetScore() const override { return m_Score; }
    [[nodiscard]] u32 GetLives() const { return m_Lives; }
    [[nodiscard]] u32 GetWave() const override { return m_Wave; }
    [[nodiscard]] usize GetAsteroidCount() const override { return m_Asteroids.size(); }
    // True unless a game is being played: while entering initials, on the game over screen and
    // on the title screen.
    [[nodiscard]] bool IsGameOver() const override { return m_State != State::Playing; }
    // True while the player controls a ship (the app pauses then when the window loses focus).
    [[nodiscard]] bool IsPlaying() const { return m_State == State::Playing; }
    // On the game over screen with the table shown (Start restarts from here).
    [[nodiscard]] bool IsOnGameOverScreen() const override { return m_State == State::GameOver; }
    [[nodiscard]] bool IsEnteringInitials() const { return m_State == State::EnterInitials; }
    // True while the ship is alive and its engine fires (for the looping thrust sound).
    [[nodiscard]] bool IsThrusting() const override
    {
        return m_State == State::Playing && m_ShipAlive && m_Ship.Thrusting;
    }
    // Sounds requested since the last ClearSounds (call both after every Update).
    [[nodiscard]] const std::vector<GameSound>& GetSounds() const override { return m_Sounds; }
    void ClearSounds() override { m_Sounds.clear(); }
    // Counts every destroyed ship, so the app can react (e.g. rumble) when it changes.
    [[nodiscard]] u32 GetShipsLost() const override { return m_ShipsLost; }
    void SetPrompts(Prompts prompts) override { m_Prompts = std::move(prompts); }
    void SetRockBounce(bool on) override { m_RockBounce = on; }

    // The saucer currently on screen, if any (Main plays its looping sound).
    [[nodiscard]] std::optional<SaucerSize> GetSaucerSize() const override
    {
        return m_Saucer ? std::optional(m_Saucer->Size) : std::nullopt;
    }

    // High scores: Main loads them at startup and saves them whenever they change.
    void SetHighScores(HighScoreTable table) override { m_HighScores = std::move(table); }
    [[nodiscard]] const HighScoreTable& GetHighScores() const override { return m_HighScores; }
    // True once after a new entry was added (then it's false until the next one).
    [[nodiscard]] bool ConsumeHighScoresChanged() override
    {
        return std::exchange(m_HighScoresChanged, false);
    }

    // Debug helpers (command line / debug panel): a saucer right now, or game over with `score`.
    void SpawnSaucer(SaucerSize size) override;
    void ForceGameOver(u32 score) override;

    // The top 10 at `top` (playfield y), for the game over and title screens.
    void DrawHighScoreTable(Emerald::Renderer2D& r, f32 top) const override;

private:
    enum class State { Attract, Playing, EnterInitials, GameOver };

    void NewGame();
    void StartWave();
    void UpdateShip(const GameInput& input, f32 dt);
    void FireBullet();
    void Hyperspace();
    void UpdateObjects(f32 dt);
    void HandleCollisions();
    void AddScore(u32 points);
    void DestroyShip();
    void EndGame();
    void UpdateInitials(const GameInput& input);
    void BreakAsteroid(const Asteroid& asteroid, std::vector<Asteroid>& fragments);
    void UpdateSaucer(f32 dt);
    void SaucerFire();
    void DestroySaucer();
    void SpawnDebris(const Vec2& position, const Vec2& velocity, u32 lines);
    [[nodiscard]] f32 NextSaucerDelay();
    void SpawnExplosion(const Vec2& position, u32 dots, f32 speed);

    void PlaySound(SoundEvent event, const Vec2& position, const Vec2& velocity = {});
    void UpdateHeartbeat(f32 dt);
    [[nodiscard]] f32 GetBeatInterval() const;

    Random m_Random;
    State m_State = State::Playing;
    f32 m_Time = 0.0f; // seconds since start, for blinking

    Ship m_Ship;
    bool m_ShipAlive = true;
    f32 m_RespawnTimer = 0.0f;      // > 0 while waiting to respawn after a crash
    f32 m_InvulnerableTimer = 0.0f; // > 0 right after (re)spawning: blinks, cannot crash
    f32 m_HyperspaceCooldown = 0.0f;
    f32 m_FlameLength = 0.0f; // flickers randomly while thrusting

    std::vector<Asteroid> m_Asteroids;
    bool m_RockBounce = false; // the ROCK BOUNCE option
    RockBounce m_Bounce;
    std::vector<Asteroid*> m_BounceRocks; // scratch: the rocks for m_Bounce
    u32 m_RockFamily = 0;                 // last family given to fragments
    std::vector<Bullet> m_Bullets;
    std::vector<Particle> m_Particles;

    std::optional<Saucer> m_Saucer;
    std::vector<Bullet> m_SaucerBullets;
    f32 m_SaucerTimer = 0.0f; // seconds until the next saucer appears

    HighScoreTable m_HighScores;
    bool m_HighScoresChanged = false;
    InitialsEntry m_InitialsEntry;
    usize m_NewRank = HighScoreTable::kMaxEntries; // row to highlight in the table (none)
    f32 m_StateTime = 0.0f;                        // seconds since initials entry / game over began

    u32 m_Score = 0;
    u32 m_Lives = 0;
    u32 m_Wave = 0;
    u32 m_ShipsLost = 0;
    std::vector<GameSound> m_Sounds;

    // Heartbeat: two tones alternating, faster as the wave's rocks get destroyed.
    f32 m_BeatTimer = 0.0f; // seconds until the next beat
    bool m_BeatHigh = true; // which tone comes next
    u32 m_WaveMass = 1;     // "rock mass" at the start of the wave (see GetBeatInterval)
    Prompts m_Prompts;
    u32 m_NextExtraLife = 0;      // score at which the next extra ship is awarded
    f32 m_NextWaveTimer = 0.0f;   // > 0 while waiting to start the next wave
    f32 m_WaveBannerTimer = 0.0f; // > 0 while "WAVE n" is shown
};

} // namespace Asteroids
