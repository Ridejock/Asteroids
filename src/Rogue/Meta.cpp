#include "Meta.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>

#include <Emerald/Core/Log.h>

namespace Asteroids::Rogue {

namespace {

// Calls fn(item) for every comma separated item of `list`.
template <typename Fn> void ForEachItem(std::string_view list, Fn&& fn)
{
    while (!list.empty()) {
        const usize comma = std::min(list.find(','), list.size());
        fn(list.substr(0, comma));
        list.remove_prefix(std::min(comma + 1, list.size()));
    }
}

bool ParseU32(std::string_view text, u32& value)
{
    const auto [ptr, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc() && ptr == text.data() + text.size();
}

} // namespace

u32 ScrapForRun(const RunResult& run)
{
    return run.ScrapCollected + 10 * run.WavesCleared + 50 * run.BossesDefeated + run.Score / 250 +
           (run.Victory ? 200 : 0);
}

bool MetaProgress::IsInPool(UpgradeId id) const
{
    return GetUpgradeInfo(id).UnlockCost == 0 || Upgrades[static_cast<usize>(id)];
}

std::vector<UpgradeId> MetaProgress::UpgradePool() const
{
    std::vector<UpgradeId> pool;
    for (usize i = 0; i < kUpgradeCount; ++i)
        if (IsInPool(static_cast<UpgradeId>(i)))
            pool.push_back(static_cast<UpgradeId>(i));
    return pool;
}

bool MetaProgress::BuyShip(ShipType type)
{
    const u32 cost = GetShipInfo(type).Cost;
    if (HasShip(type) || Scrap < cost)
        return false;
    Scrap -= cost;
    Ships[static_cast<usize>(type)] = true;
    return true;
}

bool MetaProgress::BuyUpgrade(UpgradeId id)
{
    const u32 cost = GetUpgradeInfo(id).UnlockCost;
    if (IsInPool(id) || Scrap < cost)
        return false;
    Scrap -= cost;
    Upgrades[static_cast<usize>(id)] = true;
    return true;
}

bool MetaProgress::Select(ShipType type)
{
    if (!HasShip(type))
        return false;
    Selected = type;
    return true;
}

void MetaProgress::AddRun(u32 scrap, u32 sectorReached)
{
    Scrap += scrap;
    ++Runs;
    BestSector = std::max(BestSector, sectorReached);
}

std::string MetaProgress::Serialize() const
{
    std::string ships;
    std::string upgrades;
    for (usize i = 0; i < kShipCount; ++i)
        if (Ships[i] && i != 0)
            ships +=
                (ships.empty() ? "" : ",") + std::string(GetShipInfo(static_cast<ShipType>(i)).Key);
    for (usize i = 0; i < kUpgradeCount; ++i)
        if (Upgrades[i])
            upgrades += (upgrades.empty() ? "" : ",") +
                        std::string(GetUpgradeInfo(static_cast<UpgradeId>(i)).Key);
    return "scrap=" + std::to_string(Scrap) + "\nruns=" + std::to_string(Runs) +
           "\nbest=" + std::to_string(BestSector) + "\nships=" + ships + "\nupgrades=" + upgrades +
           "\nship=" + std::string(GetShipInfo(Selected).Key) + "\n";
}

MetaProgress MetaProgress::Parse(std::string_view text)
{
    const auto findShip = [](std::string_view key) {
        for (usize i = 0; i < kShipCount; ++i)
            if (GetShipInfo(static_cast<ShipType>(i)).Key == key)
                return static_cast<ShipType>(i);
        return ShipType::Count;
    };
    MetaProgress meta;
    ShipType selected = ShipType::Striker;
    while (!text.empty()) {
        const usize end = std::min(text.find('\n'), text.size());
        std::string_view line = text.substr(0, end);
        text.remove_prefix(std::min(end + 1, text.size()));
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        const usize equals = line.find('=');
        if (equals == std::string_view::npos)
            continue;
        const std::string_view key = line.substr(0, equals);
        const std::string_view value = line.substr(equals + 1);
        u32 number = 0;
        if (key == "scrap" && ParseU32(value, number))
            meta.Scrap = number;
        else if (key == "runs" && ParseU32(value, number))
            meta.Runs = number;
        else if (key == "best" && ParseU32(value, number))
            meta.BestSector = number;
        else if (key == "ships")
            ForEachItem(value, [&](std::string_view item) {
                if (const ShipType type = findShip(item); type != ShipType::Count)
                    meta.Ships[static_cast<usize>(type)] = true;
            });
        else if (key == "upgrades")
            ForEachItem(value, [&](std::string_view item) {
                for (usize i = 0; i < kUpgradeCount; ++i)
                    if (GetUpgradeInfo(static_cast<UpgradeId>(i)).Key == item)
                        meta.Upgrades[i] = true;
            });
        else if (key == "ship")
            selected = findShip(value);
    }
    if (selected != ShipType::Count)
        meta.Select(selected); // only if it is owned
    return meta;
}

MetaProgress MetaProgress::Load(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        EM_INFO("No hangar progress yet ({} not found)", file.string());
        return {};
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    MetaProgress meta = Parse(buffer.str());
    EM_INFO("Loaded hangar progress from {}: {} scrap, {} runs", file.string(), meta.Scrap,
            meta.Runs);
    return meta;
}

bool MetaProgress::Save(const std::filesystem::path& file) const
{
    // Via a temporary file, so a crash mid-write keeps the old progress.
    std::filesystem::path temp = file;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << Serialize();
        if (!out) {
            EM_WARN("Could not write hangar progress to {}", temp.string());
            return false;
        }
    }
    std::error_code error;
    std::filesystem::rename(temp, file, error);
    if (error) {
        EM_WARN("Could not save hangar progress to {}: {}", file.string(), error.message());
        return false;
    }
    EM_INFO("Saved hangar progress to {}", file.string());
    return true;
}

} // namespace Asteroids::Rogue
