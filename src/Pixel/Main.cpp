// ROCK BLASTER ROGUE, the pixel version: the roguelike (RogueGame: runs of 3 sectors, upgrades,
// bosses, a hangar between runs) on the shared AsteroidsApp (controls, menus, sounds, particle
// effects), drawn with pixel-art sprites from assets/pixel/atlas.png over a twinkling starfield.
// The HUD and screens keep the vector font; this file adds their icons.

#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "AsteroidsApp.h"
#include "GameInfo.h"
#include "Meta.h"
#include "Playfield.h"
#include "Random.h"
#include "RogueGame.h"

namespace {

using namespace Asteroids;
using namespace Asteroids::Rogue;
using Emerald::Sprite;
using Emerald::SpriteOptions;

// How the sprites map onto the game's objects.
namespace Look {
constexpr f32 kShipScale = 0.5f;        // 38 x 80 sprite -> about 19 x 40 on screen
constexpr f32 kShipPivotY = 35.0f;      // sprite row at the ship's position (nose is 17 above)
constexpr f32 kShipHullHeight = 67.0f;  // sprite rows above the engine flames (all ships)
constexpr f32 kAsteroidDiameter = 2.1f; // sprite size = collision radius * this
constexpr f32 kSaucerWidth = 2.55f;     // sprite width = collision radius * this
constexpr f32 kMothershipWidth = 4.0f;  // boss sprites, relative to their collision radius
constexpr f32 kStationWidth = 2.5f;
constexpr f32 kHudIconScale = 2.0f; // 16 x 16 upgrade icons
constexpr f32 kCardIconScale = 4.0f;
constexpr u32 kStarCount = 160;
const Vec4 kPlayerBulletColor{1.0f, 0.95f, 0.6f, 1.0f};
const Vec4 kSaucerBulletColor{1.0f, 0.3f, 0.45f, 1.0f};
const Vec4 kBossBulletColor{1.0f, 0.55f, 0.2f, 1.0f};
const Vec4 kFlash{2.2f, 2.2f, 2.2f, 1.0f}; // tint right after a hit (brighter than the sprite)
} // namespace Look

constexpr const char* kMetaFile = "meta.txt"; // hangar progress, in the per-user folder

// (Tint is a Vec3 plus Brightness rather than a Vec4: Vec4 is 16-byte aligned, which would pad
// this struct and trigger MSVC warning C4324.)
struct Star {
    Vec2 Position;
    f32 Size = 1.0f;
    Emerald::Vec3 Tint{1.0f, 1.0f, 1.0f};
    f32 Brightness = 1.0f;
    f32 TwinkleSpeed = 1.0f;
    f32 TwinklePhase = 0.0f;
};

class PixelRogue final : public AsteroidsApp {
public:
    PixelRogue(const Emerald::ApplicationSpec& spec, const GameInfo::Edition& edition,
               Options options, Settings settings, std::unique_ptr<RogueGame> game)
        : AsteroidsApp(spec, edition, std::move(options), std::move(settings), "highscores.txt",
                       std::move(game))
    {
    }

    [[nodiscard]] const RogueGame& GetGame() const
    {
        return static_cast<const RogueGame&>(GetMode());
    }
    [[nodiscard]] RogueGame& GetGame() { return static_cast<RogueGame&>(GetMode()); }

    // Hangar progress lives next to the high scores, in the per-user folder.
    void LoadMeta()
    {
        m_MetaFile = GetUserFile(kMetaFile);
        if (!m_MetaFile.empty())
            GetGame().SetMeta(MetaProgress::Load(m_MetaFile));
    }

protected:
    void OnLoadAssets() override
    {
        SDL_GPUDevice* device = GetRenderer().GetDevice();
        const std::filesystem::path folder = Emerald::Paths::GetBasePath() / "assets" / "pixel";
        // Nearest filtering (the default): crisp pixels at any scale.
        m_Atlas = Emerald::TextureAtlas::Load(device, folder / "atlas.png", folder / "atlas.json");

        // A 1 x 1 white texture: tinted and scaled, it draws stars, bullets and the dimming.
        Emerald::Image white;
        white.Width = 1;
        white.Height = 1;
        white.Pixels = {255, 255, 255, 255};
        m_White = Emerald::Texture::Create(device, white);
        if (m_White)
            m_WhiteSprite = Emerald::Sprite::FromTexture(*m_White);

        if (m_Atlas) {
            LoadSprites();
        } else {
            EM_ERROR("Pixel sprites missing (expected {}); drawing outlines instead",
                     folder.string());
        }
        MakeStars();
    }

