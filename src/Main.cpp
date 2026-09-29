// Asteroids: a classic vector arcade game on top of the Emerald engine.
//
// This file connects the Game to the engine: Emerald's Application owns the window, the GPU
// and the main loop, and calls the hooks below. Game logic runs in OnFixedUpdate (120 times per
// second, whatever the frame rate), drawing happens in OnRender2D.

#include <charconv>
#include <cmath>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3/SDL_filesystem.h>

#include <Emerald/Emerald.h>

#if EMERALD_WITH_IMGUI
#include <imgui.h>
#endif

#include "Game.h"
#include "Playfield.h"
#include "Sounds.h"

namespace {

using Emerald::GamepadAxis;
using Emerald::GamepadButton;
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
    // The controls are named actions bound to keys and gamepad inputs; the rest of the code only
    // uses the names, so remapping is one line here (or a RebindAction call at runtime). Gamepad
    // buttons are named by position (South = A / Cross / B on Switch), so this works for Xbox,
    // PlayStation and Switch pads alike.
    void OnStart() override
    {
        Emerald::Input& input = GetInput();
        input.BindAxis("Rotate", Key::A, Key::D);
        input.BindAxis("Rotate", Key::Left, Key::Right);
        input.BindAxis("Rotate", GamepadAxis::LeftX); // analog: a half-tilted stick turns slower
        input.BindAxis("Rotate", GamepadButton::DPadLeft, GamepadButton::DPadRight);
        input.BindAction("Thrust", {Key::W, Key::Up});
        input.BindAction("Thrust", {GamepadButton::LeftStickUp, GamepadButton::RightTrigger,
                                    GamepadButton::DPadUp});
        input.BindAction("Fire", {Key::Space});
        input.BindAction("Fire", {GamepadButton::South, GamepadButton::RightShoulder});
        input.BindAction("Hyperspace", {Key::LeftShift, Key::RightShift});
        input.BindAction("Hyperspace", {GamepadButton::North});
        input.BindAction("Start", {Key::Enter});
        input.BindAction("Start", {GamepadButton::Start, GamepadButton::South});
        input.BindAction("Mute", {Key::M});
        input.BindAction("Mute", {GamepadButton::Back});
        input.BindAction("Quit", {Key::Escape});

        m_Sounds = Asteroids::MakeSounds(); // all generated, takes a few milliseconds
        LoadSoundOverrides();
    }

    void OnFixedUpdate(f32 dt) override
    {
        // Translate actions into game input. "Pressed" is true for one fixed step per press.
        Emerald::Input& in = GetInput();
        Asteroids::GameInput input;
        input.Ship.Rotate = in.GetAxis("Rotate");
        input.Ship.Thrust = in.IsActionDown("Thrust");
        input.FirePressed = in.WasActionPressed("Fire");
        input.HyperspacePressed = in.WasActionPressed("Hyperspace");
        input.StartPressed = in.WasActionPressed("Start");
        m_Game.Update(input, dt);

        // A short, heavy rumble when the ship is destroyed.
        if (m_Game.GetShipsLost() != m_ShipsLost) {
            m_ShipsLost = m_Game.GetShipsLost();
            in.Rumble(0.8f, 0.4f, 300);
        }
        PlayGameSounds();
    }

    void OnUpdate(f32 /*dt*/) override
    {
        if (GetInput().WasActionPressed("Quit"))
            Quit();
        if (GetInput().WasActionPressed("Mute"))
            GetAudio().SetMuted(!GetAudio().IsMuted());
        UpdateStartPrompt();
        UpdateBackgroundSounds();

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
        const Emerald::Gamepads& pads = GetInput().GetGamepads();
        for (usize i = 0; i < pads.GetCount(); ++i)
            ImGui::Text("Pad: %s (%s)", pads.GetInfo(i).Name.c_str(),
                        Emerald::GetGamepadTypeName(pads.GetInfo(i).Type));
        ImGui::Text("Rotate %+.2f", static_cast<f64>(GetInput().GetAxis("Rotate")));
        ImGui::Separator();
        Emerald::Audio& audio = GetAudio();
        f32 volume = audio.GetMasterVolume();
        if (ImGui::SliderFloat("Master volume", &volume, 0.0f, 1.0f))
            audio.SetMasterVolume(volume);
        bool muted = audio.IsMuted();
        if (ImGui::Checkbox("Muted (M)", &muted))
            audio.SetMuted(muted);
        if (ImGui::SliderFloat("Ambience volume", &m_AmbienceVolume, 0.0f, 1.0f))
            audio.SetVolume(m_AmbienceVoice, m_AmbienceVolume);
        ImGui::Text("Sound overrides: %s", m_Overrides.empty() ? "none" : m_Overrides.c_str());
        ImGui::Text("Audio: %s, %zu voices playing", audio.IsAvailable() ? "on" : "no device",
                    audio.GetPlayingCount());
        ImGui::End();
#endif
    }

private:
    // Player-supplied sounds in assets/sounds/ next to the executable replace generated ones.
    void LoadSoundOverrides()
    {
        // SDL_GetBasePath is UTF-8 (and ends with a separator).
        const std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "";
        const std::filesystem::path folder =
            std::filesystem::path(std::u8string(base.begin(), base.end())) / "assets" / "sounds";
        for (const std::string& name : Asteroids::LoadOverrides(m_Sounds, folder))
            m_Overrides += (m_Overrides.empty() ? "" : ", ") + name;
        EM_INFO("Sound overrides from {}: {}", folder.string(),
                m_Overrides.empty() ? "none (using generated sounds)" : m_Overrides);
    }

