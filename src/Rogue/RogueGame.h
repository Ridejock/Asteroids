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
#include "Meta.h"
#include "Random.h"
#include "Run.h"
#include "Saucer.h"
#include "ScoreScreens.h"
#include "Ship.h"
#include "Upgrades.h"

namespace Asteroids::Rogue {

// A rock with a kind (see RockKind) and hit points.
struct Rock {
    Asteroid Body;
    RockKind Kind = RockKind::Normal;
    i32 Health = 1;
    u32 Id = 0;       // unique per run, so a piercing shot hits each rock only once
    f32 Flash = 0.0f; // > 0 right after a hit that didn't break it (drawn brighter)
};

// A shot from the ship: bullets, or homing missiles.
struct Shot {
    Vec2 Position;
    Vec2 Velocity;
    f32 TimeLeft = Bullet::kLifetime;
    u32 PierceLeft = 0; // rocks it may still pass through
    u32 LastHit = 0;    // id of the rock it just passed through (0 = none)
    bool Missile = false;
};

// Enemy fire. Boss patterns don't wrap (they would fill the screen); saucer shots do.
struct EnemyShot {
    Vec2 Position;
    Vec2 Velocity;
    f32 TimeLeft = Saucer::kBulletLifetime;
    bool Wraps = true;
};

// A saucer with hit points. Escorts (launched by the mothership) stay and hunt the ship.
struct Enemy {
    Saucer Body;
    i32 Health = 1;
    f32 Flash = 0.0f;
    bool Escort = false;
};

struct Boss {
    BossKind Kind = BossKind::GiantRock;
    Vec2 Position;
    Vec2 Velocity;
    f32 Angle = 0.0f;
    f32 Spin = 0.0f;
    f32 Radius = 80.0f; // for collisions
    i32 Health = 1;
    i32 MaxHealth = 1;
    f32 Flash = 0.0f;
    f32 Time = 0.0f;       // seconds since it appeared
    f32 FireTimer = 0.0f;  // next aimed volley / pattern bullet
    f32 SpawnTimer = 0.0f; // next escort saucer
    f32 PatternAngle = 0.0f;
    f32 PatternSpin = 1.1f; // radians per second, flips now and then
    u32 Shed = 0;           // armored chunks already shed (giant rock)
    bool Entering = true;   // flying in (can't be hurt yet)
};

// Scrap dropped by destroyed enemies; the ship collects it by flying close.
struct Pickup {
    Vec2 Position;
    Vec2 Velocity;
    f32 TimeLeft = 10.0f;
    u32 Value = 1;
};

// The roguelike: a run is 3 sectors of 4 waves and a boss, with one life (and shields). After
// every wave the player picks one of three random upgrades; scrap earned in a run buys ships and
// more upgrades in the hangar (MetaProgress, kept between runs). All randomness comes from the
// seeded Random, so a seed plus the same input replays the same run.
class RogueGame final : public GameMode {
public:
    explicit RogueGame(u32 seed);

    // --- GameMode ---
    void ShowTitle() override;
    [[nodiscard]] bool IsOnTitle() const override { return m_State == State::Attract; }
    void StartGame() override;
    void Update(const GameInput& input, f32 dt) override;
    [[nodiscard]] bool IsGameOver() const override
    {
        return m_State != State::Playing && m_State != State::Upgrade;
    }
    [[nodiscard]] bool IsOnGameOverScreen() const override { return m_State == State::Results; }
    [[nodiscard]] bool IsInMenuScreen() const override
    {
        return m_State == State::Hangar || m_State == State::Upgrade;
    }
    [[nodiscard]] std::vector<std::string> GetTitleExtras() const override { return {"HANGAR"}; }
    void OpenTitleExtra(usize index) override;
    void DrawHud(Emerald::Renderer2D& r) const override;
    void DrawHighScoreTable(Emerald::Renderer2D& r, f32 top) const override;
    [[nodiscard]] const Ship& GetShip() const override { return m_Ship; }
    [[nodiscard]] bool IsThrusting() const override
    {
        return m_State == State::Playing && m_ShipAlive && m_Ship.Thrusting;
    }
    [[nodiscard]] std::optional<SaucerSize> GetSaucerSize() const override;
    [[nodiscard]] u32 GetShipsLost() const override { return m_ShipsLost; }
    [[nodiscard]] const std::vector<GameSound>& GetSounds() const override { return m_Sounds; }
    void ClearSounds() override { m_Sounds.clear(); }
    void SetPrompts(Prompts prompts) override { m_Prompts = std::move(prompts); }
    void SetHighScores(HighScoreTable table) override { m_HighScores = std::move(table); }
    [[nodiscard]] const HighScoreTable& GetHighScores() const override { return m_HighScores; }
    [[nodiscard]] bool ConsumeHighScoresChanged() override
    {
        return std::exchange(m_HighScoresChanged, false);
    }
    [[nodiscard]] u32 GetScore() const override { return m_Score; }
    [[nodiscard]] u32 GetWave() const override { return m_Stage.Index() + 1; }
    [[nodiscard]] usize GetAsteroidCount() const override { return m_Rocks.size(); }
    void SpawnSaucer(SaucerSize size) override;
    void ForceGameOver(u32 score) override;
    // "hangar", "upgrades" (the picker), "boss1".."boss3", "sector1".."sector3", "results".
    bool OpenDebugScreen(std::string_view name) override;

