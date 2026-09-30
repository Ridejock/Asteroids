#include "Upgrades.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Asteroids::Rogue {

namespace {

// Same order as UpgradeId.
constexpr std::array<UpgradeInfo, kUpgradeCount> kUpgrades{{
    {"SPREAD SHOT", "TWO MORE SHOTS PER VOLLEY", "spread", 2, 0},
    {"PIERCING ROUNDS", "SHOTS PASS THROUGH ONE MORE ROCK", "pierce", 3, 0},
    {"RAPID FIRE", "FIRE FASTER, MORE SHOTS ON SCREEN", "rapid", 3, 0},
    {"SHIELD", "ABSORBS A HIT, RECHARGES EACH WAVE", "shield", 3, 0},
    {"THRUSTER", "FASTER ENGINE AND TURNING", "thruster", 3, 0},
    {"HYPER BLAST", "HYPERSPACE HURTS NEARBY ENEMIES", "hyperblast", 3, 0},
    {"HOMING MISSILES", "AUTO-FIRES A SEEKING MISSILE", "homing", 3, 300},
    {"REAR GUN", "ALSO FIRES BACKWARDS", "reargun", 2, 200},
    {"LONG RANGE", "FASTER, LONGER-LIVED SHOTS", "longrange", 2, 150},
    {"SCRAP MAGNET", "PULLS IN SCRAP FROM FURTHER AWAY", "magnet", 2, 100},
}};

// Same order as ShipType.
constexpr std::array<ShipInfo, kShipCount> kShips{{
    {"STRIKER", "BALANCED ALL-ROUNDER", "striker", 0, 1.0f, 1.0f, 0, UpgradeId::Count},
    {"BULWARK", "HEAVY: STARTS WITH A SHIELD", "bulwark", 250, 0.85f, 1.15f, 0, UpgradeId::Shield},
    {"WASP", "FAST AND TWITCHY: RAPID FIRE", "wasp", 400, 1.3f, 0.9f, 0, UpgradeId::RapidFire},
    {"LANCER", "SLOW GUN, PIERCING LONG SHOTS", "lancer", 600, 1.0f, 1.25f, 1, UpgradeId::Piercing},
}};

constexpr f32 kBaseCooldown = 0.16f; // seconds between volleys without upgrades

} // namespace

const UpgradeInfo& GetUpgradeInfo(UpgradeId id)
{
    return kUpgrades[static_cast<usize>(id)];
}

const ShipInfo& GetShipInfo(ShipType type)
{
    return kShips[static_cast<usize>(type)];
}

bool UpgradeSet::Add(UpgradeId id)
{
    if (id == UpgradeId::Count || IsMaxed(id))
        return false;
    ++m_Levels[static_cast<usize>(id)];
    return true;
}

u32 UpgradeSet::Total() const
{
    u32 total = 0;
    for (const u8 level : m_Levels)
        total += level;
    return total;
}

std::vector<UpgradeId> DrawOffers(Random& random, const UpgradeSet& owned,
                                  const std::vector<UpgradeId>& pool, usize count)
{
    std::vector<UpgradeId> candidates;
    for (const UpgradeId id : pool)
        if (!owned.IsMaxed(id) &&
            std::find(candidates.begin(), candidates.end(), id) == candidates.end())
            candidates.push_back(id);
    // A partial Fisher-Yates shuffle with our Random, so a seed always gives the same offers.
    const usize n = std::min(count, candidates.size());
    for (usize i = 0; i < n; ++i) {
        const usize j = static_cast<usize>(
            random.Int(static_cast<i32>(i), static_cast<i32>(candidates.size() - 1)));
        std::swap(candidates[i], candidates[j]);
    }
    candidates.resize(n);
    return candidates;
}

CombatStats ComputeStats(ShipType ship, const UpgradeSet& upgrades)
{
    const ShipInfo& info = GetShipInfo(ship);
    const auto level = [&](UpgradeId id) { return static_cast<u32>(upgrades.Level(id)); };
    const auto levelF = [&](UpgradeId id) { return static_cast<f32>(upgrades.Level(id)); };

    CombatStats s;
    s.ShotsPerVolley = 1 + 2 * level(UpgradeId::SpreadShot);
    s.SpreadAngle = 0.16f;
    s.Pierce = level(UpgradeId::Piercing);
    // Each rapid fire level: 25% shorter cooldown and two more volleys on screen.
    s.FireCooldown =
        kBaseCooldown * info.CooldownScale * std::pow(0.75f, levelF(UpgradeId::RapidFire));
    s.MaxVolleys = 4 + info.ExtraShots + 2 * level(UpgradeId::RapidFire);
    s.ShieldCharges = level(UpgradeId::Shield);
    s.MissileInterval = level(UpgradeId::Homing) > 0 ? 3.0f / levelF(UpgradeId::Homing) : 0.0f;
    s.ThrustScale = info.ThrustScale * (1.0f + 0.2f * levelF(UpgradeId::Thruster));
    if (const u32 blast = level(UpgradeId::HyperBlast); blast > 0) {
        s.BlastRadius = 110.0f + 40.0f * static_cast<f32>(blast);
        s.BlastDamage = static_cast<i32>(1 + blast);
    }
    s.RearShots = level(UpgradeId::RearGun);
    s.BulletSpeedScale = 1.0f + 0.15f * levelF(UpgradeId::LongRange);
    s.BulletLifetimeScale =
        1.0f + 0.4f * levelF(UpgradeId::LongRange) + (ship == ShipType::Lancer ? 0.3f : 0.0f);
    s.PickupRadius = 60.0f + 90.0f * levelF(UpgradeId::Magnet);
    return s;
}

} // namespace Asteroids::Rogue