    void OnGameStepped() override
    {
        if (GetGame().ConsumeMetaChanged() && !m_MetaFile.empty())
            GetGame().GetMeta().Save(m_MetaFile);
    }

    void DrawWorld(Emerald::Renderer2D& r) override
    {
        const RogueGame& game = GetGame();
        DrawStars(r, game.GetTime());
        if (!m_Atlas) {
            DrawOutlines(r);
            return;
        }
        // Dimmed behind the screens (results, hangar, picker), so the text stays readable.
        const bool dim = game.IsGameOver() && !game.IsOnTitle();
        const Vec4 tint = dim ? Vec4(0.6f, 0.6f, 0.6f, 0.45f) : Vec4(1.0f);

        for (const Pickup& p : game.GetPickups()) {
            // Scrap bits blink faster as they are about to vanish.
            if (p.TimeLeft < 2.0f && std::fmod(p.TimeLeft * 8.0f, 2.0f) < 0.7f)
                continue;
            r.DrawSprite(m_Scrap, p.Position,
                         {.Scale = Vec2(2.0f), .Rotation = game.GetTime() * 2.0f});
        }

        for (const Rock& rock : game.GetRocks()) {
            const Asteroid& body = rock.Body;
            const Sprite& sprite = RockSprite(rock);
            const f32 scale = body.Radius * Look::kAsteroidDiameter /
                              Emerald::Max(sprite.Region.Size.x, sprite.Region.Size.y);
            const Vec4 rockTint = rock.Flash > 0.0f ? Look::kFlash : tint;
            ForEachWrappedCopy(body.Position, body.Radius * 1.2f, [&](const Vec2& p) {
                r.DrawSprite(sprite, p,
                             {.Scale = Vec2(scale), .Rotation = body.Angle, .Tint = rockTint});
            });
        }

        if (const std::optional<Boss>& boss = game.GetBoss())
            DrawBoss(r, *boss, game.GetTime(), tint);

        for (const Enemy& enemy : game.GetEnemies()) {
            const Saucer& saucer = enemy.Body;
            const f32 scale = saucer.Radius() * Look::kSaucerWidth / m_Saucer.Region.Size.x;
            const f32 wobble = 0.08f * std::sin(game.GetTime() * 5.0f);
            // Escorts (from the mothership) are reddish; tougher saucers flash when hit.
            const Vec4 saucerTint = enemy.Flash > 0.0f ? Look::kFlash
                                    : enemy.Escort     ? Vec4(1.0f, 0.7f, 0.7f, 1.0f)
                                                       : tint;
            r.DrawSprite(m_Saucer, saucer.Position,
                         {.Scale = Vec2(scale), .Rotation = wobble, .Tint = saucerTint});
        }

        if (game.IsShipVisible())
            DrawShip(r, game);

        if (!m_White)
            return;
        for (const Shot& shot : game.GetShots()) {
            if (shot.Missile) {
                const f32 angle = std::atan2(shot.Velocity.y, shot.Velocity.x);
                r.DrawSprite(m_Missile, shot.Position,
                             {.Scale = Vec2(2.0f), .Rotation = angle + Emerald::HalfPi});
            } else {
                // Piercing shots are a little bigger and bluer.
                const bool piercing = shot.PierceLeft > 0;
                DrawSquare(r, shot.Position, piercing ? 4.0f : 3.0f,
                           piercing ? Vec4(0.7f, 0.95f, 1.0f, 1.0f) : Look::kPlayerBulletColor);
            }
        }
        for (const EnemyShot& shot : game.GetEnemyShots()) {
            // Boss patterns (non-wrapping) are big orange orbs, saucer shots small red squares.
            if (shot.Wraps) {
                DrawSquare(r, shot.Position, 3.0f, Look::kSaucerBulletColor);
            } else {
                DrawSquare(r, shot.Position, 7.0f, {1.0f, 0.4f, 0.15f, 0.35f});
                DrawSquare(r, shot.Position, 4.0f, Look::kBossBulletColor);
            }
        }
    }

