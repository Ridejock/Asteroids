#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <Emerald/Core/Defines.h>

#include "Upgrades.h"

namespace Asteroids::Rogue {

// How a run went, for the scrap reward.
struct RunResult {
    u32 Score = 0;
    u32 WavesCleared = 0; // bosses count as waves
    u32 BossesDefeated = 0;
    u32 ScrapCollected = 0; // bits picked up during the run
    bool Victory = false;   // all three sectors cleared
};
// Scrap earned by a run: what was picked up plus bonuses for progress.
[[nodiscard]] u32 ScrapForRun(const RunResult& run);

// What carries over between runs: scrap, the ships bought, the upgrades added to the pool, and
// the chosen ship. Saved as "key=value" lines (unknown keys and bad values are ignored, so an
// old or damaged file keeps whatever still makes sense):
//
//   scrap=120
//   runs=4
//   best=2
//   ships=striker,wasp
//   upgrades=homing
//   ship=wasp
struct MetaProgress {
    u32 Scrap = 0;
    u32 Runs = 0;                               // runs played
    u32 BestSector = 0;                         // deepest sector reached (0 = none yet)
    std::array<bool, kShipCount> Ships{true};   // owned (the Striker always is)
    std::array<bool, kUpgradeCount> Upgrades{}; // bought hangar upgrades (free ones: always)
    ShipType Selected = ShipType::Striker;

    [[nodiscard]] bool HasShip(ShipType type) const { return Ships[static_cast<usize>(type)]; }
    [[nodiscard]] bool IsInPool(UpgradeId id) const;
    // Everything a run may offer: the free upgrades plus the bought ones.
    [[nodiscard]] std::vector<UpgradeId> UpgradePool() const;
    // Spend scrap; false (nothing changes) if already owned or too expensive.
    bool BuyShip(ShipType type);
    bool BuyUpgrade(UpgradeId id);
    // Selects an owned ship (false if it isn't owned).
    bool Select(ShipType type);
    // Books a finished run: its scrap, the run count and the best sector.
    void AddRun(u32 scrap, u32 sectorReached);

    [[nodiscard]] std::string Serialize() const;
    [[nodiscard]] static MetaProgress Parse(std::string_view text);
    // A missing file is a fresh start; both log what happened.
    [[nodiscard]] static MetaProgress Load(const std::filesystem::path& file);
    bool Save(const std::filesystem::path& file) const;
};

} // namespace Asteroids::Rogue
