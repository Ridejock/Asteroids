// Asteroids, pixel version: the same game as the vector one (shared AsteroidsApp: rules, sounds,
// controls, high scores), drawn with pixel-art sprites from assets/pixel/atlas.png over a
// twinkling starfield. The HUD keeps the vector font.
//
// Gameplay is identical: collision radii are the shared ones, and every sprite is scaled to fit
// its object's radius.

#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "AsteroidsApp.h"
#include "GameInfo.h"
#include "Playfield.h"
#include "Random.h"

namespace {

using namespace Asteroids;
using Emerald::Sprite;
using Emerald::SpriteOptions;

// How the sprites map onto the game's objects.
namespace Look {
constexpr f32 kShipScale = 0.5f;         // 38 x 80 sprite -> about 19 x 40 on screen
constexpr Vec2 kShipPivot{19.0f, 35.0f}; // sprite pixel at the ship's position (nose is 17 above)
constexpr f32 kShipHullHeight = 67.0f;   // sprite rows above the engine flames
constexpr f32 kLifeIconScale = 0.3f;
constexpr f32 kAsteroidDiameter = 2.1f; // sprite size = collision radius * this
constexpr f32 kSaucerWidth = 2.55f;     // sprite width = collision radius * this
constexpr u32 kStarCount = 160;
const Vec4 kPlayerBulletColor{1.0f, 0.95f, 0.6f, 1.0f};
const Vec4 kSaucerBulletColor{1.0f, 0.3f, 0.45f, 1.0f};
} // namespace Look

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

class PixelAsteroids final : public AsteroidsApp {
public:
    using AsteroidsApp::AsteroidsApp;

protected:
    void OnLoadAssets() override
    {
        SDL_GPUDevice* device = GetRenderer().GetDevice();
        const std::filesystem::path folder = Emerald::Paths::GetBasePath() / "assets" / "pixel";
        // Nearest filtering (the default): crisp pixels at any scale.
        m_Atlas = Emerald::TextureAtlas::Load(device, folder / "atlas.png", folder / "atlas.json");

        // A 1 x 1 white texture: tinted and scaled, it draws stars, bullets and debris.
        Emerald::Image white;
        white.Width = 1;
        white.Height = 1;
        white.Pixels = {255, 255, 255, 255};
        m_White = Emerald::Texture::Create(device, white);
        if (m_White)
            m_WhiteSprite = Emerald::Sprite::FromTexture(*m_White);

        if (m_Atlas) {
            m_Ship = m_Atlas->Get("ship");
            m_ShipNoFlame =
                m_Ship.Crop({0.0f, 0.0f}, {m_Ship.Region.Size.x, Look::kShipHullHeight});
            m_Saucer = m_Atlas->Get("enemy");
            const char* names[3][2] = {{"asteroid_large_1", "asteroid_large_2"},
                                       {"asteroid_medium_1", "asteroid_medium_2"},
                                       {"asteroid_small_1", "asteroid_small_2"}};
            for (usize size = 0; size < 3; ++size)
                for (usize look = 0; look < 2; ++look)
                    m_Asteroids[size][look] = m_Atlas->Get(names[size][look]);
        } else {
            EM_ERROR("Pixel sprites missing (expected {}); drawing outlines instead",
                     folder.string());
        }
        MakeStars();
    }

