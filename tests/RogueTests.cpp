// Tests for the roguelike's rules: upgrade stacking and stats, seeded offers and wave plans,
// run progression (3 sectors x 4 waves + bosses, permadeath) and the meta progress save file.
// No window, GPU or audio needed.

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

#include <Emerald/Core/Log.h>

#include "Check.h"
#include "HighScores.h"
#include "Meta.h"
#include "RogueGame.h"
#include "Run.h"
#include "Upgrades.h"

namespace {

using namespace Asteroids;
using namespace Asteroids::Rogue;

void TestUpgradeStacking()
{
    UpgradeSet set;
    Check(set.Total() == 0, "a new run has no upgrades");
    Check(set.Add(UpgradeId::SpreadShot) && set.Add(UpgradeId::SpreadShot),
          "spread shot stacks to level 2");
    Check(!set.Add(UpgradeId::SpreadShot) && set.Level(UpgradeId::SpreadShot) == 2,
          "a maxed upgrade doesn't go further");
    Check(!set.Add(UpgradeId::Count), "'none' is not an upgrade");

    const CombatStats base = ComputeStats(ShipType::Striker, {});
    const CombatStats spread = ComputeStats(ShipType::Striker, set);
    Check(base.ShotsPerVolley == 1 && spread.ShotsPerVolley == 5, "spread: 1 -> 3 -> 5 shots");

    UpgradeSet rapid;
    rapid.Add(UpgradeId::RapidFire);
    const CombatStats one = ComputeStats(ShipType::Striker, rapid);
    rapid.Add(UpgradeId::RapidFire);
    const CombatStats two = ComputeStats(ShipType::Striker, rapid);
    Check(two.FireCooldown < one.FireCooldown && one.FireCooldown < base.FireCooldown,
          "rapid fire levels keep shortening the cooldown");
    Check(two.MaxVolleys == base.MaxVolleys + 4, "rapid fire: two more volleys per level");

    UpgradeSet mixed;
    mixed.Add(UpgradeId::Shield);
    mixed.Add(UpgradeId::Shield);
    mixed.Add(UpgradeId::Piercing);
    mixed.Add(UpgradeId::HyperBlast);
    mixed.Add(UpgradeId::Homing);
    const CombatStats m = ComputeStats(ShipType::Striker, mixed);
    Check(m.ShieldCharges == 2 && m.Pierce == 1, "shield and piercing count their levels");
    Check(m.BlastRadius > 0.0f && m.BlastDamage > 0 && base.BlastRadius == 0.0f,
          "hyper blast only with the upgrade");
    Check(m.MissileInterval > 0.0f && base.MissileInterval == 0.0f, "missiles only with homing");
    Check(ComputeStats(ShipType::Wasp, {}).ThrustScale > base.ThrustScale,
          "the Wasp has the stronger engine");
}

void TestOffers()
{
    std::vector<UpgradeId> pool;
    for (usize i = 0; i < kUpgradeCount; ++i)
        pool.push_back(static_cast<UpgradeId>(i));

    Random a(42);
    Random b(42);
    UpgradeSet owned;
    bool same = true;
    bool valid = true;
    for (i32 round = 0; round < 20; ++round) {
        const std::vector<UpgradeId> x = DrawOffers(a, owned, pool, 3);
        const std::vector<UpgradeId> y = DrawOffers(b, owned, pool, 3);
        same = same && x == y;
        for (usize i = 0; i < x.size(); ++i) {
            valid = valid && !owned.IsMaxed(x[i]);
            for (usize j = i + 1; j < x.size(); ++j)
                valid = valid && x[i] != x[j];
        }
        if (!x.empty())
            owned.Add(x.front());
    }
    Check(same, "the same seed gives the same offers");
    Check(valid, "offers are distinct and never maxed");

    UpgradeSet maxed;
    maxed.Add(UpgradeId::SpreadShot);
    maxed.Add(UpgradeId::SpreadShot);
    const std::vector<UpgradeId> small{UpgradeId::SpreadShot, UpgradeId::Shield};
    const std::vector<UpgradeId> offers = DrawOffers(a, maxed, small, 3);
    Check(offers.size() == 1 && offers[0] == UpgradeId::Shield,
          "a thin pool offers what's left (and skips maxed upgrades)");

    MetaProgress meta;
    const std::vector<UpgradeId> fresh = meta.UpgradePool();
    Check(std::find(fresh.begin(), fresh.end(), UpgradeId::Homing) == fresh.end() &&
              std::find(fresh.begin(), fresh.end(), UpgradeId::SpreadShot) != fresh.end(),
          "homing missiles have to be bought for the pool");
}

void TestStages()
{
    Stage stage;
    u32 count = 1;
    u32 bosses = 0;
    while (const std::optional<Stage> next = NextStage(stage)) {
        bosses += stage.IsBoss() ? 1 : 0;
        Check(next->Index() == stage.Index() + 1, "stages count up by one");
        stage = *next;
        ++count;
    }
    bosses += stage.IsBoss() ? 1 : 0;
    Check(count == kSectors * (kWavesPerSector + 1), "a run is 3 sectors x (4 waves + boss)");
    Check(bosses == 3 && stage.Sector == 3 && stage.IsBoss(), "it ends with the third boss");
    Check(Stage{2, 3}.Label() == "SECTOR 2 - WAVE 3" && Stage{2, 5}.Label() == "SECTOR 2 - BOSS",
          "stage labels");
    Check(BossFor(1) == BossKind::GiantRock && BossFor(2) == BossKind::Mothership &&
              BossFor(3) == BossKind::Station,
          "one boss per sector");

    Random a(7);
    Random b(7);
    const WavePlan first = PlanWave(a, Stage{1, 1});
    Check(first.RockCount == 4 &&
              std::all_of(first.Rocks.begin(), first.Rocks.begin() + first.RockCount,
                          [](RockKind k) { return k == RockKind::Normal; }),
          "the first wave is four plain rocks");
    (void)PlanWave(b, Stage{1, 1});
    bool same = true;
    for (u32 s = 1; s <= kSectors; ++s) {
        for (u32 w = 1; w <= kWavesPerSector; ++w) {
            const WavePlan x = PlanWave(a, Stage{s, w});
            const WavePlan y = PlanWave(b, Stage{s, w});
            same = same && x.RockCount == y.RockCount && x.Rocks == y.Rocks;
        }
    }
    Check(same, "wave plans are repeatable with a seed");
    Random c(1);
    const WavePlan early = PlanWave(c, Stage{1, 4});
    const WavePlan late = PlanWave(c, Stage{3, 4});
    Check(late.RockCount > early.RockCount && late.SaucerHealth > early.SaucerHealth &&
              late.AimError < early.AimError && late.SaucerDelayMax < early.SaucerDelayMax,
          "deeper sectors: more rocks, tougher and more frequent saucers");
    Check(RockHealth(RockKind::Metal, 0) > 1 && RockHealth(RockKind::Normal, 0) == 1,
          "metal rocks take several hits");
}

void TestRunProgression()
{
    RogueGame game(123);
    game.StartGame();
    Check(game.IsPlaying() && game.GetStage() == Stage{} && game.GetAsteroidCount() == 4,
          "a run starts at sector 1, wave 1 with four rocks");
    game.ClearStageForTest();
    Check(game.IsPickingUpgrade() && game.GetOffers().size() == 3,
          "clearing a wave offers three upgrades");
    const UpgradeId picked = game.GetOffers()[0];
    game.ChooseOffer(0);
    Check(game.IsPlaying() && game.GetStage() == (Stage{1, 2}) &&
              game.GetUpgrades().Level(picked) == 1,
          "picking one applies it and starts the next wave");

    // Play through: the seed decides every offer, so two games stay in step.
    RogueGame twin(123);
    twin.StartGame();
    twin.ClearStageForTest();
    twin.ChooseOffer(0);
    bool inStep = true;
    bool sawBoss = false;
    for (i32 i = 0; i < 30 && game.IsPlaying(); ++i) {
        sawBoss = sawBoss || game.GetBoss().has_value();
        game.ClearStageForTest();
        twin.ClearStageForTest();
        inStep = inStep && game.GetOffers() == twin.GetOffers();
        if (game.IsPickingUpgrade()) {
            game.ChooseOffer(1);
            twin.ChooseOffer(1);
        }
    }
    Check(inStep, "same seed, same choices: same offers all run long");
    Check(sawBoss, "boss stages have a boss");
    Check(game.IsRunOver() && game.IsVictory(), "clearing the third boss wins the run");
    Check(game.GetUpgrades().Total() == 14,
          "one upgrade after each of the 14 stages before the last");
    Check(game.GetMeta().Runs == 1 && game.GetMeta().Scrap > 0 && game.GetMeta().BestSector == 3,
          "the run pays scrap into the meta progress");
    Check(game.ConsumeMetaChanged() && !game.ConsumeMetaChanged(), "meta change is reported once");

    // Permadeath: one death ends the run.
    RogueGame dead(5);
    dead.StartGame();
    dead.ForceGameOver(0);
    Check(dead.IsRunOver() && !dead.IsVictory() && dead.GetMeta().Runs == 1, "dying ends the run");

    // Ships start with their upgrade; the shield recharges at the next wave.
    RogueGame bulwark(9);
    MetaProgress meta;
    meta.Scrap = 1000;
    meta.BuyShip(ShipType::Bulwark);
    meta.Select(ShipType::Bulwark);
    bulwark.SetMeta(meta);
    bulwark.StartGame();
    Check(bulwark.GetShipType() == ShipType::Bulwark && bulwark.GetShield() == 1 &&
              bulwark.GetUpgrades().Level(UpgradeId::Shield) == 1,
          "the Bulwark starts with a charged shield");
}

void TestMetaSave()
{
    MetaProgress meta;
    Check(meta.HasShip(ShipType::Striker) && !meta.HasShip(ShipType::Wasp),
          "only the Striker at first");
    Check(!meta.BuyShip(ShipType::Wasp) && meta.Scrap == 0, "can't buy without scrap");
    meta.AddRun(ScrapForRun({.Score = 5000,
                             .WavesCleared = 5,
                             .BossesDefeated = 1,
                             .ScrapCollected = 40,
                             .Victory = false}),
                2);
    Check(meta.Scrap == 40 + 50 + 50 + 20 && meta.Runs == 1 && meta.BestSector == 2,
          "scrap = pickups + 10/wave + 50/boss + score/250");
    meta.Scrap = 1000;
    Check(meta.BuyShip(ShipType::Wasp) && meta.Scrap == 1000 - GetShipInfo(ShipType::Wasp).Cost,
          "buying a ship costs its price");
    Check(!meta.BuyShip(ShipType::Wasp), "a ship is bought only once");
    Check(meta.Select(ShipType::Wasp) && !meta.Select(ShipType::Lancer),
          "only owned ships can be selected");
    Check(meta.BuyUpgrade(UpgradeId::Homing) && meta.IsInPool(UpgradeId::Homing),
          "a bought upgrade joins the pool");

    const MetaProgress back = MetaProgress::Parse(meta.Serialize());
    Check(back.Scrap == meta.Scrap && back.Runs == 1 && back.BestSector == 2 &&
              back.HasShip(ShipType::Wasp) && !back.HasShip(ShipType::Lancer) &&
              back.IsInPool(UpgradeId::Homing) && back.Selected == ShipType::Wasp,
          "serialize / parse round trip");

    const MetaProgress messy =
        MetaProgress::Parse("scrap=12x\r\nruns=3\r\nships=lancer,bogus\nship=wasp\nnonsense\n");
    Check(messy.Scrap == 0 && messy.Runs == 3 && messy.HasShip(ShipType::Lancer) &&
              messy.Selected == ShipType::Striker,
          "bad values are skipped, an unowned selection falls back to the Striker");

    const std::filesystem::path file =
        std::filesystem::temp_directory_path() / "rogue_meta_test.txt";
    Check(meta.Save(file), "saving works");
    const MetaProgress loaded = MetaProgress::Load(file);
    Check(loaded.Serialize() == meta.Serialize(), "load gives back what was saved");
    std::filesystem::remove(file);
    Check(MetaProgress::Load(file).Scrap == 0, "a missing file is a fresh start");
}

void TestHighScoreNotes()
{
    HighScoreTable table;
    table.Insert("ABC", 5000, "S2 W3");
    table.Insert("XYZ", 900);
    Check(table.Serialize() == "ABC 5000 S2 W3\nXYZ 900\n", "the note is a last column");
    const HighScoreTable back = HighScoreTable::Parse(table.Serialize());
    Check(back.GetEntries().size() == 2 && back.GetEntries()[0].Note == "S2 W3" &&
              back.GetEntries()[1].Note.empty(),
          "notes survive a round trip");
}

} // namespace

int main()
{
    Emerald::Log::Init({}); // the game logs as it goes (console only)
    TestUpgradeStacking();
    TestOffers();
    TestStages();
    TestRunProgression();
    TestMetaSave();
    TestHighScoreNotes();
    std::printf("%d failed\n", g_Failures);
    return g_Failures == 0 ? 0 : 1;
}