    // Optional loops: `ambience` under the gameplay, `music` on the game over screen, cross-faded
    // when the state changes. Without those files nothing plays (Play ignores empty sounds).
    void UpdateBackgroundSounds()
    {
        const bool gameOver = m_Game.IsGameOver();
        if (m_BackgroundStarted && gameOver == m_WasGameOver)
            return;
        m_BackgroundStarted = true;
        m_WasGameOver = gameOver;
        Emerald::Audio& audio = GetAudio();
        if (gameOver) {
            audio.Stop(m_AmbienceVoice, 800.0f);
            m_MusicVoice = audio.Play(m_Sounds.Music,
                                      {.Volume = kMusicVolume, .Loop = true, .FadeInMs = 1500.0f});
        } else {
            audio.Stop(m_MusicVoice, 600.0f);
            m_AmbienceVoice = audio.Play(
                m_Sounds.Ambience, {.Volume = m_AmbienceVolume, .Loop = true, .FadeInMs = 1500.0f});
        }
    }

    // Plays what the game asked for this step, and starts/stops the looping thrust sound.
    void PlayGameSounds()
    {
        Emerald::Audio& audio = GetAudio();
        for (const Asteroids::GameSound& sound : m_Game.GetSounds()) {
            using Asteroids::SoundEvent;
            const Emerald::Sound* s = nullptr;
            f32 volume = 1.0f;
            switch (sound.Event) {
            case SoundEvent::Fire:
                s = &m_Sounds.Fire;
                break;
            case SoundEvent::ExplosionLarge:
                s = &m_Sounds.ExplosionLarge;
                break;
            case SoundEvent::ExplosionMedium:
                s = &m_Sounds.ExplosionMedium;
                break;
            case SoundEvent::ExplosionSmall:
                s = &m_Sounds.ExplosionSmall;
                break;
            case SoundEvent::ShipExplosion:
                s = &m_Sounds.ShipExplosion;
                break;
            case SoundEvent::ExtraLife:
                s = &m_Sounds.ExtraLife;
                break;
            case SoundEvent::Hyperspace:
                s = &m_Sounds.Hyperspace;
                break;
            case SoundEvent::BeatHigh:
                s = &m_Sounds.BeatHigh;
                volume = 0.8f;
                break;
            case SoundEvent::BeatLow:
                s = &m_Sounds.BeatLow;
                volume = 0.8f;
                break;
            }
            if (s)
                audio.Play(*s, {.Volume = volume, .Pan = sound.Pan});
        }
        m_Game.ClearSounds();

        // Thrust loop: fades in when the engine fires, fades out when it stops (also on death and
        // game over, since IsThrusting is false then).
        // On every start a new voice: the previous one may still be fading out.
        const bool thrusting = m_Game.IsThrusting();
        if (thrusting && !m_WasThrusting)
            m_ThrustVoice =
                audio.Play(m_Sounds.Thrust, {.Volume = 0.45f, .Loop = true, .FadeInMs = 40.0f});
        else if (!thrusting && m_WasThrusting)
            audio.Stop(m_ThrustVoice, 120.0f);
        m_WasThrusting = thrusting;
    }

    // "PRESS ENTER" without a gamepad, otherwise the label of the pad's South button, e.g.
    // "PRESS CROSS" on a PS4 pad or "PRESS B" on a Switch Pro Controller.
    void UpdateStartPrompt()
    {
        const Emerald::Gamepads& pads = GetInput().GetGamepads();
        std::string prompt = "PRESS ENTER";
        if (pads.GetCount() > 0)
            prompt = std::string("PRESS ") + pads.GetButtonLabel(GamepadButton::South);
        m_Game.SetStartPrompt(std::move(prompt));
    }

    Options m_Options;
    Asteroids::Game m_Game;
    u32 m_ShipsLost = 0;
    Asteroids::Sounds m_Sounds;
    Emerald::VoiceHandle m_ThrustVoice;
    bool m_WasThrusting = false;
    static constexpr f32 kMusicVolume = 0.35f;
    f32 m_AmbienceVolume = 0.25f;
    Emerald::VoiceHandle m_MusicVoice;
    Emerald::VoiceHandle m_AmbienceVoice;
    bool m_BackgroundStarted = false;
    bool m_WasGameOver = false;
    std::string m_Overrides; // names of the loaded overrides, for the log and the debug panel
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