    void DrawWorld(Emerald::Renderer2D& r) override
    {
        const Game& game = GetGame();
        DrawStars(r, game.GetTime());
        if (!m_Atlas) {
            DrawOutlines(r); // sprites missing: still playable
            return;
        }

        // Rocks, dimmed once the game is over (like the vector version) so the text stays
        // readable.
        const Vec4 rockTint = game.IsGameOver() ? Vec4(0.6f, 0.6f, 0.6f, 0.45f) : Vec4(1.0f);
        for (const Asteroid& asteroid : game.GetAsteroids()) {
            // Two looks per size; the spin direction picks one (stable for the rock's lifetime,
            // and no extra random numbers, so the game plays exactly like the vector version).
            const Sprite& sprite =
                m_Asteroids[static_cast<usize>(asteroid.Size)][asteroid.Spin >= 0.0f ? 0 : 1];
            const f32 scale = asteroid.Radius * Look::kAsteroidDiameter /
                              Emerald::Max(sprite.Region.Size.x, sprite.Region.Size.y);
            ForEachWrappedCopy(asteroid.Position, asteroid.Radius * 1.2f, [&](const Vec2& p) {
                r.DrawSprite(sprite, p,
                             {.Scale = Vec2(scale), .Rotation = asteroid.Angle, .Tint = rockTint});
            });
        }

        if (const std::optional<Saucer>& saucer = game.GetSaucer()) {
            const f32 scale = saucer->Radius() * Look::kSaucerWidth / m_Saucer.Region.Size.x;
            // A slight wobble, so it looks like it hovers.
            const f32 wobble = 0.08f * std::sin(game.GetTime() * 5.0f);
            r.DrawSprite(m_Saucer, saucer->Position, {.Scale = Vec2(scale), .Rotation = wobble});
        }

        if (game.IsShipVisible()) {
            const Ship& ship = game.GetShip();
            // The sprite has its flames drawn in: show them only while thrusting (and let them
            // flicker with the game's flame), otherwise crop them off.
            const bool flame = game.GetFlameLength() > 0.0f;
            const Sprite& sprite = flame ? m_Ship : m_ShipNoFlame;
            const SpriteOptions options{
                .Scale = Vec2(Look::kShipScale),
                .Rotation = ship.Angle + Emerald::HalfPi, // the sprite points up (-HalfPi)
                .Origin = Look::kShipPivot / sprite.Region.Size,
            };
            ForEachWrappedCopy(ship.Position, 24.0f,
                               [&](const Vec2& p) { r.DrawSprite(sprite, p, options); });
        }

        // Bullets and explosions on top: small squares of the white texture.
        if (!m_White)
            return;
        for (const Bullet& bullet : game.GetBullets())
            DrawSquare(r, bullet.Position, 3.0f, Look::kPlayerBulletColor);
        for (const Bullet& bullet : game.GetSaucerBullets())
            DrawSquare(r, bullet.Position, 3.0f, Look::kSaucerBulletColor);
        for (const Particle& p : game.GetParticles()) {
            const f32 fade = p.TimeLeft / p.Lifetime; // 1 -> 0
            if (p.Length > 0.0f) {
                // Debris: a spinning strip of hull.
                r.DrawSprite(*m_White, p.Position,
                             {.Size = {p.Length, 2.0f},
                              .Rotation = p.Angle,
                              .Tint = {0.75f, 0.78f, 0.85f, fade}});
            } else {
                // Sparks: from yellow to red as they fade.
                DrawSquare(r, p.Position, 2.0f, {1.0f, 0.4f + 0.5f * fade, 0.2f * fade, fade});
            }
        }
    }

    // Particles as small glowing squares instead of streaks (the effects' sizes are streak
    // lengths, so they are scaled down).
    [[nodiscard]] Emerald::ParticleDrawOptions GetParticleDrawOptions() const override
    {
        if (!m_White)
            return {};
        return {.Sprite = &m_WhiteSprite, .SizeScale = 0.45f};
    }

    void DrawHud(Emerald::Renderer2D& r) override
    {
        const Game& game = GetGame();
        game.DrawHud(r, !m_Atlas); // the vector ship icons only as a fallback
        if (!m_Atlas)
            return;
        for (u32 i = 0; i < game.GetLives(); ++i) {
            r.DrawSprite(m_ShipNoFlame, Game::GetLifeIconPosition(i),
                         {.Scale = Vec2(Look::kLifeIconScale)});
        }
    }

private:
    // Fallback without sprites: the objects' own outlines (as in the vector version).
    void DrawOutlines(Emerald::Renderer2D& r)
    {
        const Game& game = GetGame();
        const Vec4 white(1.0f);
        for (const Asteroid& asteroid : game.GetAsteroids())
            asteroid.Draw(r, asteroid.Position, white);
        for (const Bullet& bullet : game.GetBullets())
            r.DrawCircle(bullet.Position, 1.5f, white, 4);
        for (const Bullet& bullet : game.GetSaucerBullets())
            r.DrawCircle(bullet.Position, 1.5f, white, 4);
        if (game.GetSaucer())
            game.GetSaucer()->Draw(r, white);
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

    std::optional<Emerald::TextureAtlas> m_Atlas;
    std::optional<Emerald::Texture> m_White;
    Emerald::Sprite m_WhiteSprite; // all of m_White
    Sprite m_Ship;
    Sprite m_ShipNoFlame;
    Sprite m_Saucer;
    std::array<std::array<Sprite, 2>, 3> m_Asteroids{}; // [AsteroidSize][look]
    std::vector<Star> m_Stars;
};

} // namespace

int main(int argc, char** argv)
{
    Options options = ParseOptions(argc, argv);
    const Settings settings = Settings::Load(Settings::DefaultPath());
    Emerald::ApplicationSpec spec =
        MakeSpec(options, settings, std::string(GameInfo::kTitle) + " (PIXEL)",
                 std::string(GameInfo::kFileName) + "Pixel.log");
    spec.ShaderFormats = EMERALD_SHADER_FORMATS; // formats generated by emerald_add_shaders()

    PixelAsteroids app(spec, std::move(options), settings, "highscores_pixel.txt");
    return app.Run();
}