    // --- Meta progress: the app loads it at startup and saves it when it changed ---
    void SetMeta(MetaProgress meta) { m_Meta = std::move(meta); }
    [[nodiscard]] const MetaProgress& GetMeta() const { return m_Meta; }
    [[nodiscard]] bool ConsumeMetaChanged() { return std::exchange(m_MetaChanged, false); }

    // --- Run state (also for tests) ---
    [[nodiscard]] const Stage& GetStage() const { return m_Stage; }
    [[nodiscard]] const UpgradeSet& GetUpgrades() const { return m_Upgrades; }
    [[nodiscard]] const CombatStats& GetStats() const { return m_Stats; }
    [[nodiscard]] ShipType GetShipType() const { return m_ShipType; }
    [[nodiscard]] u32 GetShield() const { return m_Shield; }
    [[nodiscard]] u32 GetRunScrap() const { return m_RunScrap; }
    [[nodiscard]] bool IsPlaying() const { return m_State == State::Playing; }
    [[nodiscard]] bool IsPickingUpgrade() const { return m_State == State::Upgrade; }
    [[nodiscard]] bool IsInHangar() const { return m_State == State::Hangar; }
    [[nodiscard]] bool IsRunOver() const
    {
        return m_State == State::EnterInitials || m_State == State::Results;
    }
    [[nodiscard]] bool IsVictory() const { return m_Victory; }
    [[nodiscard]] const std::vector<UpgradeId>& GetOffers() const { return m_Offers; }
    [[nodiscard]] usize GetOfferCursor() const { return m_OfferCursor; }
    // Test helpers: take offer `index` now / clear the current stage at once.
    void ChooseOffer(usize index);
    void ClearStageForTest();

    // --- What there is to draw ---
    [[nodiscard]] bool IsShipVisible() const;
    [[nodiscard]] f32 GetFlameLength() const { return m_FlameLength; }
    [[nodiscard]] std::span<const Rock> GetRocks() const { return m_Rocks; }
    [[nodiscard]] std::span<const Shot> GetShots() const { return m_Shots; }
    [[nodiscard]] std::span<const EnemyShot> GetEnemyShots() const { return m_EnemyShots; }
    [[nodiscard]] std::span<const Enemy> GetEnemies() const { return m_Enemies; }
    [[nodiscard]] std::span<const Pickup> GetPickups() const { return m_Pickups; }
    [[nodiscard]] const std::optional<Boss>& GetBoss() const { return m_Boss; }
    [[nodiscard]] f32 GetTime() const { return m_Time; }
    // > 0 while the shield's ring shows (just hit / just recharged).
    [[nodiscard]] f32 GetShieldGlow() const { return m_ShieldGlow; }

    // Layout the app draws sprites into (playfield coordinates).
    [[nodiscard]] static Vec2 GetOfferIconPosition(usize index);
    [[nodiscard]] static Vec2 GetHudIconPosition(usize index); // collected upgrades, bottom left
    [[nodiscard]] static Vec2 GetHangarIconPosition(usize row);
    // Hangar rows: the ships, then the upgrades that can be bought.
    struct HangarRow {
        bool IsShip = true;
        usize Index = 0; // ShipType or UpgradeId
    };
    [[nodiscard]] static const std::vector<HangarRow>& GetHangarRows();
    [[nodiscard]] usize GetHangarCursor() const { return m_HangarCursor; }

private:
    enum class State { Attract, Hangar, Playing, Upgrade, EnterInitials, Results };

