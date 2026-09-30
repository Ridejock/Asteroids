#include "RogueGame.h"

#include <algorithm>
#include <cmath>

#include <Emerald/Core/Log.h>
#include <Emerald/Math/Common.h>

#include "Playfield.h"
#include "Text.h"

namespace Asteroids::Rogue {

namespace {

constexpr f32 kStageInvulnerable = 2.0f;  // seconds at the start of every stage
constexpr f32 kShieldInvulnerable = 1.5f; // after the shield took a hit
constexpr f32 kDeathDelay = 2.5f;         // explosion, then the run ends
constexpr f32 kHyperspaceCooldown = 1.0f;
constexpr f32 kClearDelay = 1.6f; // stage cleared -> upgrade picker
constexpr f32 kBannerTime = 2.5f;
constexpr f32 kPickDelay = 0.6f;    // the picker ignores input this long (no accidental picks)
constexpr f32 kInputDelay = 0.5f;   // initials / results
constexpr f32 kBackToTitle = 30.0f; // results -> title by itself
constexpr f32 kSafeSpawnDistance = 240.0f;
constexpr f32 kMissileSpeed = 360.0f;
constexpr f32 kMissileTurn = 5.0f;  // radians per second
constexpr f32 kPickupRange = 20.0f; // collected this close to the ship
constexpr usize kMaxEnemyShots = 160;
constexpr u32 kTitleRocks = 7;

// Screen layout.
constexpr f32 kCardWidth = 320.0f;
constexpr f32 kCardHeight = 300.0f;
constexpr f32 kCardTop = 210.0f;
constexpr f32 kCardSpacing = 360.0f;
constexpr f32 kHangarRowHeight = 42.0f;
const Vec4 kAccent{0.45f, 0.9f, 1.0f, 1.0f};  // selection, shield
const Vec4 kWarm{1.0f, 0.72f, 0.3f, 1.0f};    // scrap, costs
const Vec4 kDanger{1.0f, 0.35f, 0.35f, 1.0f}; // boss

// Splits `text` into lines no wider than `width` at `height` (on spaces).
std::vector<std::string> WrapText(std::string_view text, f32 height, f32 width)
{
    std::vector<std::string> lines;
    std::string line;
    while (!text.empty()) {
        const usize space = std::min(text.find(' '), text.size());
        const std::string_view word = text.substr(0, space);
        text.remove_prefix(std::min(space + 1, text.size()));
        const std::string candidate =
            line.empty() ? std::string(word) : line + " " + std::string(word);
        if (!line.empty() && Text::Width(candidate, height) > width) {
            lines.push_back(line);
            line = std::string(word);
        } else {
            line = candidate;
        }
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

// A filled bar from lines (the vector HUD has no filled shapes).
void FillRect(Emerald::Renderer2D& r, const Vec2& topLeft, const Vec2& size, const Vec4& color)
{
    for (f32 y = 0.5f; y < size.y; y += 1.0f)
        r.DrawLine({topLeft.x, topLeft.y + y}, {topLeft.x + size.x, topLeft.y + y}, color);
}

// The x of card `index` of `count` (centered as a group).
f32 CardCenterX(usize index, usize count)
{
    return kPlayfieldCenter.x +
           (static_cast<f32>(index) - 0.5f * static_cast<f32>(count - 1)) * kCardSpacing;
}

f32 HangarRowY(usize row)
{
    // Ships, a gap for the second heading, the upgrades, another gap, BACK.
    const usize ships = kShipCount;
    const f32 gap = row >= ships ? 50.0f : 0.0f;
    const f32 backGap = row + 1 == RogueGame::GetHangarRows().size() ? 16.0f : 0.0f;
    return 196.0f + kHangarRowHeight * static_cast<f32>(row) + gap + backGap;
}

usize SizeIndex(AsteroidSize size)
{
    return static_cast<usize>(size);
}

// Turns `v` towards `target` by at most `maxAngle` radians, keeping its length.
Vec2 TurnTowards(const Vec2& v, const Vec2& target, f32 maxAngle)
{
    const f32 current = std::atan2(v.y, v.x);
    f32 delta = std::atan2(target.y, target.x) - current;
    delta = std::remainder(delta, Emerald::TwoPi); // -Pi .. Pi
    const f32 angle = current + std::clamp(delta, -maxAngle, maxAngle);
    return Vec2(std::cos(angle), std::sin(angle)) * Emerald::Length(v);
}

Vec2 FromAngle(f32 angle)
{
    return {std::cos(angle), std::sin(angle)};
}

} // namespace

RogueGame::RogueGame(u32 seed) : m_Random(seed)
{
    ShowTitle();
}

// ---------------------------------------------------------------------------------------------
// Title, run start, stages
// ---------------------------------------------------------------------------------------------

void RogueGame::ShowTitle()
{
    m_State = State::Attract;
    m_StateTime = 0.0f;
    m_ShipAlive = false;
    m_Ship.Thrusting = false;
    m_FlameLength = 0.0f;
    m_Shots.clear();
    m_EnemyShots.clear();
    m_Enemies.clear();
    m_Pickups.clear();
    m_Boss.reset();
    m_BannerTimer = 0.0f;
    m_NewRank = HighScoreTable::kMaxEntries;
    // A mix of sizes and kinds drifting behind the logo.
    m_Rocks.clear();
    for (u32 i = 0; i < kTitleRocks; ++i) {
        const AsteroidSize size = i < 3   ? AsteroidSize::Large
                                  : i < 5 ? AsteroidSize::Medium
                                          : AsteroidSize::Small;
        const RockKind kind = static_cast<RockKind>(i % static_cast<u32>(RockKind::Count));
        const Vec2 position{m_Random.Float(0.0f, kPlayfieldSize.x),
                            m_Random.Float(0.0f, kPlayfieldSize.y)};
        m_Rocks.push_back(MakeRock(kind, size, position));
    }
}

void RogueGame::StartGame()
{
    NewRun(m_Meta.Selected, Stage{});
}

void RogueGame::OpenTitleExtra(usize index)
{
    if (index != 0)
        return;
    m_State = State::Hangar;
    m_StateTime = 0.0f;
    m_HangarCursor = 0;
    m_HangarMessage.clear();
}

void RogueGame::NewRun(ShipType ship, const Stage& stage)
{
    m_State = State::Playing;
    m_StateTime = 0.0f;
    m_ShipType = ship;
    m_Upgrades = {};
    m_Upgrades.Add(GetShipInfo(ship).Starting); // (no-op for "none")
    m_Score = 0;
    m_RunScrap = 0;
    m_BossesDefeated = 0;
    m_WavesCleared = 0;
    m_Victory = false;
    m_ScrapEarned = 0;
    m_NewRank = HighScoreTable::kMaxEntries;
    m_Rocks.clear();
    m_Shots.clear();
    m_EnemyShots.clear();
    m_Enemies.clear();
    m_Pickups.clear();
    m_Blasts.clear();
    m_Boss.reset();

    m_Ship.Reset(kPlayfieldCenter);
    m_ShipAlive = true;
    m_DeathTimer = 0.0f;
    m_FireCooldown = 0.0f;
    m_HyperspaceCooldown = 0.0f;
    ApplyStats();
    m_Shield = m_Stats.ShieldCharges;
    m_Stage = stage;
    EM_INFO("Run started: {} (seed-driven), {}", GetShipInfo(ship).Name, stage.Label());
    StartStage();
}

void RogueGame::ApplyStats()
{
    m_Stats = ComputeStats(m_ShipType, m_Upgrades);
    m_Ship.ThrustScale = m_Stats.ThrustScale;
    m_Ship.TurnScale = 1.0f + 0.5f * (m_Stats.ThrustScale - 1.0f); // turning gains half as much
    m_MissileTimer = m_Stats.MissileInterval;
}

void RogueGame::StartStage()
{
    m_Plan = PlanWave(m_Random, m_Stage);
    m_ClearTimer = 0.0f;
    m_BannerTimer = kBannerTime;
    m_InvulnerableTimer = kStageInvulnerable;
    m_SaucerTimer = m_Random.Float(m_Plan.SaucerDelayMin, m_Plan.SaucerDelayMax);

    if (m_Stage.IsBoss()) {
        Boss boss;
        boss.Kind = BossFor(m_Stage.Sector);
        switch (boss.Kind) {
        case BossKind::GiantRock:
            boss.Radius = 90.0f;
            boss.MaxHealth = 40;
            boss.Position = {kPlayfieldCenter.x, -boss.Radius};
            boss.Velocity = {0.0f, 120.0f};
            boss.Spin = 0.35f;
            break;
        case BossKind::Mothership:
            boss.Radius = 62.0f;
            boss.MaxHealth = 60;
            boss.Position = {kPlayfieldCenter.x, -boss.Radius};
            boss.Velocity = {0.0f, 110.0f};
            boss.FireTimer = 2.0f;
            boss.SpawnTimer = 1.5f;
            break;
        case BossKind::Station:
            boss.Radius = 66.0f;
            boss.MaxHealth = 90;
            boss.Position = {kPlayfieldCenter.x, -boss.Radius};
            boss.Velocity = {0.0f, 170.0f};
            boss.FireTimer = 0.5f;
            break;
        }
        boss.Health = boss.MaxHealth;
        m_Boss = boss;
        // The station takes the middle of the field: move the ship out of its way.
        if (boss.Kind == BossKind::Station &&
            Emerald::Length(m_Ship.Position - kPlayfieldCenter) < 250.0f)
            m_Ship.Position = {220.0f, kPlayfieldCenter.y};
        PlaySound(SoundEvent::BossAlarm, kPlayfieldCenter);
    }
    SpawnWaveRocks(m_Plan);
    EM_INFO("{}: {} rocks{}", m_Stage.Label(), m_Plan.RockCount, m_Boss ? " + boss" : "");
}

void RogueGame::SpawnWaveRocks(const WavePlan& plan)
{
    for (u32 i = 0; i < plan.RockCount; ++i)
        m_Rocks.push_back(MakeRock(plan.Rocks[i], AsteroidSize::Large, SafeEdgePosition()));
}

Vec2 RogueGame::SafeEdgePosition()
{
    // Somewhere on an edge, never right on top of the ship.
    Vec2 position;
    do {
        const bool onVerticalEdge = m_Random.Chance(0.5f);
        position = onVerticalEdge ? Vec2(m_Random.Chance(0.5f) ? 0.0f : kPlayfieldSize.x,
                                         m_Random.Float(0.0f, kPlayfieldSize.y))
                                  : Vec2(m_Random.Float(0.0f, kPlayfieldSize.x),
                                         m_Random.Chance(0.5f) ? 0.0f : kPlayfieldSize.y);
    } while (Emerald::Length(WrappedDelta(position, m_Ship.Position)) < kSafeSpawnDistance);
    return Wrap(position);
}

Rock RogueGame::MakeRock(RockKind kind, AsteroidSize size, const Vec2& position)
{
    Rock rock;
    rock.Body = MakeAsteroid(m_Random, size, position);
    rock.Kind = kind;
    rock.Health = RockHealth(kind, static_cast<u32>(SizeIndex(size)));
    rock.Id = m_NextRockId++;
    if (kind == RockKind::Metal)
        rock.Body.Velocity *= 0.7f; // heavy
    else if (kind == RockKind::Splitter)
        rock.Body.Velocity *= 1.25f;
    return rock;
}

void RogueGame::CheckStageCleared(f32 dt)
{
    const bool cleared = m_ShipAlive && m_Rocks.empty() && !m_Boss;
    if (!cleared)
        return;
    if (m_ClearTimer <= 0.0f) {
        m_ClearTimer = kClearDelay;
        // Saucers leave, their shots fizzle; left-over scrap flies to the ship.
        m_Enemies.clear();
        m_EnemyShots.clear();
        return;
    }
    m_ClearTimer -= dt;
    if (m_ClearTimer > 0.0f)
        return;

    ++m_WavesCleared;
    const std::optional<Stage> next = NextStage(m_Stage);
    if (!next) {
        EndRun(true);
        return;
    }
    m_Stage = *next;
    // Shots in flight would hit the next wave's rocks as they appear.
    m_Shots.clear();
    for (const Pickup& p : m_Pickups)
        m_RunScrap += p.Value;
    m_Pickups.clear();
    OfferUpgrades();
}

void RogueGame::OfferUpgrades()
{
    m_Offers = DrawOffers(m_Random, m_Upgrades, m_Meta.UpgradePool(), 3);
    m_OfferCursor = m_Offers.size() / 2; // the middle card
    if (m_Offers.empty()) {
        StartStage(); // everything is maxed out already
        return;
    }
    m_State = State::Upgrade;
    m_StateTime = 0.0f;
    m_Ship.Thrusting = false;
    m_FlameLength = 0.0f;
}

void RogueGame::ChooseOffer(usize index)
{
    if (m_State != State::Upgrade || index >= m_Offers.size())
        return;
    const UpgradeId id = m_Offers[index];
    m_Upgrades.Add(id);
    ApplyStats();
    m_Shield = m_Stats.ShieldCharges; // shields recharge every wave
    m_ShieldGlow = m_Shield > 0 ? 1.0f : 0.0f;
    EM_INFO("Upgrade: {} (level {})", GetUpgradeInfo(id).Name, m_Upgrades.Level(id));
    PlaySound(SoundEvent::Upgrade, m_Ship.Position);
    m_State = State::Playing;
    m_StateTime = 0.0f;
    m_Offers.clear();
    StartStage();
}

void RogueGame::ClearStageForTest()
{
    m_Rocks.clear();
    m_Boss.reset();
    m_ClearTimer = 0.0f;
    // Two steps: one to notice, one past the delay.
    CheckStageCleared(0.0f);
    CheckStageCleared(kClearDelay + 0.01f);
}

void RogueGame::UpdateUpgradePick(const GameInput& input)
{
    if (m_StateTime < kPickDelay)
        return;
    const usize count = m_Offers.size();
    if (input.MenuLeftPressed || input.MenuUpPressed)
        m_OfferCursor = (m_OfferCursor + count - 1) % count;
    if (input.MenuRightPressed || input.MenuDownPressed)
        m_OfferCursor = (m_OfferCursor + 1) % count;
    if (input.ConfirmPressed)
        ChooseOffer(m_OfferCursor);
}

void RogueGame::EndRun(bool victory)
{
    m_Victory = victory;
    const u32 bossesDefeated = m_BossesDefeated;
    for (const Pickup& p : m_Pickups)
        m_RunScrap += p.Value; // whatever was still floating around
    m_Pickups.clear();
    m_ScrapEarned = ScrapForRun({.Score = m_Score,
                                 .WavesCleared = m_WavesCleared,
                                 .BossesDefeated = bossesDefeated,
                                 .ScrapCollected = m_RunScrap,
                                 .Victory = victory});
    m_Meta.AddRun(m_ScrapEarned, m_Stage.Sector);
    m_MetaChanged = true;
    m_Enemies.clear();
    m_EnemyShots.clear();
    m_Ship.Thrusting = false;
    m_StateTime = 0.0f;
    EM_INFO("Run over ({}): score {}, {}, {} waves, {} scrap", victory ? "victory" : "destroyed",
            m_Score, m_Stage.Label(), m_WavesCleared, m_ScrapEarned);
    if (m_HighScores.Qualifies(m_Score)) {
        m_State = State::EnterInitials;
        m_InitialsEntry.Begin();
    } else {
        m_State = State::Results;
    }
}

void RogueGame::ForceGameOver(u32 score)
{
    if (m_State != State::Playing)
        return;
    m_Score = score;
    m_ShipAlive = false;
    EndRun(false);
}

void RogueGame::UpdateInitials(const GameInput& input)
{
    if (!m_InitialsEntry.Update(input))
        return;
    const std::string note = m_Victory ? std::string("WIN") : m_Stage.ShortLabel();
    m_NewRank = m_HighScores.Insert(m_InitialsEntry.GetInitials(), m_Score, note);
    m_HighScoresChanged = true;
    m_State = State::Results;
    m_StateTime = 0.0f;
}

bool RogueGame::OpenDebugScreen(std::string_view name)
{
    // "<screen>+god": hits only flash the shield (for recording gameplay with a dumb bot).
    if (name.ends_with("+god")) {
        m_GodMode = true;
        name.remove_suffix(4);
    }
    // Runs with a few upgrades, so the screenshots show a bit of everything.
    const auto withUpgrades = [&](const Stage& stage, u32 picks) {
        NewRun(m_Meta.Selected, stage);
        for (u32 i = 0; i < picks; ++i) {
            const std::vector<UpgradeId> offers =
                DrawOffers(m_Random, m_Upgrades, m_Meta.UpgradePool(), 1);
            if (!offers.empty())
                m_Upgrades.Add(offers.front());
        }
        ApplyStats();
        m_Shield = m_Stats.ShieldCharges;
    };
    if (name == "hangar") {
        OpenTitleExtra(0);
    } else if (name == "upgrades") {
        withUpgrades(Stage{1, 1}, 2);
        m_Stage = {1, 2}; // as after clearing wave 1
        m_Rocks.clear();
        m_Enemies.clear();
        m_BannerTimer = 0.0f;
        OfferUpgrades();
    } else if (name.size() == 5 && name.starts_with("boss") && name[4] >= '1' && name[4] <= '3') {
        withUpgrades(Stage{static_cast<u32>(name[4] - '0'), kWavesPerSector + 1}, 5);
    } else if (name.size() == 7 && name.starts_with("sector") && name[6] >= '1' && name[6] <= '3') {
        withUpgrades(Stage{static_cast<u32>(name[6] - '0'), 2}, 3);
    } else if (name == "results") {
        withUpgrades(Stage{2, 3}, 4);
        m_Score = 23450;
        m_WavesCleared = 7;
        m_BossesDefeated = 1;
        m_RunScrap = 64;
        m_ShipAlive = false;
        EndRun(false);
        m_MetaChanged = false; // just a picture: the payout isn't saved
        m_State = State::Results;
    } else {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// The fixed step
// ---------------------------------------------------------------------------------------------

void RogueGame::Update(const GameInput& input, f32 dt)
{
    m_Time += dt;
    m_StateTime += dt;
    m_HangarMessageTimer = Emerald::Max(m_HangarMessageTimer - dt, 0.0f);

    switch (m_State) {
    case State::Attract:
    case State::Hangar:
        if (m_State == State::Hangar)
            UpdateHangar(input);
        for (Rock& rock : m_Rocks)
            rock.Body.Update(dt);
        break;
    case State::Playing:
        UpdatePlaying(input, dt);
        break;
    case State::Upgrade:
        UpdateUpgradePick(input);
        break;
    case State::EnterInitials:
    case State::Results:
        if (m_State == State::EnterInitials && m_StateTime >= kInputDelay)
            UpdateInitials(input);
        else if (m_State == State::Results && m_StateTime >= kInputDelay &&
                 (input.StartPressed || m_StateTime >= kBackToTitle))
            ShowTitle();   // the hangar is on the title screen's menu
        UpdateObjects(dt); // the field keeps drifting behind the text
        break;
    }
}

void RogueGame::UpdatePlaying(const GameInput& input, f32 dt)
{
    UpdateShip(input, dt);
    UpdateObjects(dt);
    UpdateEnemies(dt);
    UpdateBoss(dt);
    UpdatePickups(dt);
    HandleCollisions();
    m_BannerTimer = Emerald::Max(m_BannerTimer - dt, 0.0f);

    if (!m_ShipAlive) {
        m_DeathTimer -= dt;
        if (m_DeathTimer <= 0.0f)
            EndRun(false);
        return;
    }
    CheckStageCleared(dt);
}

void RogueGame::UpdateShip(const GameInput& input, f32 dt)
{
    m_ShieldGlow = Emerald::Max(m_ShieldGlow - dt * 1.5f, 0.0f);
    if (!m_ShipAlive)
        return;
    m_Ship.Update(input.Ship, dt);
    m_InvulnerableTimer = Emerald::Max(m_InvulnerableTimer - dt, 0.0f);
    m_HyperspaceCooldown = Emerald::Max(m_HyperspaceCooldown - dt, 0.0f);
    m_FireCooldown = Emerald::Max(m_FireCooldown - dt, 0.0f);
    m_FlameLength = m_Ship.Thrusting && m_Random.Chance(0.8f) ? m_Random.Float(6.0f, 16.0f) : 0.0f;

    // Hold fire to keep shooting (at the cooldown's pace); a tap always tries at once.
    if ((input.FirePressed || input.FireHeld) && m_FireCooldown <= 0.0f && m_ClearTimer <= 0.0f)
        FireVolley();
    if (input.HyperspacePressed)
        Hyperspace();

    if (m_Stats.MissileInterval > 0.0f && m_ClearTimer <= 0.0f) {
        m_MissileTimer -= dt;
        if (m_MissileTimer <= 0.0f) {
            m_MissileTimer = m_Stats.MissileInterval;
            LaunchMissile();
        }
    }
}

void RogueGame::FireVolley()
{
    const u32 perVolley = m_Stats.ShotsPerVolley + m_Stats.RearShots;
    u32 bullets = 0;
    for (const Shot& shot : m_Shots)
        bullets += shot.Missile ? 0 : 1;
    if (bullets + perVolley > m_Stats.MaxVolleys * perVolley)
        return;
    m_FireCooldown = m_Stats.FireCooldown;

    const auto fire = [&](f32 angle, const Vec2& from) {
        Shot shot;
        shot.Position = from;
        shot.Velocity =
            m_Ship.Velocity + FromAngle(angle) * (Bullet::kSpeed * m_Stats.BulletSpeedScale);
        shot.TimeLeft = Bullet::kLifetime * m_Stats.BulletLifetimeScale;
        shot.PierceLeft = m_Stats.Pierce;
        m_Shots.push_back(shot);
    };
    // Forward: fanned out symmetrically around the nose.
    const f32 middle = 0.5f * static_cast<f32>(m_Stats.ShotsPerVolley - 1);
    for (u32 i = 0; i < m_Stats.ShotsPerVolley; ++i)
        fire(m_Ship.Angle + (static_cast<f32>(i) - middle) * m_Stats.SpreadAngle,
             m_Ship.NosePosition());
    // Backwards: one straight, a second one slightly apart.
    for (u32 i = 0; i < m_Stats.RearShots; ++i)
        fire(m_Ship.Angle + Emerald::Pi + (i == 0 ? 0.0f : 0.2f), m_Ship.EnginePosition());
    PlaySound(SoundEvent::Fire, m_Ship.Position);
}

void RogueGame::LaunchMissile()
{
    // Only when there is something to chase.
    if (!NearestTarget(m_Ship.Position, 900.0f))
        return;
    Shot missile;
    missile.Position = m_Ship.Position;
    // Out to the side first, then it turns towards its target.
    const f32 side = m_Random.Chance(0.5f) ? Emerald::HalfPi : -Emerald::HalfPi;
    missile.Velocity = m_Ship.Velocity * 0.5f + FromAngle(m_Ship.Angle + side) * kMissileSpeed;
    missile.TimeLeft = 3.0f;
    missile.Missile = true;
    m_Shots.push_back(missile);
    PlaySound(SoundEvent::MissileLaunch, m_Ship.Position);
}

std::optional<Vec2> RogueGame::NearestTarget(const Vec2& from, f32 range) const
{
    std::optional<Vec2> best;
    f32 bestDistance = range;
    const auto consider = [&](const Vec2& p) {
        const f32 d = Emerald::Length(WrappedDelta(from, p));
        if (d < bestDistance) {
            bestDistance = d;
            best = p;
        }
    };
    for (const Rock& rock : m_Rocks)
        consider(rock.Body.Position);
    for (const Enemy& enemy : m_Enemies)
        consider(enemy.Body.Position);
    if (m_Boss && !m_Boss->Entering)
        consider(m_Boss->Position);
    return best;
}

void RogueGame::Hyperspace()
{
    if (m_HyperspaceCooldown > 0.0f)
        return;
    const Vec2 from = m_Ship.Position;
    PlaySound(SoundEvent::Hyperspace, from);
    m_Ship.Position = {m_Random.Float(0.0f, kPlayfieldSize.x),
                       m_Random.Float(0.0f, kPlayfieldSize.y)};
    m_Ship.Velocity = {};
    m_HyperspaceCooldown = kHyperspaceCooldown;
    // Hyper blast: a shock wave where the ship left and where it arrives.
    if (m_Stats.BlastRadius > 0.0f) {
        Explode(from, m_Stats.BlastRadius, m_Stats.BlastDamage, false);
        Explode(m_Ship.Position, m_Stats.BlastRadius, m_Stats.BlastDamage, false);
        m_InvulnerableTimer = Emerald::Max(m_InvulnerableTimer, 0.3f);
    }
}

void RogueGame::UpdateObjects(f32 dt)
{
    for (Rock& rock : m_Rocks) {
        rock.Body.Update(dt);
        rock.Flash = Emerald::Max(rock.Flash - dt, 0.0f);
    }
    UpdateShots(dt);
    for (usize i = 0; i < m_EnemyShots.size();) {
        EnemyShot& shot = m_EnemyShots[i];
        shot.Position += shot.Velocity * dt;
        shot.TimeLeft -= dt;
        bool gone = shot.TimeLeft <= 0.0f;
        if (shot.Wraps)
            shot.Position = Wrap(shot.Position);
        else
            gone = gone || shot.Position.x < -20.0f || shot.Position.y < -20.0f ||
                   shot.Position.x > kPlayfieldSize.x + 20.0f ||
                   shot.Position.y > kPlayfieldSize.y + 20.0f;
        if (gone) {
            shot = m_EnemyShots.back();
            m_EnemyShots.pop_back();
        } else {
            ++i;
        }
    }
}

void RogueGame::UpdateShots(f32 dt)
{
    for (usize i = 0; i < m_Shots.size();) {
        Shot& shot = m_Shots[i];
        if (shot.Missile) {
            // Steer towards the nearest target at a fixed speed.
            if (const std::optional<Vec2> target = NearestTarget(shot.Position, 700.0f))
                shot.Velocity = TurnTowards(shot.Velocity, WrappedDelta(shot.Position, *target),
                                            kMissileTurn * dt);
            shot.Velocity = Emerald::Normalize(shot.Velocity) * kMissileSpeed;
        }
        shot.Position = Wrap(shot.Position + shot.Velocity * dt);
        shot.TimeLeft -= dt;
        if (shot.TimeLeft <= 0.0f) {
            shot = m_Shots.back();
            m_Shots.pop_back();
        } else {
            ++i;
        }
    }
}

void RogueGame::SpawnSaucer(SaucerSize size)
{
    if (m_State != State::Playing)
        return;
    Enemy enemy;
    Saucer& saucer = enemy.Body;
    saucer.Size = size;
    const bool fromLeft = m_Random.Chance(0.5f);
    saucer.Position = {fromLeft ? -saucer.Radius() : kPlayfieldSize.x + saucer.Radius(),
                       m_Random.Float(0.15f, 0.85f) * kPlayfieldSize.y};
    saucer.Velocity = {fromLeft ? saucer.Speed() : -saucer.Speed(), 0.0f};
    saucer.FireTimer = 0.6f;
    saucer.TurnTimer = m_Random.Float(0.8f, 2.0f);
    enemy.Health = static_cast<i32>(m_Plan.SaucerHealth) + (size == SaucerSize::Large ? 1 : 0);
    m_Enemies.push_back(enemy);
}

std::optional<SaucerSize> RogueGame::GetSaucerSize() const
{
    if (m_Enemies.empty() || m_State != State::Playing)
        return std::nullopt;
    return m_Enemies.front().Body.Size;
}

void RogueGame::UpdateEnemies(f32 dt)
{
    // Random saucers in normal waves, while there are rocks to fight.
    if (!m_Stage.IsBoss() && m_ShipAlive && !m_Rocks.empty() && m_Enemies.empty()) {
        m_SaucerTimer -= dt;
        if (m_SaucerTimer <= 0.0f) {
            m_SaucerTimer = m_Random.Float(m_Plan.SaucerDelayMin, m_Plan.SaucerDelayMax);
            SpawnSaucer(m_Random.Chance(m_Plan.SmallSaucerChance) ? SaucerSize::Small
                                                                  : SaucerSize::Large);
        }
    }

    for (usize i = 0; i < m_Enemies.size();) {
        Enemy& enemy = m_Enemies[i];
        Saucer& saucer = enemy.Body;
        enemy.Flash = Emerald::Max(enemy.Flash - dt, 0.0f);
        bool gone = false;
        if (enemy.Escort) {
            // Escorts drift after the ship, wrapping everywhere.
            if (m_ShipAlive)
                saucer.Velocity = TurnTowards(
                    saucer.Velocity, WrappedDelta(saucer.Position, m_Ship.Position), 1.2f * dt);
            saucer.Position = Wrap(saucer.Position + saucer.Velocity * dt);
        } else {
            // Like the arcade saucer: across the field, now and then diagonally.
            saucer.Position += saucer.Velocity * dt;
            saucer.Position.y -=
                std::floor(saucer.Position.y / kPlayfieldSize.y) * kPlayfieldSize.y;
            saucer.TurnTimer -= dt;
            if (saucer.TurnTimer <= 0.0f) {
                saucer.TurnTimer = m_Random.Float(0.8f, 2.0f);
                saucer.Velocity.y = static_cast<f32>(m_Random.Int(-1, 1)) * saucer.Speed() * 0.6f;
            }
            gone = saucer.Velocity.x > 0.0f ? saucer.Position.x > kPlayfieldSize.x + saucer.Radius()
                                            : saucer.Position.x < -saucer.Radius();
        }
        if (gone) {
            enemy = m_Enemies.back();
            m_Enemies.pop_back();
            continue;
        }
        saucer.FireTimer -= dt;
        if (saucer.FireTimer <= 0.0f) {
            saucer.FireTimer +=
                enemy.Escort ? 1.8f : saucer.FireInterval() * m_Plan.SaucerFireScale;
            EnemyFire(enemy);
        }
        ++i;
    }
}

void RogueGame::EnemyFire(Enemy& enemy)
{
    if (m_EnemyShots.size() >= kMaxEnemyShots || !m_ShipAlive)
        return;
    const Saucer& saucer = enemy.Body;
    // Small saucers and escorts aim (better deeper in); the large ones shoot anywhere.
    Vec2 direction;
    if (saucer.Size == SaucerSize::Small || enemy.Escort) {
        const f32 error = enemy.Escort ? 0.35f : m_Plan.AimError;
        direction = AimDirection(saucer.Position, m_Ship.Position, m_Random.Float(-error, error));
    } else {
        direction = m_Random.Direction();
    }
    EnemyShot shot;
    shot.Position = saucer.Position + direction * saucer.Radius();
    shot.Velocity = direction * (Saucer::kBulletSpeed * m_Plan.SaucerBulletSpeed);
    m_EnemyShots.push_back(shot);
    PlaySound(SoundEvent::SaucerFire, saucer.Position);
}

void RogueGame::UpdateBoss(f32 dt)
{
    if (!m_Boss)
        return;
    Boss& boss = *m_Boss;
    boss.Flash = Emerald::Max(boss.Flash - dt, 0.0f);
    boss.Angle += boss.Spin * dt;

    // Fly in from the top to its spot; it can't be hurt (or hurt) until it's there.
    if (boss.Entering) {
        const f32 targetY = boss.Kind == BossKind::Mothership ? 150.0f
                            : boss.Kind == BossKind::Station  ? kPlayfieldCenter.y
                                                              : 220.0f;
        boss.Position += boss.Velocity * dt;
        if (boss.Position.y >= targetY) {
            boss.Position.y = targetY;
            boss.Entering = false;
            boss.Time = 0.0f;
            boss.Velocity = boss.Kind == BossKind::GiantRock ? Vec2(55.0f, 32.0f) : Vec2();
        }
        return;
    }
    boss.Time += dt;
    const f32 health = static_cast<f32>(boss.Health) / static_cast<f32>(boss.MaxHealth);
    // An aimed fan of `count` shots at the ship.
    const auto fan = [&](u32 count, f32 spacing, f32 speed) {
        if (!m_ShipAlive)
            return;
        const Vec2 aim = AimDirection(boss.Position, m_Ship.Position, 0.0f);
        const f32 base = std::atan2(aim.y, aim.x);
        const f32 middle = 0.5f * static_cast<f32>(count - 1);
        for (u32 i = 0; i < count && m_EnemyShots.size() < kMaxEnemyShots; ++i) {
            const Vec2 dir = FromAngle(base + (static_cast<f32>(i) - middle) * spacing);
            m_EnemyShots.push_back({.Position = boss.Position + dir * boss.Radius * 0.8f,
                                    .Velocity = dir * speed,
                                    .TimeLeft = 3.0f,
                                    .Wraps = false});
        }
        PlaySound(SoundEvent::SaucerFire, boss.Position);
    };

    switch (boss.Kind) {
    case BossKind::GiantRock:
        // Drifts around (wrapping) and sheds armored chunks at 75%, 50% and 25% health.
        boss.Position = Wrap(boss.Position + boss.Velocity * dt);
        if (boss.Shed < 3 && health <= 0.75f - 0.25f * static_cast<f32>(boss.Shed)) {
            ++boss.Shed;
            for (i32 k = 0; k < 2 + static_cast<i32>(boss.Shed / 2); ++k) {
                const Vec2 dir = m_Random.Direction();
                Rock chunk = MakeRock(RockKind::Metal, AsteroidSize::Medium,
                                      Wrap(boss.Position + dir * boss.Radius));
                chunk.Body.Velocity = dir * 110.0f + boss.Velocity;
                m_NewRocks.push_back(chunk);
            }
            PlaySound(SoundEvent::ExplosionLarge, boss.Position, boss.Velocity);
        }
        break;
    case BossKind::Mothership: {
        // Sways across the top, launching escort saucers and firing fans at the ship.
        boss.Position = {kPlayfieldCenter.x + 420.0f * std::sin(boss.Time * 0.35f),
                         150.0f + 40.0f * std::sin(boss.Time * 0.8f)};
        const bool angry = health < 0.5f;
        usize escorts = 0;
        for (const Enemy& e : m_Enemies)
            escorts += e.Escort ? 1 : 0;
        boss.SpawnTimer -= dt;
        if (boss.SpawnTimer <= 0.0f) {
            boss.SpawnTimer = angry ? 3.0f : 4.5f;
            if (escorts < (angry ? 4u : 3u)) {
                Enemy escort;
                escort.Escort = true;
                escort.Body.Size = SaucerSize::Small;
                escort.Body.Position = boss.Position + Vec2(0.0f, boss.Radius * 0.6f);
                escort.Body.Velocity = {m_Random.Float(-60.0f, 60.0f), 150.0f};
                escort.Body.FireTimer = 1.2f;
                m_Enemies.push_back(escort);
                PlaySound(SoundEvent::MissileLaunch, boss.Position);
            }
        }
        boss.FireTimer -= dt;
        if (boss.FireTimer <= 0.0f) {
            boss.FireTimer = angry ? 1.7f : 2.2f;
            fan(angry ? 5 : 3, 0.22f, 300.0f);
        }
        break;
    }
    case BossKind::Station: {
        // Sits in the middle and sprays rotating arms of bullets: more arms as it gets hurt,
        // with short pauses to slip through, and aimed shots at the end.
        boss.Spin = 0.5f;
        boss.PatternAngle += boss.PatternSpin * dt;
        if (std::fmod(boss.Time, 6.0f) < dt)
            boss.PatternSpin = -boss.PatternSpin;
        const u32 arms = health > 0.6f ? 3 : health > 0.3f ? 4 : 5;
        const bool firing = std::fmod(boss.Time, 4.7f) < 3.5f;
        boss.FireTimer -= dt;
        if (firing && boss.FireTimer <= 0.0f) {
            boss.FireTimer = 0.13f;
            for (u32 a = 0; a < arms && m_EnemyShots.size() < kMaxEnemyShots; ++a) {
                const Vec2 dir =
                    FromAngle(boss.PatternAngle +
                              Emerald::TwoPi * static_cast<f32>(a) / static_cast<f32>(arms));
                m_EnemyShots.push_back({.Position = boss.Position + dir * boss.Radius,
                                        .Velocity = dir * 185.0f,
                                        .TimeLeft = 5.0f,
                                        .Wraps = false});
            }
        }
        if (health <= 0.3f) {
            boss.SpawnTimer -= dt;
            if (boss.SpawnTimer <= 0.0f) {
                boss.SpawnTimer = 1.6f;
                fan(3, 0.18f, 320.0f);
            }
        }
        break;
    }
    }
}

void RogueGame::UpdatePickups(f32 dt)
{
    const bool vacuum = m_ClearTimer > 0.0f; // the stage is over: everything flies in
    for (usize i = 0; i < m_Pickups.size();) {
        Pickup& p = m_Pickups[i];
        p.Velocity *= std::exp(-1.5f * dt);
        bool collected = false;
        if (m_ShipAlive) {
            const Vec2 toShip = WrappedDelta(p.Position, m_Ship.Position);
            const f32 distance = Emerald::Length(toShip);
            if (distance < kPickupRange) {
                collected = true;
            } else if (vacuum || distance < m_Stats.PickupRadius) {
                p.Velocity += toShip / distance * (1400.0f * dt);
                const f32 speed = Emerald::Length(p.Velocity);
                if (speed > 520.0f)
                    p.Velocity *= 520.0f / speed;
            }
        }
        p.Position = Wrap(p.Position + p.Velocity * dt);
        if (!vacuum)
            p.TimeLeft -= dt;
        if (collected) {
            m_RunScrap += p.Value;
            PlaySound(SoundEvent::Pickup, p.Position);
        }
        if (collected || p.TimeLeft <= 0.0f) {
            p = m_Pickups.back();
            m_Pickups.pop_back();
        } else {
            ++i;
        }
    }
}

void RogueGame::DropScrap(const Vec2& position, u32 bits, u32 value)
{
    for (u32 i = 0; i < bits; ++i)
        m_Pickups.push_back({.Position = position,
                             .Velocity = m_Random.Direction() * m_Random.Float(30.0f, 110.0f),
                             .TimeLeft = m_Random.Float(8.0f, 11.0f),
                             .Value = value});
}

// ---------------------------------------------------------------------------------------------
// Collisions and damage
// ---------------------------------------------------------------------------------------------

void RogueGame::HandleCollisions()
{
    // Destroyed rocks (Health <= 0) are removed and broken here; breaking may queue blasts and
    // adds its fragments to m_NewRocks (they join the field at the end of the step).
    const auto removeDeadRocks = [&](bool byPlayer) {
        for (usize i = 0; i < m_Rocks.size();) {
            if (m_Rocks[i].Health > 0) {
                ++i;
                continue;
            }
            const Rock dead = m_Rocks[i];
            m_Rocks[i] = m_Rocks.back();
            m_Rocks.pop_back();
            BreakRock(dead, byPlayer);
        }
    };

    // The ship's shots: rocks first (piercing shots go on after a kill), then saucers and boss.
    for (usize s = 0; s < m_Shots.size();) {
        Shot& shot = m_Shots[s];
        const i32 damage = shot.Missile ? 2 : 1;
        bool used = false;
        for (Rock& rock : m_Rocks) {
            if (rock.Health <= 0 || rock.Id == shot.LastHit ||
                !CirclesOverlap(shot.Position, 0.0f, rock.Body.Position, rock.Body.Radius))
                continue;
            const bool destroyed = DamageRock(rock, damage);
            if (destroyed && shot.PierceLeft > 0 && !shot.Missile) {
                --shot.PierceLeft;
                shot.LastHit = rock.Id;
            } else {
                used = true;
                break;
            }
        }
        for (usize e = 0; !used && e < m_Enemies.size(); ++e) {
            if (CirclesOverlap(shot.Position, 0.0f, m_Enemies[e].Body.Position,
                               m_Enemies[e].Body.Radius())) {
                DamageEnemy(e, damage);
                used = true;
            }
        }
        if (!used && m_Boss && !m_Boss->Entering &&
            CirclesOverlap(shot.Position, 0.0f, m_Boss->Position, m_Boss->Radius)) {
            DamageBoss(damage); // armor: even piercing shots stop here
            used = true;
        }
        if (used) {
            m_Shots[s] = m_Shots.back();
            m_Shots.pop_back();
        } else {
            ++s;
        }
    }
    removeDeadRocks(true);

    // The ship against everything (rocks and saucers it rams break; the boss doesn't).
    if (m_ShipAlive && m_InvulnerableTimer <= 0.0f) {
        for (Rock& rock : m_Rocks) {
            if (CirclesOverlap(m_Ship.Position, Ship::kRadius, rock.Body.Position,
                               rock.Body.Radius * 0.85f)) {
                rock.Health = 0;
                HitShip();
                break;
            }
        }
    }
    removeDeadRocks(true);
    if (m_ShipAlive && m_InvulnerableTimer <= 0.0f) {
        for (usize e = 0; e < m_Enemies.size(); ++e) {
            if (CirclesOverlap(m_Ship.Position, Ship::kRadius, m_Enemies[e].Body.Position,
                               m_Enemies[e].Body.Radius())) {
                DamageEnemy(e, 99);
                HitShip();
                break;
            }
        }
    }
    if (m_ShipAlive && m_InvulnerableTimer <= 0.0f && m_Boss && !m_Boss->Entering &&
        CirclesOverlap(m_Ship.Position, Ship::kRadius, m_Boss->Position, m_Boss->Radius * 0.9f)) {
        HitShip();
        // Bounce off, so a shielded ship isn't stuck inside it.
        m_Ship.Velocity = AimDirection(m_Boss->Position, m_Ship.Position, 0.0f) * 300.0f;
    }
    for (usize i = 0; i < m_EnemyShots.size();) {
        if (m_ShipAlive && m_InvulnerableTimer <= 0.0f &&
            CirclesOverlap(m_EnemyShots[i].Position, 0.0f, m_Ship.Position, Ship::kRadius)) {
            m_EnemyShots[i] = m_EnemyShots.back();
            m_EnemyShots.pop_back();
            HitShip();
        } else {
            ++i;
        }
    }

    // Saucers that fly into rocks: both break, no points (like the arcade).
    for (usize e = 0; e < m_Enemies.size();) {
        bool crashed = false;
        for (Rock& rock : m_Rocks) {
            if (rock.Health > 0 &&
                CirclesOverlap(m_Enemies[e].Body.Position, m_Enemies[e].Body.Radius(),
                               rock.Body.Position, rock.Body.Radius * 0.85f)) {
                rock.Health = 0;
                crashed = true;
                break;
            }
        }
        if (crashed) {
            const Enemy enemy = m_Enemies[e];
            m_Enemies[e] = m_Enemies.back();
            m_Enemies.pop_back();
            PlaySound(SoundEvent::SaucerExplosion, enemy.Body.Position, enemy.Body.Velocity);
        } else {
            ++e;
        }
    }
    removeDeadRocks(false);

    // Blasts, including the chain reactions of explosive rocks they set off.
    for (u32 guard = 0; !m_Blasts.empty() && guard < 64; ++guard) {
        const PendingBlast blast = m_Blasts.back();
        m_Blasts.pop_back();
        for (Rock& rock : m_Rocks) {
            const f32 distance = Emerald::Length(WrappedDelta(blast.Position, rock.Body.Position));
            if (rock.Health > 0 && distance < blast.Radius + rock.Body.Radius * 0.5f)
                DamageRock(rock, blast.Damage);
        }
        for (usize e = m_Enemies.size(); e-- > 0;) {
            if (CirclesOverlap(blast.Position, blast.Radius, m_Enemies[e].Body.Position,
                               m_Enemies[e].Body.Radius()))
                DamageEnemy(e, blast.Damage);
        }
        if (m_Boss && !m_Boss->Entering &&
            CirclesOverlap(blast.Position, blast.Radius, m_Boss->Position, m_Boss->Radius))
            DamageBoss(blast.Damage + 1);
        if (blast.HurtsShip && m_ShipAlive &&
            CirclesOverlap(blast.Position, blast.Radius * 0.6f, m_Ship.Position, Ship::kRadius))
            HitShip();
        removeDeadRocks(true);
    }
    m_Blasts.clear();

    m_Rocks.insert(m_Rocks.end(), m_NewRocks.begin(), m_NewRocks.end());
    m_NewRocks.clear();
}

bool RogueGame::DamageRock(Rock& rock, i32 damage)
{
    rock.Health -= damage;
    if (rock.Health <= 0)
        return true;
    rock.Flash = 0.15f;
    PlaySound(SoundEvent::MetalHit, rock.Body.Position, rock.Body.Velocity);
    return false;
}

void RogueGame::BreakRock(const Rock& rock, bool byPlayer)
{
    const Asteroid& body = rock.Body;
    if (byPlayer)
        AddScore(ScoreFor(body.Size) * (rock.Kind == RockKind::Normal ? 1 : 2));

    // Metal breaks into metal, splitters into three faster splitters, the rest into plain rock.
    if (const std::optional<AsteroidSize> smaller = SmallerSize(body.Size)) {
        const bool splitter = rock.Kind == RockKind::Splitter;
        const RockKind kind = rock.Kind == RockKind::Explosive ? RockKind::Normal : rock.Kind;
        for (i32 k = 0; k < (splitter ? 3 : 2); ++k) {
            Rock fragment = MakeRock(kind, *smaller, body.Position);
            fragment.Body.Velocity += body.Velocity * 0.5f;
            if (splitter)
                fragment.Body.Velocity *= 1.3f;
            m_NewRocks.push_back(fragment);
        }
    }
    const SoundEvent boom = body.Size == AsteroidSize::Large    ? SoundEvent::ExplosionLarge
                            : body.Size == AsteroidSize::Medium ? SoundEvent::ExplosionMedium
                                                                : SoundEvent::ExplosionSmall;
    PlaySound(boom, body.Position, body.Velocity);

    if (rock.Kind == RockKind::Explosive)
        Explode(body.Position, 60.0f + body.Radius * 1.4f, 2, true);
    // Scrap: metal always drops some, other rocks now and then (big ones more often).
    if (rock.Kind == RockKind::Metal)
        DropScrap(body.Position, 1, 2);
    else if (m_Random.Chance(body.Size == AsteroidSize::Large ? 0.3f : 0.12f))
        DropScrap(body.Position, 1, 1);
}

void RogueGame::DamageEnemy(usize index, i32 damage)
{
    Enemy& enemy = m_Enemies[index];
    enemy.Health -= damage;
    if (enemy.Health > 0) {
        enemy.Flash = 0.15f;
        PlaySound(SoundEvent::MetalHit, enemy.Body.Position, enemy.Body.Velocity);
        return;
    }
    // Deeper sectors pay more for their tougher saucers.
    AddScore(enemy.Body.Score() * (1 + (m_Stage.Sector - 1)) / (enemy.Escort ? 2u : 1u));
    DropScrap(enemy.Body.Position, enemy.Escort ? 1 : 3, 1);
    PlaySound(SoundEvent::SaucerExplosion, enemy.Body.Position, enemy.Body.Velocity);
    EM_INFO("Saucer destroyed (score {})", m_Score);
    m_Enemies[index] = m_Enemies.back();
    m_Enemies.pop_back();
}

void RogueGame::DamageBoss(i32 damage)
{
    Boss& boss = *m_Boss;
    if (boss.Entering)
        return;
    // At most one clank per flash, so rapid fire doesn't turn into noise.
    if (boss.Flash <= 0.0f)
        PlaySound(SoundEvent::MetalHit, boss.Position);
    boss.Flash = 0.08f;
    boss.Health -= damage;
    if (boss.Health <= 0)
        DestroyBoss();
}

void RogueGame::DestroyBoss()
{
    const Boss boss = *m_Boss;
    m_Boss.reset();
    ++m_BossesDefeated;
    AddScore(2500 * m_Stage.Sector);
    DropScrap(boss.Position, 20, 2);
    PlaySound(SoundEvent::BossExplosion, boss.Position);
    EM_INFO("Boss destroyed (score {})", m_Score);
    m_EnemyShots.clear(); // its bullet pattern vanishes with it
    // The giant rock's core splits into four big armored chunks.
    if (boss.Kind == BossKind::GiantRock) {
        for (i32 k = 0; k < 4; ++k) {
            const Vec2 dir = FromAngle(Emerald::HalfPi * static_cast<f32>(k) + 0.6f);
            Rock chunk =
                MakeRock(RockKind::Metal, AsteroidSize::Large, Wrap(boss.Position + dir * 30.0f));
            chunk.Body.Velocity = dir * 90.0f;
            m_NewRocks.push_back(chunk);
        }
    }
}

void RogueGame::Explode(const Vec2& position, f32 radius, i32 damage, bool hurtsShip)
{
    m_Blasts.push_back({position, radius, damage, hurtsShip});
    PlaySound(SoundEvent::Blast, position);
}

void RogueGame::HitShip()
{
    if (!m_ShipAlive || m_InvulnerableTimer > 0.0f)
        return;
    if (m_Shield > 0 || m_GodMode) {
        if (m_Shield > 0 && !m_GodMode)
            --m_Shield;
        m_InvulnerableTimer = kShieldInvulnerable;
        m_ShieldGlow = 1.0f;
        PlaySound(SoundEvent::ShieldHit, m_Ship.Position, m_Ship.Velocity);
        EM_INFO("Shield hit, {} left", m_Shield);
        return;
    }
    m_ShipAlive = false;
    m_Ship.Thrusting = false;
    m_FlameLength = 0.0f;
    m_DeathTimer = kDeathDelay;
    ++m_ShipsLost;
    PlaySound(SoundEvent::ShipExplosion, m_Ship.Position, m_Ship.Velocity);
    EM_INFO("Ship destroyed at {} (score {})", m_Stage.Label(), m_Score);
}

void RogueGame::AddScore(u32 points)
{
    m_Score += points;
}

void RogueGame::PlaySound(SoundEvent event, const Vec2& position, const Vec2& velocity)
{
    const f32 pan = (position.x / kPlayfieldSize.x * 2.0f - 1.0f) * 0.6f;
    m_Sounds.push_back({event, pan, position, velocity});
}

bool RogueGame::IsShipVisible() const
{
    if (!m_ShipAlive || (m_State != State::Playing && m_State != State::Upgrade))
        return false;
    return m_InvulnerableTimer <= 0.0f || std::fmod(m_Time * 8.0f, 2.0f) < 1.0f;
}

// ---------------------------------------------------------------------------------------------
// Hangar (between runs)
// ---------------------------------------------------------------------------------------------

const std::vector<RogueGame::HangarRow>& RogueGame::GetHangarRows()
{
    // The ships, the upgrades that cost scrap, then BACK (an upgrade row with Index = Count).
    static const std::vector<HangarRow> rows = [] {
        std::vector<HangarRow> list;
        for (usize i = 0; i < kShipCount; ++i)
            list.push_back({true, i});
        for (usize i = 0; i < kUpgradeCount; ++i)
            if (GetUpgradeInfo(static_cast<UpgradeId>(i)).UnlockCost > 0)
                list.push_back({false, i});
        list.push_back({false, kUpgradeCount});
        return list;
    }();
    return rows;
}

Vec2 RogueGame::GetHangarIconPosition(usize row)
{
    return {400.0f, HangarRowY(row) + 10.0f};
}

void RogueGame::UpdateHangar(const GameInput& input)
{
    const std::vector<HangarRow>& rows = GetHangarRows();
    if (input.MenuUpPressed)
        m_HangarCursor = (m_HangarCursor + rows.size() - 1) % rows.size();
    if (input.MenuDownPressed)
        m_HangarCursor = (m_HangarCursor + 1) % rows.size();
    const HangarRow& row = rows[m_HangarCursor];
    const bool back = !row.IsShip && row.Index == kUpgradeCount;
    if (input.MenuBackPressed || (input.ConfirmPressed && back)) {
        m_State = State::Attract; // back to the title (its rocks kept drifting)
        m_StateTime = 0.0f;
        return;
    }
    if (!input.ConfirmPressed)
        return;

    const auto say = [&](std::string message) {
        m_HangarMessage = std::move(message);
        m_HangarMessageTimer = 1.8f;
    };
    if (row.IsShip) {
        const ShipType type = static_cast<ShipType>(row.Index);
        if (m_Meta.HasShip(type)) {
            m_Meta.Select(type);
            PlaySound(SoundEvent::Upgrade, kPlayfieldCenter);
            say(std::string(GetShipInfo(type).Name) + " SELECTED");
        } else if (m_Meta.BuyShip(type)) {
            m_Meta.Select(type);
            PlaySound(SoundEvent::Purchase, kPlayfieldCenter);
            say(std::string(GetShipInfo(type).Name) + " BOUGHT");
        } else {
            PlaySound(SoundEvent::MetalHit, kPlayfieldCenter);
            say("NOT ENOUGH SCRAP");
        }
    } else {
        const UpgradeId id = static_cast<UpgradeId>(row.Index);
        if (m_Meta.IsInPool(id)) {
            say("ALREADY IN THE UPGRADE POOL");
        } else if (m_Meta.BuyUpgrade(id)) {
            PlaySound(SoundEvent::Purchase, kPlayfieldCenter);
            say(std::string(GetUpgradeInfo(id).Name) + " ADDED TO THE POOL");
        } else {
            PlaySound(SoundEvent::MetalHit, kPlayfieldCenter);
            say("NOT ENOUGH SCRAP");
        }
    }
    m_MetaChanged = true;
}

// ---------------------------------------------------------------------------------------------
// HUD and screens (vector font; the app adds sprites at the Get*IconPosition spots)
// ---------------------------------------------------------------------------------------------

Vec2 RogueGame::GetOfferIconPosition(usize index)
{
    return {CardCenterX(index, 3), kCardTop + 80.0f}; // (cards are laid out for 3)
}

Vec2 RogueGame::GetHudIconPosition(usize index)
{
    return {40.0f + 44.0f * static_cast<f32>(index), 684.0f};
}

void RogueGame::DrawHighScoreTable(Emerald::Renderer2D& r, f32 top) const
{
    Asteroids::DrawHighScoreTable(r, m_HighScores, top, m_NewRank, m_Time);
}

void RogueGame::DrawHud(Emerald::Renderer2D& r) const
{
    switch (m_State) {
    case State::Attract:
        return; // the title screen has its own text
    case State::Hangar:
        DrawHangar(r);
        return;
    case State::Playing:
        DrawPlayHud(r);
        return;
    case State::Upgrade:
        DrawPlayHud(r);
        DrawUpgradePick(r);
        return;
    case State::EnterInitials:
        m_InitialsEntry.Draw(r, m_Prompts, m_Time);
        return;
    case State::Results:
        DrawResults(r);
        return;
    }
}

void RogueGame::DrawPlayHud(Emerald::Renderer2D& r) const
{
    const f32 centerX = kPlayfieldCenter.x;
    const std::string score =
        m_Score < 10 ? "0" + std::to_string(m_Score) : std::to_string(m_Score);
    Text::Draw(r, score, {40.0f, 24.0f}, 30.0f, kTextColor);
    Text::DrawCentered(r, m_Stage.Label(), centerX, 22.0f, 16.0f, kDimTextColor);
    const std::string scrap = "SCRAP " + std::to_string(m_RunScrap);
    Text::Draw(r, scrap, {1240.0f - Text::Width(scrap, 18.0f), 28.0f}, 18.0f, kWarm);

    // Shield charges under the score: filled = ready.
    for (u32 i = 0; i < m_Stats.ShieldCharges; ++i) {
        const Vec2 c{52.0f + 22.0f * static_cast<f32>(i), 82.0f};
        const bool ready = i < m_Shield;
        r.DrawCircle(c, 7.0f, ready ? kAccent : kDimTextColor, 16);
        if (ready)
            r.DrawCircle(c, 3.5f, kAccent, 10);
    }

    // Levels next to the collected upgrades' icons (the app draws the icons).
    usize slot = 0;
    for (usize i = 0; i < kUpgradeCount; ++i) {
        const u8 level = m_Upgrades.Level(static_cast<UpgradeId>(i));
        if (level == 0)
            continue;
        if (level > 1)
            Text::Draw(r, std::to_string(level), GetHudIconPosition(slot) + Vec2(12.0f, 2.0f),
                       12.0f, kTextColor);
        ++slot;
    }

    // The boss's name and health bar along the top.
    if (m_Boss) {
        const Boss& boss = *m_Boss;
        const char* names[] = {"THE MONOLITH", "THE MOTHERSHIP", "THE BASTION"};
        Text::DrawCentered(r, names[static_cast<usize>(boss.Kind)], centerX, 48.0f, 16.0f, kDanger);
        constexpr Vec2 kBarSize{480.0f, 10.0f};
        const Vec2 barTop{centerX - 0.5f * kBarSize.x, 72.0f};
        const f32 fill =
            static_cast<f32>(Emerald::Max(boss.Health, 0)) / static_cast<f32>(boss.MaxHealth);
        FillRect(r, barTop, {kBarSize.x * fill, kBarSize.y},
                 boss.Flash > 0.0f ? kTextColor : kDanger);
        r.DrawRect(barTop - Vec2(2.0f), kBarSize + Vec2(4.0f), kDimTextColor);
    }

    if (m_BannerTimer > 0.0f && m_State == State::Playing) {
        Text::DrawCentered(r, m_Stage.Label(), centerX, 200.0f, 30.0f, kTextColor);
        if (m_Stage.IsBoss() && std::fmod(m_Time * 3.0f, 2.0f) < 1.3f)
            Text::DrawCentered(r, "WARNING: BOSS APPROACHING", centerX, 250.0f, 20.0f, kDanger);
    }
    if (m_ClearTimer > 0.0f)
        Text::DrawCentered(r, m_Stage.IsBoss() ? "SECTOR CLEAR" : "WAVE CLEAR", centerX, 230.0f,
                           34.0f, kAccent);
}

void RogueGame::DrawUpgradePick(Emerald::Renderer2D& r) const
{
    const f32 centerX = kPlayfieldCenter.x;
    Text::DrawCentered(r, "CHOOSE AN UPGRADE", centerX, 110.0f, 34.0f, kTextColor);
    Text::DrawCentered(r, "NEXT: " + m_Stage.Label(), centerX, 160.0f, 16.0f, kDimTextColor);

    for (usize i = 0; i < m_Offers.size(); ++i) {
        const UpgradeInfo& info = GetUpgradeInfo(m_Offers[i]);
        const bool selected = i == m_OfferCursor;
        const f32 cx = CardCenterX(i, 3);
        const Vec2 topLeft{cx - 0.5f * kCardWidth, kCardTop};
        // The chosen card has a pulsing double frame.
        const f32 pulse = 0.75f + 0.25f * std::sin(m_Time * 6.0f);
        const Vec4 frame = selected ? Vec4(kAccent.x, kAccent.y, kAccent.z, pulse) : kDimTextColor;
        r.DrawRect(topLeft, {kCardWidth, kCardHeight}, frame);
        if (selected)
            r.DrawRect(topLeft - Vec2(4.0f), Vec2(kCardWidth, kCardHeight) + Vec2(8.0f), frame);

        const Vec4 text = selected ? kTextColor : Vec4(0.8f, 0.82f, 0.86f, 1.0f);
        Text::DrawCentered(r, info.Name, cx, kCardTop + 150.0f, 20.0f, text);
        const u8 level = m_Upgrades.Level(m_Offers[i]);
        const std::string levelText =
            level == 0 ? "NEW"
                       : "LEVEL " + std::to_string(level) + " > " + std::to_string(level + 1);
        Text::DrawCentered(r, levelText + "  (MAX " + std::to_string(info.MaxLevel) + ")", cx,
                           kCardTop + 185.0f, 14.0f, kWarm);
        f32 y = kCardTop + 220.0f;
        for (const std::string& line : WrapText(info.Description, 14.0f, kCardWidth - 30.0f)) {
            Text::DrawCentered(r, line, cx, y, 14.0f, text);
            y += 22.0f;
        }
    }

    if (m_StateTime >= kPickDelay)
        Text::DrawCentered(r, "LEFT / RIGHT: CHOOSE     " + m_Prompts.Confirm + ": TAKE", centerX,
                           560.0f, 18.0f, kTextColor);
}

void RogueGame::DrawHangar(Emerald::Renderer2D& r) const
{
    const f32 centerX = kPlayfieldCenter.x;
    Text::DrawCentered(r, "HANGAR", centerX, 40.0f, 40.0f, kTextColor);
    Text::DrawCentered(r, "SCRAP: " + std::to_string(m_Meta.Scrap), centerX, 104.0f, 22.0f, kWarm);
    Text::Draw(r, "SHIPS", {340.0f, 160.0f}, 16.0f, kDimTextColor);
    Text::Draw(r, "UPGRADES FOR THE POOL", {340.0f, HangarRowY(kShipCount) - 36.0f}, 16.0f,
               kDimTextColor);

    const std::vector<HangarRow>& rows = GetHangarRows();
    std::string_view description;
    for (usize i = 0; i < rows.size(); ++i) {
        const HangarRow& row = rows[i];
        const f32 y = HangarRowY(i);
        const bool selected = i == m_HangarCursor;
        std::string name;
        std::string status;
        Vec4 statusColor = kDimTextColor;
        if (row.IsShip) {
            const ShipType type = static_cast<ShipType>(row.Index);
            const ShipInfo& info = GetShipInfo(type);
            name = info.Name;
            if (selected)
                description = info.Description;
            if (m_Meta.Selected == type) {
                status = "SELECTED";
                statusColor = kAccent;
            } else if (m_Meta.HasShip(type)) {
                status = "OWNED";
            } else {
                status = std::to_string(info.Cost) + " SCRAP";
                statusColor = m_Meta.Scrap >= info.Cost ? kWarm : kDimTextColor;
            }
        } else if (row.Index == kUpgradeCount) {
            name = "BACK";
        } else {
            const UpgradeId id = static_cast<UpgradeId>(row.Index);
            const UpgradeInfo& info = GetUpgradeInfo(id);
            name = info.Name;
            if (selected)
                description = info.Description;
            if (m_Meta.IsInPool(id)) {
                status = "IN POOL";
                statusColor = kAccent;
            } else {
                status = std::to_string(info.UnlockCost) + " SCRAP";
                statusColor = m_Meta.Scrap >= info.UnlockCost ? kWarm : kDimTextColor;
            }
        }
        const Vec4 color = selected ? kTextColor : Vec4(0.75f, 0.78f, 0.82f, 1.0f);
        Text::Draw(r, name, {440.0f, y}, 20.0f, color);
        Text::Draw(r, status, {940.0f - Text::Width(status, 16.0f), y + 3.0f}, 16.0f, statusColor);
        if (selected)
            r.DrawRect({340.0f, y - 10.0f}, {620.0f, kHangarRowHeight - 2.0f}, kAccent);
    }

    const bool message = m_HangarMessageTimer > 0.0f && !m_HangarMessage.empty();
    Text::DrawCentered(r, message ? std::string_view(m_HangarMessage) : description, centerX,
                       640.0f, 16.0f, message ? kWarm : kTextColor);
    Text::DrawCentered(r, m_Prompts.Confirm + ": BUY / SELECT     ESC: BACK", centerX, 680.0f,
                       14.0f, kDimTextColor);
}

void RogueGame::DrawResults(Emerald::Renderer2D& r) const
{
    const f32 centerX = kPlayfieldCenter.x;
    Text::DrawCentered(r, m_Victory ? "ALL SECTORS CLEARED" : "SHIP DESTROYED", centerX, 44.0f,
                       40.0f, m_Victory ? kAccent : kDanger);
    const std::string reached = m_Victory ? "VICTORY" : "REACHED " + m_Stage.Label();
    Text::DrawCentered(r, reached, centerX, 108.0f, 18.0f, kTextColor);
    Text::DrawCentered(r,
                       "SCORE " + std::to_string(m_Score) + "     BOSSES " +
                           std::to_string(m_BossesDefeated) + "     UPGRADES " +
                           std::to_string(m_Upgrades.Total()),
                       centerX, 140.0f, 18.0f, kTextColor);
    Text::DrawCentered(r,
                       "SCRAP EARNED +" + std::to_string(m_ScrapEarned) + "   (TOTAL " +
                           std::to_string(m_Meta.Scrap) + ")",
                       centerX, 174.0f, 20.0f, kWarm);
    if (!m_HighScores.GetEntries().empty())
        DrawHighScoreTable(r, 226.0f);
    if (m_StateTime >= kInputDelay && std::fmod(m_Time, 1.2f) < 0.8f)
        Text::DrawCentered(r, m_Prompts.Start, centerX, 668.0f, 22.0f, kTextColor);
}

} // namespace Asteroids::Rogue
