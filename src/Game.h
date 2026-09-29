#pragma once

#include <vector>

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Vec2.h>
#include <Emerald/Renderer/Renderer2D.h>

#include "Asteroid.h"
#include "Bullet.h"
#include "Random.h"
#include "Ship.h"

namespace Asteroids {

// Player input for one fixed step. The "pressed" flags are true only in the step the key went
// down (Emerald's Input takes care of that), so holding Space does not auto-fire.
struct GameInput {
    ShipControls Ship;
    bool FirePressed = false;
    bool HyperspacePressed = false;
    bool StartPressed = false;
};

// All of the game's state and rules. It knows nothing about windows or GPUs: Main.cpp feeds it
// input at a fixed rate (Update) and gives it a Renderer2D already set up for playfield
// coordinates (Draw).
class Game {
public:
    explicit Game(u32 seed);

    void Update(const GameInput& input, f32 dt);
    void Draw(Emerald::Renderer2D& r) const;

    [[nodiscard]] u32 GetScore() const { return m_Score; }
    [[nodiscard]] u32 GetLives() const { return m_Lives; }
    [[nodiscard]] u32 GetWave() const { return m_Wave; }
    [[nodiscard]] usize GetAsteroidCount() const { return m_Asteroids.size(); }
    [[nodiscard]] bool IsGameOver() const { return m_State == State::GameOver; }

private:
    enum class State { Playing, GameOver };

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

    void NewGame();
    void StartWave();
    void UpdateShip(const GameInput& input, f32 dt);
    void FireBullet();
    void Hyperspace();
    void UpdateObjects(f32 dt);
    void HandleCollisions();
    void AddScore(u32 points);
    void DestroyShip();
    void SpawnExplosion(const Vec2& position, u32 dots, f32 speed);
    [[nodiscard]] bool IsShipVisible() const;
    void DrawHud(Emerald::Renderer2D& r) const;

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
    std::vector<Bullet> m_Bullets;
    std::vector<Particle> m_Particles;

    u32 m_Score = 0;
    u32 m_Lives = 0;
    u32 m_Wave = 0;
    u32 m_NextExtraLife = 0;      // score at which the next extra ship is awarded
    f32 m_NextWaveTimer = 0.0f;   // > 0 while waiting to start the next wave
    f32 m_WaveBannerTimer = 0.0f; // > 0 while "WAVE n" is shown
};

} // namespace Asteroids