    void NewRun(ShipType ship, const Stage& stage);
    void StartStage();
    void SpawnWaveRocks(const WavePlan& plan);
    void ApplyStats();
    void UpdatePlaying(const GameInput& input, f32 dt);
    void UpdateShip(const GameInput& input, f32 dt);
    void FireVolley();
    void LaunchMissile();
    void Hyperspace();
    void UpdateObjects(f32 dt);
    void UpdateShots(f32 dt);
    void UpdateEnemies(f32 dt);
    void EnemyFire(Enemy& enemy);
    void UpdateBoss(f32 dt);
    void UpdatePickups(f32 dt);
    void HandleCollisions();
    // Hurts a rock; true if that destroyed it (the caller removes it and calls BreakRock).
    bool DamageRock(Rock& rock, i32 damage);
    void BreakRock(const Rock& rock, bool byPlayer);
    void DamageBoss(i32 damage);
    void DestroyBoss();
    void DamageEnemy(usize index, i32 damage);
    // Hurts everything within `radius` (the ship too if `hurtsShip`), explosive rocks chaining.
    void Explode(const Vec2& position, f32 radius, i32 damage, bool hurtsShip);
    void HitShip();
    void DropScrap(const Vec2& position, u32 bits, u32 value);
    void CheckStageCleared(f32 dt);
    void OfferUpgrades();
    void UpdateUpgradePick(const GameInput& input);
    void UpdateHangar(const GameInput& input);
    void EndRun(bool victory);
    void UpdateInitials(const GameInput& input);
    void AddScore(u32 points);
    [[nodiscard]] Rock MakeRock(RockKind kind, AsteroidSize size, const Vec2& position);
    [[nodiscard]] Vec2 SafeEdgePosition();
    [[nodiscard]] std::optional<Vec2> NearestTarget(const Vec2& from, f32 range) const;
    void PlaySound(SoundEvent event, const Vec2& position, const Vec2& velocity = {});

    void DrawPlayHud(Emerald::Renderer2D& r) const;
    void DrawUpgradePick(Emerald::Renderer2D& r) const;
    void DrawHangar(Emerald::Renderer2D& r) const;
    void DrawResults(Emerald::Renderer2D& r) const;

    Random m_Random;
    State m_State = State::Attract;
    f32 m_Time = 0.0f;
    f32 m_StateTime = 0.0f; // seconds in the current state

    // The run.
    Stage m_Stage;
    ShipType m_ShipType = ShipType::Striker;
    UpgradeSet m_Upgrades;
    CombatStats m_Stats;
    WavePlan m_Plan;
    u32 m_Score = 0;
    u32 m_RunScrap = 0; // picked up this run
    u32 m_BossesDefeated = 0;
    u32 m_WavesCleared = 0;
    bool m_Victory = false;
    u32 m_ScrapEarned = 0; // total reward of the finished run (results screen)

    Ship m_Ship;
    bool m_ShipAlive = true;
    u32 m_Shield = 0;
    f32 m_ShieldGlow = 0.0f;
    f32 m_InvulnerableTimer = 0.0f;
    f32 m_DeathTimer = 0.0f; // after the crash, until the run ends
    f32 m_FireCooldown = 0.0f;
    f32 m_MissileTimer = 0.0f;
    f32 m_HyperspaceCooldown = 0.0f;
    f32 m_FlameLength = 0.0f;

    std::vector<Rock> m_Rocks;
    std::vector<Rock> m_NewRocks; // fragments, added after the collision pass
    u32 m_NextRockId = 1;
    std::vector<Shot> m_Shots;
    std::vector<EnemyShot> m_EnemyShots;
    std::vector<Enemy> m_Enemies;
    f32 m_SaucerTimer = 0.0f;
    std::optional<Boss> m_Boss;
    std::vector<Pickup> m_Pickups;
    struct PendingBlast {
        Vec2 Position;
        f32 Radius = 0.0f;
        i32 Damage = 0;
        bool HurtsShip = false;
    };
    std::vector<PendingBlast> m_Blasts;

    f32 m_ClearTimer = 0.0f;  // > 0 while the cleared stage winds down
    f32 m_BannerTimer = 0.0f; // > 0 while the stage name is shown
    std::vector<UpgradeId> m_Offers;
    usize m_OfferCursor = 0;
    usize m_HangarCursor = 0;
    std::string m_HangarMessage; // e.g. "NOT ENOUGH SCRAP"
    f32 m_HangarMessageTimer = 0.0f;

    MetaProgress m_Meta;
    bool m_MetaChanged = false;
    HighScoreTable m_HighScores;
    bool m_HighScoresChanged = false;
    InitialsEntry m_InitialsEntry;
    usize m_NewRank = HighScoreTable::kMaxEntries;
    Prompts m_Prompts;
    u32 m_ShipsLost = 0;
    std::vector<GameSound> m_Sounds;
};

} // namespace Asteroids::Rogue