    [[nodiscard]] Emerald::ParticleDrawOptions GetParticleDrawOptions() const override
    {
        if (!m_White)
            return {};
        return {.Sprite = &m_WhiteSprite, .SizeScale = 0.45f};
    }

    void DrawHud(Emerald::Renderer2D& r) override
    {
        const RogueGame& game = GetGame();
        // A dark veil behind the full-screen screens.
        if (m_White && (game.IsPickingUpgrade() || game.IsInHangar() || game.IsRunOver()))
            r.DrawSprite(*m_White, kPlayfieldCenter,
                         {.Size = kPlayfieldSize, .Tint = {0.0f, 0.0f, 0.04f, 0.55f}});
        game.DrawHud(r);
        if (!m_Atlas)
            return;

        // Collected upgrades along the bottom while playing.
        if (game.IsPlaying() || game.IsPickingUpgrade()) {
            usize slot = 0;
            for (usize i = 0; i < kUpgradeCount; ++i) {
                if (game.GetUpgrades().Level(static_cast<UpgradeId>(i)) == 0)
                    continue;
                r.DrawSprite(m_Icons[i], RogueGame::GetHudIconPosition(slot++),
                             {.Scale = Vec2(Look::kHudIconScale)});
            }
        }
        if (game.IsPickingUpgrade()) {
            const std::vector<UpgradeId>& offers = game.GetOffers();
            for (usize i = 0; i < offers.size(); ++i) {
                // The chosen card's icon bobs a little.
                const bool selected = i == game.GetOfferCursor();
                const Vec2 bob{0.0f, selected ? 3.0f * std::sin(game.GetTime() * 5.0f) : 0.0f};
                const Vec2 position =
                    RogueGame::GetOfferIconPosition(i + (3 - offers.size()) / 2) + bob;
                r.DrawSprite(m_Icons[static_cast<usize>(offers[i])], position,
                             {.Scale = Vec2(Look::kCardIconScale),
                              .Tint = selected ? Vec4(1.0f) : Vec4(0.7f, 0.7f, 0.75f, 1.0f)});
            }
        }
        if (game.IsInHangar()) {
            const std::vector<RogueGame::HangarRow>& rows = RogueGame::GetHangarRows();
            for (usize i = 0; i < rows.size(); ++i) {
                const RogueGame::HangarRow& row = rows[i];
                const Vec2 position = RogueGame::GetHangarIconPosition(i);
                if (row.IsShip) {
                    const bool owned = game.GetMeta().HasShip(static_cast<ShipType>(row.Index));
                    r.DrawSprite(m_ShipsNoFlame[row.Index], position,
                                 {.Scale = Vec2(0.45f),
                                  .Tint = owned ? Vec4(1.0f) : Vec4(0.35f, 0.35f, 0.4f, 1.0f)});
                } else if (row.Index < kUpgradeCount) {
                    const bool owned = game.GetMeta().IsInPool(static_cast<UpgradeId>(row.Index));
                    r.DrawSprite(m_Icons[row.Index], position,
                                 {.Scale = Vec2(Look::kHudIconScale),
                                  .Tint = owned ? Vec4(1.0f) : Vec4(0.45f, 0.45f, 0.5f, 1.0f)});
                }
            }
        }
    }

private:
    void LoadSprites()
    {
        const Emerald::TextureAtlas& atlas = *m_Atlas;
        const char* ships[kShipCount] = {"ship", "ship_bulwark", "ship_wasp", "ship_lancer"};
        for (usize i = 0; i < kShipCount; ++i) {
            m_Ships[i] = atlas.Get(ships[i]);
            m_ShipsNoFlame[i] =
                m_Ships[i].Crop({0.0f, 0.0f}, {m_Ships[i].Region.Size.x, Look::kShipHullHeight});
        }
        m_Saucer = atlas.Get("enemy");
        const char* sizes[3] = {"large", "medium", "small"};
        for (usize size = 0; size < 3; ++size) {
            for (usize look = 0; look < 2; ++look)
                m_Rocks[0][size][look] = atlas.Get("asteroid_" + std::string(sizes[size]) + "_" +
                                                   std::to_string(look + 1));
            const char* kinds[3] = {"explosive", "metal", "splitter"}; // RockKind order
            for (usize kind = 0; kind < 3; ++kind) {
                const Sprite sprite =
                    atlas.Get("rock_" + std::string(kinds[kind]) + "_" + sizes[size]);
                m_Rocks[kind + 1][size] = {sprite, sprite};
            }
        }
        m_Bosses = {atlas.Get("boss_monolith"), atlas.Get("boss_mothership"),
                    atlas.Get("boss_station")};
        for (usize i = 0; i < kUpgradeCount; ++i)
            m_Icons[i] =
                atlas.Get("upgrade_" + std::string(GetUpgradeInfo(static_cast<UpgradeId>(i)).Key));
        m_Scrap = atlas.Get("scrap");
        m_Missile = atlas.Get("missile");
    }

