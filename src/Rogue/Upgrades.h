#pragma once

#include <array>
#include <string_view>
#include <vector>

#include <Emerald/Core/Defines.h>

#include "Random.h"

namespace Asteroids::Rogue {

// Everything the ship can pick up between waves. Levels stack up to UpgradeInfo::MaxLevel.
enum class UpgradeId : u8 {
    SpreadShot, // more shots per volley, fanned out
    Piercing,   // shots fly through rocks
    RapidFire,  // shorter cooldown, more shots on screen
    Shield,     // absorbs a hit, recharges every wave
    Thruster,   // accelerates, turns and tops out faster
    HyperBlast, // hyperspace damages everything near both ends of the jump
    Homing,     // a homing missile every few seconds (hangar unlock)
    RearGun,    // also fires backwards (hangar unlock)
    LongRange,  // faster, longer-lived shots (hangar unlock)
    Magnet,     // pulls scrap bits in from further away (hangar unlock)
    Count
};
inline constexpr usize kUpgradeCount = static_cast<usize>(UpgradeId::Count);

struct UpgradeInfo {
    std::string_view Name;        // upper case, for the vector font
    std::string_view Description; // one short line
    std::string_view Key;         // for the save file and sprite names ("upgrade_<key>")
    u8 MaxLevel = 1;
    u32 UnlockCost = 0; // scrap to add it to the pool in the hangar (0 = in it from the start)
};
[[nodiscard]] const UpgradeInfo& GetUpgradeInfo(UpgradeId id);

// The upgrades a run has collected: a level per upgrade.
class UpgradeSet {
public:
    [[nodiscard]] u8 Level(UpgradeId id) const { return m_Levels[static_cast<usize>(id)]; }
    [[nodiscard]] bool IsMaxed(UpgradeId id) const
    {
        return Level(id) >= GetUpgradeInfo(id).MaxLevel;
    }
    // One more level; false (and no change) if it's already maxed.
    bool Add(UpgradeId id);
    // Sum of all levels.
    [[nodiscard]] u32 Total() const;

private:
    std::array<u8, kUpgradeCount> m_Levels{};
};

// Up to `count` different, not yet maxed upgrades from `pool`, picked at random (fewer if the
// pool runs dry; empty once everything is maxed).
[[nodiscard]] std::vector<UpgradeId> DrawOffers(Random& random, const UpgradeSet& owned,
                                                const std::vector<UpgradeId>& pool, usize count);

// The ships that can start a run (the first is free, the others are bought in the hangar).
enum class ShipType : u8 { Striker, Bulwark, Wasp, Lancer, Count };
inline constexpr usize kShipCount = static_cast<usize>(ShipType::Count);

struct ShipInfo {
    std::string_view Name;
    std::string_view Description;
    std::string_view Key;     // save file and sprite name ("ship" is the Striker's sprite)
    u32 Cost = 0;             // scrap in the hangar
    f32 ThrustScale = 1.0f;   // acceleration, top speed and turning relative to the classic ship
    f32 CooldownScale = 1.0f; // seconds between volleys relative to the base
    u32 ExtraShots = 0;       // more shots on screen at once
    UpgradeId Starting = UpgradeId::Count; // the run begins with this upgrade (Count = none)
};
[[nodiscard]] const ShipInfo& GetShipInfo(ShipType type);

// What a ship with a set of upgrades can do: the rules read only this.
struct CombatStats {
    u32 ShotsPerVolley = 1;
    f32 SpreadAngle = 0.0f;     // radians between neighbouring shots of a volley
    u32 Pierce = 0;             // rocks a shot passes through before it is used up
    f32 FireCooldown = 0.0f;    // seconds between volleys
    u32 MaxVolleys = 4;         // volleys on screen at once (the arcade allows 4 shots)
    u32 ShieldCharges = 0;      // hits absorbed per wave
    f32 MissileInterval = 0.0f; // seconds between homing missiles (0 = none)
    f32 ThrustScale = 1.0f;     // ship acceleration / top speed / turning multiplier
    f32 BlastRadius = 0.0f;     // hyperspace blast (0 = none)
    i32 BlastDamage = 0;
    u32 RearShots = 0; // shots fired backwards with every volley
    f32 BulletSpeedScale = 1.0f;
    f32 BulletLifetimeScale = 1.0f;
    f32 PickupRadius = 60.0f; // scrap bits closer than this fly to the ship
};
[[nodiscard]] CombatStats ComputeStats(ShipType ship, const UpgradeSet& upgrades);

} // namespace Asteroids::Rogue
