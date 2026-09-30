#include "Run.h"

#include <algorithm>

namespace Asteroids::Rogue {

std::string Stage::Label() const
{
    return "SECTOR " + std::to_string(Sector) +
           (IsBoss() ? std::string(" - BOSS") : " - WAVE " + std::to_string(Wave));
}

std::string Stage::ShortLabel() const
{
    return "S" + std::to_string(Sector) +
           (IsBoss() ? std::string(" BOSS") : " W" + std::to_string(Wave));
}

std::optional<Stage> NextStage(const Stage& stage)
{
    if (!stage.IsBoss())
        return Stage{stage.Sector, stage.Wave + 1};
    if (stage.Sector >= kSectors)
        return std::nullopt;
    return Stage{stage.Sector + 1, 1};
}

i32 RockHealth(RockKind kind, u32 sizeIndex)
{
    // Metal: 4 / 3 / 2 hits; everything else breaks at the first.
    return kind == RockKind::Metal ? 4 - static_cast<i32>(sizeIndex) : 1;
}

BossKind BossFor(u32 sector)
{
    return sector <= 1   ? BossKind::GiantRock
           : sector == 2 ? BossKind::Mothership
                         : BossKind::Station;
}

WavePlan PlanWave(Random& random, const Stage& stage)
{
    WavePlan plan;
    const u32 depth = stage.Sector - 1; // 0..2
    const u32 wave = std::min(stage.Wave, kWavesPerSector);

    // Rocks: 4 in the very first wave, one more per wave and two more per sector. Bosses get
    // two as cover, except the giant rock (it brings its own).
    plan.RockCount =
        stage.IsBoss() ? (stage.Sector == 1 ? 0 : 2) : std::min<u32>(3 + wave + 2 * depth, 12);

    // Special kinds: [explosive, metal, splitter] chances, rising with depth.
    struct Mix {
        f32 Explosive, Metal, Splitter;
    };
    constexpr std::array<Mix, kSectors> kMix{
        {{0.12f, 0.08f, 0.10f}, {0.18f, 0.18f, 0.18f}, {0.20f, 0.25f, 0.22f}}};
    const Mix mix = kMix[depth];
    const f32 ramp = 0.6f + 0.2f * static_cast<f32>(wave - 1); // wave 1: 60% .. wave 4: 120%
    for (u32 i = 0; i < plan.RockCount; ++i) {
        const f32 roll = random.Float(0.0f, 1.0f);
        const f32 e = mix.Explosive * ramp;
        const f32 m = e + mix.Metal * ramp;
        const f32 s = m + mix.Splitter * ramp;
        // The first wave of the run is all plain rocks (learn the controls first).
        plan.Rocks[i] = stage.Index() == 0 ? RockKind::Normal
                        : roll < e         ? RockKind::Explosive
                        : roll < m         ? RockKind::Metal
                        : roll < s         ? RockKind::Splitter
                                           : RockKind::Normal;
    }

    // Saucers: more often, smaller, better aimed and tougher deeper in.
    constexpr std::array<WavePlan, kSectors> kSaucers{{
        {.SaucerDelayMin = 12.0f,
         .SaucerDelayMax = 18.0f,
         .SmallSaucerChance = 0.1f,
         .AimError = 0.4f,
         .SaucerFireScale = 1.0f,
         .SaucerBulletSpeed = 1.0f,
         .SaucerHealth = 1},
        {.SaucerDelayMin = 9.0f,
         .SaucerDelayMax = 14.0f,
         .SmallSaucerChance = 0.35f,
         .AimError = 0.22f,
         .SaucerFireScale = 0.85f,
         .SaucerBulletSpeed = 1.1f,
         .SaucerHealth = 2},
        {.SaucerDelayMin = 6.0f,
         .SaucerDelayMax = 11.0f,
         .SmallSaucerChance = 0.6f,
         .AimError = 0.12f,
         .SaucerFireScale = 0.7f,
         .SaucerBulletSpeed = 1.2f,
         .SaucerHealth = 3},
    }};
    const WavePlan& saucers = kSaucers[depth];
    plan.SaucerDelayMin = saucers.SaucerDelayMin;
    plan.SaucerDelayMax = saucers.SaucerDelayMax;
    plan.SmallSaucerChance = saucers.SmallSaucerChance;
    plan.AimError = saucers.AimError;
    plan.SaucerFireScale = saucers.SaucerFireScale;
    plan.SaucerBulletSpeed = saucers.SaucerBulletSpeed;
    plan.SaucerHealth = saucers.SaucerHealth;
    return plan;
}

} // namespace Asteroids::Rogue