    [[nodiscard]] const Sprite& RockSprite(const Rock& rock) const
    {
        // Plain rocks have two looks; the spin direction picks one (stable, no extra randomness).
        return m_Rocks[static_cast<usize>(rock.Kind)][static_cast<usize>(rock.Body.Size)]
                      [rock.Body.Spin >= 0.0f ? 0 : 1];
    }

    void DrawBoss(Emerald::Renderer2D& r, const Boss& boss, f32 time, const Vec4& tint)
    {
        const Sprite& sprite = m_Bosses[static_cast<usize>(boss.Kind)];
        const Vec2 size = sprite.Region.Size;
        const Vec4 bossTint = boss.Flash > 0.0f ? Look::kFlash : tint;
        switch (boss.Kind) {
        case BossKind::GiantRock: {
            const f32 scale = boss.Radius * Look::kAsteroidDiameter / Emerald::Max(size.x, size.y);
            const SpriteOptions options{
                .Scale = Vec2(scale), .Rotation = boss.Angle, .Tint = bossTint};
            // (No wrapped copies while it flies in from above the field.)
            if (boss.Entering)
                r.DrawSprite(sprite, boss.Position, options);
            else
                ForEachWrappedCopy(boss.Position, boss.Radius * 1.2f,
                                   [&](const Vec2& p) { r.DrawSprite(sprite, p, options); });
            break;
        }
        case BossKind::Mothership:
            r.DrawSprite(sprite, boss.Position,
                         {.Scale = Vec2(boss.Radius * Look::kMothershipWidth / size.x),
                          .Rotation = 0.04f * std::sin(time * 1.3f),
                          .Tint = bossTint});
            break;
        case BossKind::Station:
            r.DrawSprite(sprite, boss.Position,
                         {.Scale = Vec2(boss.Radius * Look::kStationWidth / size.x),
                          .Rotation = boss.Angle,
                          .Tint = bossTint});
            break;
        }
    }

    void DrawShip(Emerald::Renderer2D& r, const RogueGame& game)
    {
        const Ship& ship = game.GetShip();
        const usize type = static_cast<usize>(game.GetShipType());
        // The flames are drawn in: shown only while thrusting (flickering with the game's flame).
        const bool flame = game.GetFlameLength() > 0.0f;
        const Sprite& sprite = flame ? m_Ships[type] : m_ShipsNoFlame[type];
        const SpriteOptions options{
            .Scale = Vec2(Look::kShipScale),
            .Rotation = ship.Angle + Emerald::HalfPi, // the sprites point up (-HalfPi)
            .Origin = Vec2(0.5f, Look::kShipPivotY / sprite.Region.Size.y),
        };
        ForEachWrappedCopy(ship.Position, 24.0f,
                           [&](const Vec2& p) { r.DrawSprite(sprite, p, options); });
        // A faint bubble while the shield is up, bright for a moment when it's hit.
        if (game.GetShield() > 0 || game.GetShieldGlow() > 0.0f) {
            const f32 pulse = 0.18f + 0.06f * std::sin(game.GetTime() * 4.0f);
            const f32 alpha =
                Emerald::Max(game.GetShield() > 0 ? pulse : 0.0f, game.GetShieldGlow());
            r.DrawCircle(ship.Position, 22.0f, {0.5f, 0.9f, 1.0f, alpha}, 24);
        }
    }

