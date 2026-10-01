// The vector version (the one that is released): everything drawn with lines, with a soft glow
// like an old vector monitor.
// All the gameplay, sound and controls live in the shared AsteroidsApp; this file only draws the
// world.

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "AsteroidsApp.h"
#include "Game.h"
#include "GameInfo.h"
#include "Playfield.h"
#include "Text.h"

namespace {

using namespace Asteroids;

const Vec4 kShipColor{0.95f, 0.97f, 1.0f, 1.0f};
const Vec4 kAsteroidColor{0.78f, 0.83f, 0.9f, 1.0f};
const Vec4 kBulletColor{1.0f, 1.0f, 1.0f, 1.0f};
const Vec4 kSaucerColor{0.95f, 0.97f, 1.0f, 1.0f};

// Glow: each outline is first drawn in two rings of faint, bluish copies around its position (a
// close brighter one and a wider fainter one), then once sharp on top. Cheap (just more lines)
// and it reads like the bloom of a vector monitor.
constexpr i32 kGlowCopies = 8; // per ring
struct GlowRing {
    f32 Radius; // playfield units
    f32 Alpha;  // times the object's own alpha
};
constexpr GlowRing kGlowRings[] = {{1.3f, 0.22f}, {3.2f, 0.07f}};
const Vec4 kGlowTint{0.55f, 0.8f, 1.0f, 1.0f};

// Share Tech Mono (SIL Open Font License, assets/fonts/): monospaced, so its letters sit evenly
// on the line font's grid.
constexpr const char* kFontFile = "ShareTechMono-Regular.ttf";

// Calls draw(offset, color) for the glow copies and then for the sharp line itself.
template <typename DrawFn> void DrawGlowing(const Vec4& color, DrawFn&& draw)
{
    for (const GlowRing& ring : kGlowRings) {
        const Vec4 glow{kGlowTint.x, kGlowTint.y, kGlowTint.z, color.w * ring.Alpha};
        for (i32 i = 0; i < kGlowCopies; ++i) {
            const f32 angle = Emerald::TwoPi * static_cast<f32>(i) / static_cast<f32>(kGlowCopies);
            draw(Vec2(std::cos(angle), std::sin(angle)) * ring.Radius, glow);
        }
    }
    draw(Vec2(0.0f, 0.0f), color);
}

class VectorAsteroids final : public AsteroidsApp {
public:
    using AsteroidsApp::AsteroidsApp;
    ~VectorAsteroids() override { Text::SetStyle({}); } // before m_Fonts goes

    [[nodiscard]] const Game& GetGame() const { return static_cast<const Game&>(GetMode()); }

protected:
    [[nodiscard]] bool HasCrtOption() const override { return true; }

    // Text: a TTF font, smooth (Linear) and additive with a faint halo so it glows like the
    // lines, spaced on the line font's grid (layouts and the arcade look stay as they were).
    // Baked at several sizes: without mipmaps a bake should not be shrunk much, and the big one
    // keeps the title sharp.
    void OnLoadAssets() override
    {
        SDL_GPUDevice* device = GetRenderer().GetDevice();
        const Emerald::FontOptions options{.Ranges = {Text::kCapitalsAndSymbols}, .Oversample = 2};
        const f32 small[] = {20.0f, 32.0f, 48.0f};
        const f32 large[] = {80.0f, 140.0f}; // big enough not to need oversampling
        m_Fonts = Text::LoadFonts(device, kFontFile, small, options);
        std::vector<Emerald::Font> big =
            Text::LoadFonts(device, kFontFile, large, {.Ranges = options.Ranges, .Oversample = 1});
        if (m_Fonts.empty() || big.empty()) {
            m_Fonts.clear(); // keep the line font
            return;
        }
        for (Emerald::Font& font : big)
            m_Fonts.push_back(std::move(font));
        Text::SetStyle(
            {.Fonts = Text::Pointers(m_Fonts), .Monospace = true, .Additive = true, .Halo = 0.16f});
    }

    void DrawWorld(Emerald::Renderer2D& r) override
    {
        const Game& game = GetGame();

        // Once the game is over the rocks keep drifting, dimmed so the text stays readable.
        Vec4 asteroidColor = kAsteroidColor;
        if (game.IsGameOver())
            asteroidColor.w = 0.35f;
        for (const Asteroid& asteroid : game.GetAsteroids()) {
            ForEachWrappedCopy(asteroid.Position, asteroid.Radius * 1.2f, [&](const Vec2& p) {
                DrawGlowing(asteroidColor, [&](const Vec2& offset, const Vec4& color) {
                    asteroid.Draw(r, p + offset, color);
                });
            });
        }

        for (const auto& bullets : {game.GetBullets(), game.GetSaucerBullets()}) {
            for (const Bullet& bullet : bullets) {
                DrawGlowing(kBulletColor, [&](const Vec2& offset, const Vec4& color) {
                    r.DrawCircle(bullet.Position + offset, 1.5f, color, 4);
                });
            }
        }
        if (game.GetSaucer()) {
            DrawGlowing(kSaucerColor, [&](const Vec2& offset, const Vec4& color) {
                Saucer moved = *game.GetSaucer(); // Saucer::Draw draws at its own position
                moved.Position += offset;
                moved.Draw(r, color);
            });
        }

        for (const Particle& p : game.GetParticles()) {
            const f32 fade = p.TimeLeft / p.Lifetime; // 1 -> 0
            const Vec4 color{1.0f, 1.0f, 1.0f, fade};
            if (p.Length > 0.0f) {
                const Vec2 half = Vec2(std::cos(p.Angle), std::sin(p.Angle)) * (0.5f * p.Length);
                r.DrawLine(p.Position - half, p.Position + half, color);
            } else {
                r.DrawCircle(p.Position, 1.0f, color, 4);
            }
        }

        if (game.IsShipVisible()) {
            const Ship& ship = game.GetShip();
            ForEachWrappedCopy(ship.Position, 20.0f, [&](const Vec2& p) {
                DrawGlowing(kShipColor, [&](const Vec2& offset, const Vec4& color) {
                    ship.Draw(r, p + offset, color, game.GetFlameLength());
                });
            });
        }
    }

private:
    std::vector<Emerald::Font> m_Fonts; // Text's style points at these
};

} // namespace

int main(int argc, char** argv)
{
    Options options = ParseOptions(argc, argv);
    const GameInfo::Edition& edition = GameInfo::kVector;
    const Settings settings = Settings::Load(Settings::DefaultPath(edition.FileName));
    Emerald::ApplicationSpec spec = MakeSpec(options, settings, edition);
    spec.ShaderFormats = EMERALD_SHADER_FORMATS;  // formats generated by emerald_add_shaders()
    spec.Args = {argv, static_cast<usize>(argc)}; // engine options, e.g. --gpu vulkan

    auto game = std::make_unique<Game>(MakeSeed(options));
    VectorAsteroids app(spec, edition, std::move(options), settings, "highscores.txt",
                        std::move(game));
    return app.Run();
}
