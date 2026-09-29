// Asteroids: a classic vector arcade game on top of the Emerald engine.
//
// This file connects the Game to the engine: Emerald's Application owns the window, the GPU
// and the main loop, and calls the hooks below. Game logic runs in OnFixedUpdate (120 times per
// second, whatever the frame rate), drawing happens in OnRender2D.

#include <charconv>
#include <cmath>
#include <random>
#include <string>
#include <string_view>
#include <utility>

#include <Emerald/Emerald.h>

#if EMERALD_WITH_IMGUI
#include <imgui.h>
#endif

#include "Game.h"
#include "Playfield.h"

namespace {

using Emerald::Key;
using Emerald::Mat4;
using Emerald::Vec2;

// Command line options (handy for automated runs): --frames N quits after N frames,
// --screenshot out.png saves the last frame, --seed N makes the asteroids repeatable.
struct Options {
    u64 Frames = 0;
    std::string ScreenshotPath;
    u32 Seed = 0; // 0 = random
};

class AsteroidsApp final : public Emerald::Application {
public:
    AsteroidsApp(const Emerald::ApplicationSpec& spec, Options options)
        : Application(spec), m_Options(std::move(options)),
          m_Game(m_Options.Seed != 0 ? m_Options.Seed : std::random_device{}())
    {
    }

protected:
    // The controls are named actions bound to keys; the rest of the code only uses the names,
    // so remapping a key is one line here (or a RebindAction call at runtime).
    void OnStart() override
    {
        Emerald::Input& input = GetInput();
        input.BindAxis("Rotate", Key::A, Key::D);
        input.BindAxis("Rotate", Key::Left, Key::Right);
        input.BindAction("Thrust", {Key::W, Key::Up});
        input.BindAction("Fire", {Key::Space});
        input.BindAction("Hyperspace", {Key::LeftShift, Key::RightShift});
        input.BindAction("Start", {Key::Enter});
        input.BindAction("Quit", {Key::Escape});
    }

    void OnFixedUpdate(f32 dt) override
    {
        // Translate actions into game input. "Pressed" is true for one fixed step per press.
        const Emerald::Input& in = GetInput();
        Asteroids::GameInput input;
        input.Ship.Rotate = in.GetAxis("Rotate");
        input.Ship.Thrust = in.IsActionDown("Thrust");
        input.FirePressed = in.WasActionPressed("Fire");
        input.HyperspacePressed = in.WasActionPressed("Hyperspace");
        input.StartPressed = in.WasActionPressed("Start");
        m_Game.Update(input, dt);
    }

    void OnUpdate(f32 /*dt*/) override
    {
        if (GetInput().WasActionPressed("Quit"))
            Quit();

        // For --screenshot: capture the last frame of a --frames run (or frame 120 otherwise).
        const u64 shotFrame = m_Options.Frames != 0 ? m_Options.Frames : 120;
        if (!m_Options.ScreenshotPath.empty() && GetFrameCount() + 1 == shotFrame)
            GetRenderer().RequestScreenshot(m_Options.ScreenshotPath);
    }

    void OnRender2D(Emerald::Renderer2D& r) override
    {
        // Fit the fixed-size playfield into the window: uniform scale, centered, black bars on
        // the sides (or top and bottom) if the aspect ratio differs. We work in the render
        // target's real pixels (the swapchain size of this frame), so the clip rectangle below
        // is exact. Read the matrix right to left: playfield -> scaled -> moved into the
        // window -> pixel-space projection.
        const Emerald::Renderer& renderer = GetRenderer();
        const Vec2 target(static_cast<f32>(renderer.GetFrameWidth()),
                          static_cast<f32>(renderer.GetFrameHeight()));
        const Vec2 playfield = Asteroids::kPlayfieldSize;
        const f32 scale = Emerald::Min(target.x / playfield.x, target.y / playfield.y);
        const Vec2 size = playfield * scale;
        const Vec2 offset = (target - size) * 0.5f;
        const Mat4 viewProjection = Mat4::OrthoPixelSpace(target.x, target.y) *
                                    Mat4::Translate(offset) * Mat4::Scale(Vec2(scale));

        // Clip to the playfield, so objects wrapping around an edge do not show in the bars.
        // (Rounded outwards, so nothing on the playfield's edge is cut off.)
        const i32 left = static_cast<i32>(std::floor(offset.x));
        const i32 top = static_cast<i32>(std::floor(offset.y));
        const SDL_Rect clip{left, top, static_cast<i32>(std::ceil(offset.x + size.x)) - left,
                            static_cast<i32>(std::ceil(offset.y + size.y)) - top};

        r.Begin(viewProjection, clip);
        m_Game.Draw(r);
        // Outline the playfield when there are bars, so the wrap-around edges are visible.
        if (offset.x >= 1.0f || offset.y >= 1.0f)
            r.DrawRect({0.5f, 0.5f}, playfield - Vec2(1.0f), {1.0f, 1.0f, 1.0f, 0.2f});
        r.End();
    }

    void OnImGui() override
    {
#if EMERALD_WITH_IMGUI
        ImGui::SetNextWindowPos(ImVec2(10.0f, 130.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Asteroids debug");
        ImGui::Text("FPS: %.0f", static_cast<f64>(ImGui::GetIO().Framerate));
        ImGui::Text("Fixed update: %.0f Hz", static_cast<f64>(1.0f / GetFixedDeltaSeconds()));
        ImGui::Text("Wave %u, %zu asteroids", m_Game.GetWave(), m_Game.GetAsteroidCount());
        ImGui::Text("Score %u, lives %u%s", m_Game.GetScore(), m_Game.GetLives(),
                    m_Game.IsGameOver() ? " (game over)" : "");
        ImGui::Text("Lines drawn: %u", GetRenderer2D().GetLastFrameLineCount());
        ImGui::End();
#endif
    }

private:
    Options m_Options;
    Asteroids::Game m_Game;
};

Options ParseOptions(i32 argc, char** argv)
{
    Options options;
    for (i32 i = 1; i + 1 < argc; ++i) {
        const std::string_view arg(argv[i]);
        const std::string_view value(argv[i + 1]);
        if (arg == "--frames") {
            std::from_chars(value.data(), value.data() + value.size(), options.Frames);
            ++i;
        } else if (arg == "--screenshot") {
            options.ScreenshotPath = value;
            ++i;
        } else if (arg == "--seed") {
            std::from_chars(value.data(), value.data() + value.size(), options.Seed);
            ++i;
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv)
{
    Options options = ParseOptions(argc, argv);

    Emerald::ApplicationSpec spec;
    spec.Window.Title = "Asteroids";
    spec.Window.Width = 1280;
    spec.Window.Height = 720;
    spec.ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    spec.FixedUpdateRate = 120.0;
    spec.MaxFrames = options.Frames;
    spec.LogFile = "logs/Asteroids.log";
    spec.ShaderFormats = EMERALD_SHADER_FORMATS; // formats generated by emerald_add_shaders()

    AsteroidsApp app(spec, std::move(options));
    return app.Run();
}