    // Fallback without sprites: outlines, as in the vector version.
    void DrawOutlines(Emerald::Renderer2D& r)
    {
        const RogueGame& game = GetGame();
        const Vec4 white(1.0f);
        for (const Rock& rock : game.GetRocks())
            rock.Body.Draw(r, rock.Body.Position, white);
        for (const Shot& shot : game.GetShots())
            r.DrawCircle(shot.Position, 1.5f, white, 4);
        for (const EnemyShot& shot : game.GetEnemyShots())
            r.DrawCircle(shot.Position, 1.5f, white, 4);
        for (const Enemy& enemy : game.GetEnemies())
            enemy.Body.Draw(r, white);
        if (const std::optional<Boss>& boss = game.GetBoss())
            r.DrawCircle(boss->Position, boss->Radius, white, 40);
        if (game.IsShipVisible())
            game.GetShip().Draw(r, game.GetShip().Position, white, game.GetFlameLength());
    }

    void DrawSquare(Emerald::Renderer2D& r, const Vec2& center, f32 size, const Vec4& color)
    {
        r.DrawSprite(*m_White, center, {.Size = Vec2(size), .Tint = color, .PixelSnap = true});
    }

    // A fixed random sky (same every run): mostly faint white, a few bluish or warm stars.
    void MakeStars()
    {
        Random random(1979);
        m_Stars.clear();
        for (u32 i = 0; i < Look::kStarCount; ++i) {
            Star star;
            star.Position = {random.Float(0.0f, kPlayfieldSize.x),
                             random.Float(0.0f, kPlayfieldSize.y)};
            star.Size = random.Chance(0.15f) ? 2.0f : 1.0f;
            const f32 brightness = random.Float(0.25f, 0.8f);
            const f32 hue = random.Float(0.0f, 1.0f);
            star.Tint = hue < 0.15f   ? Emerald::Vec3(0.7f, 0.8f, 1.0f)  // blue
                        : hue < 0.25f ? Emerald::Vec3(1.0f, 0.9f, 0.7f)  // warm
                                      : Emerald::Vec3(1.0f, 1.0f, 1.0f); // white
            star.Brightness = brightness;
            star.TwinkleSpeed = random.Float(0.5f, 2.5f);
            star.TwinklePhase = random.Float(0.0f, Emerald::TwoPi);
            m_Stars.push_back(star);
        }
    }

    void DrawStars(Emerald::Renderer2D& r, f32 time)
    {
        if (!m_White)
            return;
        for (const Star& star : m_Stars) {
            const f32 twinkle =
                0.7f + 0.3f * std::sin(time * star.TwinkleSpeed + star.TwinklePhase);
            const Vec4 color{star.Tint.x, star.Tint.y, star.Tint.z, star.Brightness * twinkle};
            DrawSquare(r, star.Position, star.Size, color);
        }
    }

    std::filesystem::path m_MetaFile; // empty = can't save
    std::optional<Emerald::TextureAtlas> m_Atlas;
    std::optional<Emerald::Texture> m_White;
    Sprite m_WhiteSprite; // all of m_White
    std::array<Sprite, kShipCount> m_Ships{};
    std::array<Sprite, kShipCount> m_ShipsNoFlame{};
    Sprite m_Saucer;
    // [RockKind][AsteroidSize][look]
    std::array<std::array<std::array<Sprite, 2>, 3>, static_cast<usize>(RockKind::Count)> m_Rocks{};
    std::array<Sprite, 3> m_Bosses{}; // BossKind order
    std::array<Sprite, kUpgradeCount> m_Icons{};
    Sprite m_Scrap;
    Sprite m_Missile;
    std::vector<Star> m_Stars;
};

} // namespace

int main(int argc, char** argv)
{
    Options options = ParseOptions(argc, argv);
    const GameInfo::Edition& edition = GameInfo::kPixel;
    const Settings settings = Settings::Load(Settings::DefaultPath(edition.FileName));
    Emerald::ApplicationSpec spec = MakeSpec(options, settings, edition);
    spec.ShaderFormats = EMERALD_SHADER_FORMATS; // formats generated by emerald_add_shaders()

    const u32 seed = MakeSeed(options);
    PixelRogue app(spec, edition, std::move(options), settings, std::make_unique<RogueGame>(seed));
    app.LoadMeta(); // before Run, so a --screen that starts a run already uses the chosen ship
    return app.Run();
}
