// The shared application of both versions, on top of the Emerald engine.
//
// This file connects the Game to the engine: Emerald's Application owns the window, the GPU
// and the main loop, and calls the hooks below. Game logic runs in OnFixedUpdate (120 times per
// second, whatever the frame rate), drawing happens in OnRender2D. Both executables (vector and
// pixel) derive from AsteroidsApp and only add how the world is drawn.

#include "AsteroidsApp.h"

#include <charconv>
#include <cmath>
#include <random>
#include <string_view>
#include <utility>

#include <SDL3/SDL_events.h>

#if EMERALD_WITH_IMGUI
#include <imgui.h>
#endif

#include "GameInfo.h"
#include "Icon.h"
#include "Playfield.h"
#include "VectorFont.h"

namespace Asteroids {

namespace {

using Emerald::GamepadAxis;
using Emerald::GamepadButton;
using Emerald::Key;
using Emerald::Mat4;

constexpr f32 kShakeDecay = 7.0f; // per second (exponential)
constexpr f32 kMaxShake = 10.0f;  // playfield pixels
const Vec4 kMenuHint{0.95f, 0.97f, 1.0f, 0.45f};

// Seconds into the title screen at which `page` shows (see TitleScreen::PageAt).
f32 TitleTimeFor(TitleScreen::Page page, bool haveHighScores)
{
    const f32 half = 0.5f * TitleScreen::kPageSeconds;
    switch (page) {
    case TitleScreen::Page::HighScores:
        return TitleScreen::kPageSeconds + half;
    case TitleScreen::Page::Controls:
        return (haveHighScores ? 2.0f : 1.0f) * TitleScreen::kPageSeconds + half;
    default:
        return 0.0f;
    }
}

} // namespace

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
        } else if (arg == "--saucer") {
            options.Saucer = value;
            ++i;
        } else if (arg == "--game-over") {
            u32 score = 0;
            std::from_chars(value.data(), value.data() + value.size(), score);
            options.GameOverScore = score;
            ++i;
        } else if (arg == "--screen") {
            options.Screen = value;
            ++i;
        }
    }
    return options;
}

Emerald::ApplicationSpec MakeSpec(const Options& options, const Settings& settings,
                                  const GameInfo::Edition& edition)
{
    Emerald::ApplicationSpec spec;
    spec.Window.Title = edition.Title;
    spec.Window.Width = 1280;
    spec.Window.Height = 720;
    // Headless/automated runs (--frames) always use a window, whatever was saved.
    spec.Window.Fullscreen = settings.Fullscreen && options.Frames == 0;
    spec.Window.VSync = settings.VSync;
    spec.ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    spec.FixedUpdateRate = 120.0;
    spec.MaxFrames = options.Frames;
    // The log goes to the per-user folder (the game's folder may not be writable, e.g. when it
    // was unzipped somewhere under Program Files), else next to the executable.
    const std::filesystem::path pref =
        Emerald::Paths::GetPrefPath(GameInfo::kOrganization, edition.FileName);
    const std::string logName = std::string(edition.FileName) + ".log";
    spec.LogFile = pref.empty() ? std::filesystem::path("logs") / logName : pref / "logs" / logName;
    return spec;
}

u32 MakeSeed(const Options& options)
{
    return options.Seed != 0 ? options.Seed : std::random_device{}();
}

bool RepeatingPress::Update(bool pressed, bool down, f32 dt)
{
    if (pressed) {
        m_HeldTime = 0.0f;
        m_NextRepeat = kDelay;
        return true;
    }
    if (!down)
        return false;
    m_HeldTime += dt;
    if (m_HeldTime < m_NextRepeat)
        return false;
    m_NextRepeat += kInterval;
    return true;
}

AsteroidsApp::AsteroidsApp(const Emerald::ApplicationSpec& spec, const GameInfo::Edition& edition,
                           Options options, Settings settings, std::string highScoresFile,
                           std::unique_ptr<GameMode> mode)
    : Application(spec), m_Edition(edition), m_Options(std::move(options)), m_Settings(settings),
      m_SettingsFile(Settings::DefaultPath(edition.FileName)),
      m_HighScoresName(std::move(highScoresFile)), m_Game(std::move(mode)),
      m_TitleMenu(MakeTitleMenu(*m_Game))
{
}

Menu AsteroidsApp::MakeTitleMenu(const GameMode& mode)
{
    std::vector<std::string> items{"START GAME"};
    for (std::string& extra : mode.GetTitleExtras())
        items.push_back(std::move(extra));
    items.push_back("OPTIONS");
    items.push_back("QUIT GAME");
    return Menu(std::move(items));
}

std::filesystem::path AsteroidsApp::GetUserFile(std::string_view name) const
{
    const std::filesystem::path folder =
        Emerald::Paths::GetPrefPath(GameInfo::kOrganization, m_Edition.FileName);
    return folder.empty() ? folder : (folder / name).make_preferred();
}

// The controls are named actions bound to keys and gamepad inputs; the rest of the code only
// uses the names, so remapping is one line here (or a RebindAction call at runtime). Gamepad
// buttons are named by position (South = A / Cross / B on Switch), so this works for Xbox,
// PlayStation and Switch pads alike.
void AsteroidsApp::BindControls()
{
    Emerald::Input& input = GetInput();
    input.BindAxis("Rotate", Key::A, Key::D);
    input.BindAxis("Rotate", Key::Left, Key::Right);
    input.BindAxis("Rotate", GamepadAxis::LeftX); // analog: a half-tilted stick turns slower
    input.BindAxis("Rotate", GamepadButton::DPadLeft, GamepadButton::DPadRight);
    input.BindAction("Thrust", {Key::W, Key::Up});
    input.BindAction(
        "Thrust", {GamepadButton::LeftStickUp, GamepadButton::RightTrigger, GamepadButton::DPadUp});
    input.BindAction("Fire", {Key::Space});
    input.BindAction("Fire", {GamepadButton::South, GamepadButton::RightShoulder});
    input.BindAction("Hyperspace", {Key::LeftShift, Key::RightShift});
    input.BindAction("Hyperspace", {GamepadButton::North});
    input.BindAction("Start", {Key::Enter});
    input.BindAction("Start", {GamepadButton::Start, GamepadButton::South});
    input.BindAction("Mute", {Key::M});
    input.BindAction("Mute", {GamepadButton::Back});
    // Pause menu in a game; the title screen's menu on the title screen. (Pad Start also starts
    // a game there and on the game over screen, which wins.)
    input.BindAction("Pause", {Key::Escape, Key::P});
    input.BindAction("Pause", {GamepadButton::Start});
    // Menu actions (menus and entering initials).
    input.BindAction("MenuUp", {Key::W, Key::Up});
    input.BindAction("MenuUp", {GamepadButton::DPadUp, GamepadButton::LeftStickUp});
    input.BindAction("MenuDown", {Key::S, Key::Down});
    input.BindAction("MenuDown", {GamepadButton::DPadDown, GamepadButton::LeftStickDown});
    input.BindAction("MenuLeft", {Key::A, Key::Left});
    input.BindAction("MenuLeft", {GamepadButton::DPadLeft, GamepadButton::LeftStickLeft});
    input.BindAction("MenuRight", {Key::D, Key::Right});
    input.BindAction("MenuRight", {GamepadButton::DPadRight, GamepadButton::LeftStickRight});
    input.BindAction("Confirm", {Key::Enter, Key::Space});
    input.BindAction("Confirm", {GamepadButton::South});
    input.BindAction("Back", {Key::Backspace}); // initials: previous letter
    input.BindAction("Back", {GamepadButton::East});
    input.BindAction("MenuBack", {Key::Escape, Key::Backspace});
    input.BindAction("MenuBack", {GamepadButton::East});
}

void AsteroidsApp::OnStart()
{
    BindControls();
    EM_INFO("{} {}; settings file: {}", m_Edition.Title, GameInfo::kVersion,
            m_SettingsFile.empty() ? "none" : m_SettingsFile.string());
    GetWindow().SetIcon(MakeIcon(64));
    ApplySettings();

    // A 1 x 1 white texture: tinted black and stretched, it darkens the game behind menus.
    Emerald::Image white;
    white.Width = 1;
    white.Height = 1;
    white.Pixels = {255, 255, 255, 255};
    m_White = Emerald::Texture::Create(GetRenderer().GetDevice(), white);

    m_Sounds = MakeSounds(); // all generated, takes a few milliseconds
    LoadSoundOverrides();
    LoadHighScores();
    ApplyStartScreen();

    OnLoadAssets(); // the version's own assets (e.g. textures)
}

// The title screen, unless the command line asks for something else (tests, screenshots).
void AsteroidsApp::ApplyStartScreen()
{
    const std::string& screen = m_Options.Screen;
    const bool haveScores = !m_Game->GetHighScores().GetEntries().empty();
    if (screen == "scores")
        m_TitleTime = TitleTimeFor(TitleScreen::Page::HighScores, haveScores);
    else if (screen == "controls")
        m_TitleTime = TitleTimeFor(TitleScreen::Page::Controls, haveScores);

    const bool play = screen == "play" || screen == "pause" || screen == "options" ||
                      !m_Options.Saucer.empty() || m_Options.GameOverScore.has_value();
    if (!play)
        return;
    m_Game->StartGame();
    if (m_Options.Saucer == "large" || m_Options.Saucer == "small")
        m_Game->SpawnSaucer(m_Options.Saucer == "large" ? SaucerSize::Large : SaucerSize::Small);
    if (m_Options.GameOverScore)
        m_Game->ForceGameOver(*m_Options.GameOverScore);
    if (screen == "pause" || screen == "options")
        Pause();
    if (screen == "options")
        OpenOverlay(Overlay::Options);
}

void AsteroidsApp::OnEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        // F11 or Alt+Enter: fullscreen on/off (the Enter of Alt+Enter is ignored by the game).
        if (!event.key.repeat &&
            (event.key.scancode == SDL_SCANCODE_F11 ||
             (event.key.scancode == SDL_SCANCODE_RETURN && (event.key.mod & SDL_KMOD_ALT) != 0)))
            ToggleFullscreen();
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        // Alt-tabbed away (or a notification took focus) mid-game: pause.
        if (m_Overlays.empty() && !m_Game->IsOnTitle() && !m_Game->IsOnGameOverScreen() &&
            !m_Game->IsInMenuScreen())
            Pause();
        break;
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN: {
        // Also changes made by the OS (e.g. a window manager shortcut): keep the setting in sync.
        const bool fullscreen = event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN;
        if (m_Settings.Fullscreen != fullscreen) {
            m_Settings.Fullscreen = fullscreen;
            SaveSettings();
        }
        break;
    }
    default:
        break;
    }
}

void AsteroidsApp::OnFixedUpdate(f32 dt)
{
    // Translate actions into input. "Pressed" is true for one fixed step per press.
    Emerald::Input& in = GetInput();
    const Emerald::Keyboard& keys = in.GetKeyboard();
    const bool alt = keys.IsKeyDown(Key::LeftAlt) || keys.IsKeyDown(Key::RightAlt); // Alt+Enter
    const auto repeat = [&](RepeatingPress& press, const char* action) {
        return press.Update(in.WasActionPressed(action), in.IsActionDown(action), dt);
    };
    MenuInput menu;
    menu.Up = repeat(m_MenuUp, "MenuUp");
    menu.Down = repeat(m_MenuDown, "MenuDown");
    menu.Left = repeat(m_MenuLeft, "MenuLeft");
    menu.Right = repeat(m_MenuRight, "MenuRight");
    menu.Confirm = in.WasActionPressed("Confirm") && !alt;
    menu.Back = in.WasActionPressed("MenuBack");
    const bool pausePressed = in.WasActionPressed("Pause");
    m_MenuTime += dt;
    m_Shake *= std::exp(-kShakeDecay * dt);

    if (!m_Overlays.empty()) {
        // The pause key (P, pad Start) also closes the pause menu; Esc does that as "back".
        if (pausePressed && !menu.Back && m_Overlays.back() == Overlay::PauseMenu)
            CloseOverlay();
        else
            UpdateOverlay(menu);
        if (!m_Game->IsOnTitle())
            return; // paused
        m_TitleTime += dt;
        m_Game->Update({}, dt); // the title screen's rocks keep drifting behind its menus
        m_Effects.Update(*m_Game, dt);
        PlayGameSounds();
        OnGameStepped();
        return;
    }

    // The mode's own screens (e.g. a shop) get the menu input, Esc included.
    if (m_Game->IsInMenuScreen()) {
        GameInput input;
        input.MenuUpPressed = menu.Up;
        input.MenuDownPressed = menu.Down;
        input.MenuLeftPressed = menu.Left;
        input.MenuRightPressed = menu.Right;
        input.ConfirmPressed = menu.Confirm;
        input.MenuBackPressed = menu.Back;
        m_Game->Update(input, dt);
        m_Effects.Update(*m_Game, dt);
        PlayGameSounds();
        OnGameStepped();
        if (m_Game->IsOnTitle())
            m_TitleTime = 0.0f;
        return;
    }

    GameInput input;
    input.Ship.Rotate = in.GetAxis("Rotate");
    input.Ship.Thrust = in.IsActionDown("Thrust");
    input.FirePressed = in.WasActionPressed("Fire");
    input.HyperspacePressed = in.WasActionPressed("Hyperspace");
    input.StartPressed = in.WasActionPressed("Start") && !alt;
    input.MenuUpPressed = menu.Up;
    input.MenuDownPressed = menu.Down;
    input.MenuLeftPressed = menu.Left;
    input.MenuRightPressed = menu.Right;
    input.ConfirmPressed = menu.Confirm;
    input.BackPressed = in.WasActionPressed("Back");

    if (m_Game->IsOnTitle()) {
        m_TitleTime += dt;
        if (input.StartPressed || menu.Confirm)
            m_Game->StartGame();
        else if (pausePressed)
            OpenOverlay(Overlay::TitleMenu);
        input = {}; // nothing to steer on the title screen, and no shot from the start key
    } else if (pausePressed && !(m_Game->IsOnGameOverScreen() && input.StartPressed)) {
        Pause();
        return;
    }
    const bool wasOnTitle = m_Game->IsOnTitle();
    m_Game->Update(input, dt);
    if (!wasOnTitle && m_Game->IsOnTitle())
        m_TitleTime = 0.0f; // the game over screen timed out
    if (m_Game->ConsumeHighScoresChanged() && !m_HighScoresFile.empty())
        m_Game->GetHighScores().Save(m_HighScoresFile);

    // A short, heavy rumble when the ship is destroyed.
    if (m_Game->GetShipsLost() != m_ShipsLost) {
        m_ShipsLost = m_Game->GetShipsLost();
        in.Rumble(0.8f, 0.4f, 300);
    }
    m_Effects.Update(*m_Game, dt); // (reads the sounds, so before PlayGameSounds clears them)
    PlayGameSounds();
    OnGameStepped();
}

void AsteroidsApp::OnUpdate(f32 /*dt*/)
{
    if (GetInput().WasActionPressed("Mute"))
        GetAudio().SetMuted(!GetAudio().IsMuted());
    UpdatePrompts();
    UpdateBackgroundSounds();

    // For --screenshot: capture the last frame of a --frames run (or frame 120 otherwise).
    const u64 shotFrame = m_Options.Frames != 0 ? m_Options.Frames : 120;
    if (!m_Options.ScreenshotPath.empty() && GetFrameCount() + 1 == shotFrame)
        GetRenderer().RequestScreenshot(m_Options.ScreenshotPath);
}

void AsteroidsApp::OnRender2D(Emerald::Renderer2D& r)
{
    // Fit the fixed-size playfield into the window: uniform scale, centered, black bars on
    // the sides (or top and bottom) if the aspect ratio differs. We work in the render
    // target's real pixels (the swapchain size of this frame), so the clip rectangle below
    // is exact. Read the matrix right to left: playfield -> scaled -> moved into the
    // window -> pixel-space projection.
    const Emerald::Renderer& renderer = GetRenderer();
    const Vec2 target(static_cast<f32>(renderer.GetFrameWidth()),
                      static_cast<f32>(renderer.GetFrameHeight()));
    const Vec2 playfield = kPlayfieldSize;
    const f32 scale = Emerald::Min(target.x / playfield.x, target.y / playfield.y);
    const Vec2 size = playfield * scale;
    const Vec2 offset = (target - size) * 0.5f;
    // Screen shake: the whole playfield jumps by a random offset that dies down quickly.
    Vec2 shake{};
    if (m_Settings.ScreenShake && m_Overlays.empty() && m_Shake > 0.05f)
        shake = m_ShakeRandom.Direction() * m_Shake;
    const Mat4 viewProjection = Mat4::OrthoPixelSpace(target.x, target.y) *
                                Mat4::Translate(offset) * Mat4::Scale(Vec2(scale)) *
                                Mat4::Translate(shake);

    // Clip to the playfield, so objects wrapping around an edge do not show in the bars.
    // (Rounded outwards, so nothing on the playfield's edge is cut off.)
    const i32 left = static_cast<i32>(std::floor(offset.x));
    const i32 top = static_cast<i32>(std::floor(offset.y));
    const SDL_Rect clip{left, top, static_cast<i32>(std::ceil(offset.x + size.x)) - left,
                        static_cast<i32>(std::ceil(offset.y + size.y)) - top};

    r.Begin(viewProjection, clip);
    DrawWorld(r);
    m_Effects.Draw(r, GetParticleDrawOptions());
    if (!m_Game->IsOnTitle())
        DrawHud(r);
    else if (m_Options.Screen == "logo")
        TitleScreen::DrawCover(r, m_Edition, m_TitleTime);
    else if (m_Overlays.empty())
        TitleScreen::Draw(
            r, m_Edition, *m_Game,
            TitleScreen::PageAt(m_TitleTime, !m_Game->GetHighScores().GetEntries().empty()),
            m_Prompts.Start, m_PadLabels, m_TitleTime);
    DrawOverlay(r);
    // Outline the playfield when there are bars, so the wrap-around edges are visible.
    if (offset.x >= 1.0f || offset.y >= 1.0f)
        r.DrawRect({0.5f, 0.5f}, playfield - Vec2(1.0f), {1.0f, 1.0f, 1.0f, 0.2f});
    r.End();
}

void AsteroidsApp::OnImGui()
{
#if EMERALD_WITH_IMGUI
    ImGui::SetNextWindowPos(ImVec2(10.0f, 130.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Debug");
    ImGui::Text("FPS: %.0f", static_cast<f64>(ImGui::GetIO().Framerate));
    ImGui::Text("Fixed update: %.0f Hz", static_cast<f64>(1.0f / GetFixedDeltaSeconds()));
    ImGui::Text("Wave %u, %zu asteroids", m_Game->GetWave(), m_Game->GetAsteroidCount());
    ImGui::Text("Score %u, lives %u%s", m_Game->GetScore(), m_Game->GetLives(),
                m_Game->IsGameOver() ? " (game over)" : "");
    const Emerald::Renderer2D& r2d = GetRenderer2D();
    ImGui::Text("Particles: %u / %u", m_Effects.GetParticles().GetCount(),
                m_Effects.GetParticles().GetCapacity());
    ImGui::Text("Drawn: %u lines, %u sprites, %u draw calls", r2d.GetLastFrameLineCount(),
                r2d.GetLastFrameSpriteCount(), r2d.GetLastFrameDrawCalls());
    // Testing shortcuts.
    if (ImGui::Button("Large saucer"))
        m_Game->SpawnSaucer(SaucerSize::Large);
    ImGui::SameLine();
    if (ImGui::Button("Small saucer"))
        m_Game->SpawnSaucer(SaucerSize::Small);
    ImGui::SameLine();
    if (ImGui::Button("Game over"))
        m_Game->ForceGameOver(m_Game->GetScore());
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

// Player-supplied sounds in assets/sounds/ next to the executable replace generated ones.
void AsteroidsApp::LoadSoundOverrides()
{
    const std::filesystem::path folder = Emerald::Paths::GetBasePath() / "assets" / "sounds";
    for (const std::string& name : LoadOverrides(m_Sounds, folder))
        m_Overrides += (m_Overrides.empty() ? "" : ", ") + name;
    EM_INFO("Sound overrides from {}: {}", folder.string(),
            m_Overrides.empty() ? "none (using generated sounds)" : m_Overrides);
}

// The table lives in the per-user folder (e.g. %APPDATA%\Ridejock\<game> on Windows).
void AsteroidsApp::LoadHighScores()
{
    m_HighScoresFile = GetUserFile(m_HighScoresName);
    if (m_HighScoresFile.empty()) {
        EM_WARN("No folder for high scores: they will not be saved");
        return;
    }
    EM_INFO("High scores file: {}", m_HighScoresFile.string());
    m_Game->SetHighScores(HighScoreTable::Load(m_HighScoresFile));
}

// Optional loops: `ambience` under the gameplay, `music` on the title and game over screens,
// cross-faded when the state changes. Without those files (the release has none) nothing plays.
void AsteroidsApp::UpdateBackgroundSounds()
{
    const bool gameOver = m_Game->IsGameOver();
    if (m_BackgroundStarted && gameOver == m_WasGameOver)
        return;
    m_BackgroundStarted = true;
    m_WasGameOver = gameOver;
    Emerald::Audio& audio = GetAudio();
    if (gameOver) {
        audio.Stop(m_AmbienceVoice, 800.0f);
        m_MusicVoice =
            audio.Play(m_Sounds.Music, {.Volume = kMusicVolume, .Loop = true, .FadeInMs = 1500.0f});
    } else {
        audio.Stop(m_MusicVoice, 600.0f);
        m_AmbienceVoice = audio.Play(
            m_Sounds.Ambience, {.Volume = m_AmbienceVolume, .Loop = true, .FadeInMs = 1500.0f});
    }
}

// Plays what the game asked for this step, and starts/stops the looping thrust sound.
void AsteroidsApp::PlayGameSounds()
{
    Emerald::Audio& audio = GetAudio();
    for (const GameSound& sound : m_Game->GetSounds()) {
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
        case SoundEvent::SaucerFire:
            s = &m_Sounds.SaucerFire;
            break;
        case SoundEvent::SaucerExplosion:
            s = &m_Sounds.ExplosionLarge;
            break;
        case SoundEvent::ShieldHit:
            s = &m_Sounds.ShieldHit;
            break;
        case SoundEvent::Upgrade:
            s = &m_Sounds.Upgrade;
            break;
        case SoundEvent::Purchase:
            s = &m_Sounds.Purchase;
            break;
        case SoundEvent::MetalHit:
            s = &m_Sounds.MetalHit;
            volume = 0.7f;
            break;
        case SoundEvent::Blast:
            s = &m_Sounds.Blast;
            break;
        case SoundEvent::MissileLaunch:
            s = &m_Sounds.MissileLaunch;
            volume = 0.6f;
            break;
        case SoundEvent::BossAlarm:
            s = &m_Sounds.BossAlarm;
            break;
        case SoundEvent::BossExplosion:
            s = &m_Sounds.BossExplosion;
            break;
        }
        if (s)
            audio.Play(*s, {.Volume = volume * GetSfxVolume(), .Pan = sound.Pan});
        // Explosions shake the screen a little (the ship's a lot).
        if (sound.Event == SoundEvent::ShipExplosion)
            AddShake(9.0f);
        else if (sound.Event == SoundEvent::SaucerExplosion || sound.Event == SoundEvent::Blast)
            AddShake(5.0f);
        else if (sound.Event == SoundEvent::BossExplosion)
            AddShake(14.0f);
        else if (sound.Event == SoundEvent::ExplosionLarge)
            AddShake(3.0f);
        else if (sound.Event == SoundEvent::ExplosionMedium)
            AddShake(1.5f);
    }
    m_Game->ClearSounds();

    // Thrust loop: fades in when the engine fires, fades out when it stops (also on death and
    // game over, since IsThrusting is false then).
    // On every start a new voice: the previous one may still be fading out.
    const bool thrusting = m_Game->IsThrusting();
    if (thrusting && !m_WasThrusting)
        m_ThrustVoice = audio.Play(
            m_Sounds.Thrust, {.Volume = 0.45f * GetSfxVolume(), .Loop = true, .FadeInMs = 40.0f});
    else if (!thrusting && m_WasThrusting)
        audio.Stop(m_ThrustVoice, 120.0f);
    m_WasThrusting = thrusting;

    // Saucer siren: loops while a saucer is on screen, fades out when it is destroyed, flies
    // off or the game ends.
    const std::optional<SaucerSize> saucer = m_Game->GetSaucerSize();
    if (saucer != m_SaucerSound) {
        audio.Stop(m_SaucerVoice, 150.0f);
        if (saucer)
            m_SaucerVoice = audio.Play(
                *saucer == SaucerSize::Large ? m_Sounds.SaucerLarge : m_Sounds.SaucerSmall,
                {.Volume = 0.6f * GetSfxVolume(), .Loop = true, .FadeInMs = 80.0f});
        m_SaucerSound = saucer;
    }
}

// Keyboard texts without a gamepad; with one, the labels printed on its buttons are added,
// e.g. "PRESS OPTIONS / ENTER" on a PS4 pad or "PRESS + / ENTER" on a Switch Pro Controller.
void AsteroidsApp::UpdatePrompts()
{
    const Emerald::Gamepads& pads = GetInput().GetGamepads();
    Prompts prompts;
    if (pads.GetCount() > 0) {
        const auto label = [&](GamepadButton b) { return std::string(pads.GetButtonLabel(b)); };
        prompts.Start = "PRESS " + label(GamepadButton::Start) + " / ENTER";
        prompts.Confirm = label(GamepadButton::South) + " / ENTER";
        prompts.Back = label(GamepadButton::East) + " / BACKSPACE";
    }
    m_Prompts = prompts;
    m_Game->SetPrompts(std::move(prompts));

    m_PadLabels = {};
    if (pads.GetCount() > 0) {
        m_PadLabels.Fire = pads.GetButtonLabel(GamepadButton::South);
        m_PadLabels.Hyperspace = pads.GetButtonLabel(GamepadButton::North);
        m_PadLabels.Thrust = pads.GetButtonLabel(GamepadButton::RightTrigger);
        m_PadLabels.Pause = pads.GetButtonLabel(GamepadButton::Start);
        m_PadLabels.Mute = pads.GetButtonLabel(GamepadButton::Back);
    }
}

// --- Menus -----------------------------------------------------------------------------------

void AsteroidsApp::OpenOverlay(Overlay overlay)
{
    m_Overlays.push_back(overlay);
    m_MenuTime = 0.0f;
    if (overlay == Overlay::TitleMenu)
        m_TitleMenu.SetSelected(0);
    else if (overlay == Overlay::PauseMenu)
        m_PauseMenu.SetSelected(0);
    else if (overlay == Overlay::Options)
        m_OptionsMenu.SetSelected(0);
}

void AsteroidsApp::CloseOverlay()
{
    if (!m_Overlays.empty())
        m_Overlays.pop_back();
}

// Pauses the running game: the pause menu opens and the looping sounds stop (they start again by
// themselves when the game goes on).
void AsteroidsApp::Pause()
{
    OpenOverlay(Overlay::PauseMenu);
    Emerald::Audio& audio = GetAudio();
    audio.Stop(m_ThrustVoice, 80.0f);
    m_WasThrusting = false;
    audio.Stop(m_SaucerVoice, 150.0f);
    m_SaucerSound.reset();
}

void AsteroidsApp::QuitToTitle()
{
    m_Overlays.clear();
    m_Game->ShowTitle();
    m_Effects.Clear();
    m_TitleTime = 0.0f;
}

void AsteroidsApp::UpdateOverlay(const MenuInput& input)
{
    switch (m_Overlays.back()) {
    case Overlay::TitleMenu: {
        const MenuAction action = m_TitleMenu.Update(input);
        if (action == MenuAction::Back) {
            CloseOverlay();
        } else if (action == MenuAction::Confirm) {
            // START GAME, the mode's extras, OPTIONS, QUIT GAME.
            const usize selected = m_TitleMenu.GetSelected();
            const usize extras = m_TitleMenu.GetCount() - 3;
            if (selected == 0) {
                CloseOverlay();
                m_Game->StartGame();
            } else if (selected <= extras) {
                CloseOverlay();
                m_Game->OpenTitleExtra(selected - 1);
            } else if (selected == extras + 1) {
                OpenOverlay(Overlay::Options);
            } else {
                Quit();
            }
        }
        break;
    }
    case Overlay::PauseMenu: {
        const MenuAction action = m_PauseMenu.Update(input);
        if (action == MenuAction::Back) {
            CloseOverlay();
        } else if (action == MenuAction::Confirm) {
            switch (m_PauseMenu.GetSelected()) {
            case 0:
                CloseOverlay();
                break;
            case 1:
                OpenOverlay(Overlay::Options);
                break;
            case 2:
                QuitToTitle();
                break;
            default:
                Quit();
                break;
            }
        }
        break;
    }
    case Overlay::Options:
        UpdateOptions(m_OptionsMenu.Update(input));
        break;
    case Overlay::Controls:
        if (input.Back || input.Confirm)
            CloseOverlay();
        break;
    }
}

void AsteroidsApp::UpdateOptions(MenuAction action)
{
    if (action == MenuAction::None)
        return;
    if (action == MenuAction::Back) {
        CloseOverlay();
        return;
    }
    // Volumes: left/right in steps (confirm steps up and wraps around). Switches: any of them.
    const auto stepVolume = [action](u32& volume) {
        const u32 step = Settings::kVolumeStep;
        if (action == MenuAction::Left)
            volume = volume >= step ? volume - step : 0;
        else if (action == MenuAction::Right)
            volume = Emerald::Min(volume + step, 100u);
        else
            volume = volume >= 100 ? 0 : Emerald::Min(volume + step, 100u);
    };
    switch (m_OptionsMenu.GetSelected()) {
    case 0:
        stepVolume(m_Settings.MasterVolume);
        break;
    case 1:
        stepVolume(m_Settings.SfxVolume);
        GetAudio().Play(m_Sounds.Fire, {.Volume = GetSfxVolume()}); // a sample at the new level
        break;
    case 2:
        m_Settings.Fullscreen = !m_Settings.Fullscreen;
        break;
    case 3:
        m_Settings.VSync = !m_Settings.VSync;
        break;
    case 4:
        m_Settings.ScreenShake = !m_Settings.ScreenShake;
        if (m_Settings.ScreenShake)
            AddShake(6.0f); // show what it does (visible once the menu is closed)
        break;
    case 5:
        m_Settings.Particles = !m_Settings.Particles;
        break;
    case 6:
        if (action == MenuAction::Confirm)
            OpenOverlay(Overlay::Controls);
        return;
    default:
        if (action == MenuAction::Confirm)
            CloseOverlay();
        return;
    }
    ApplySettings();
    SaveSettings();
}

void AsteroidsApp::DrawOverlay(Emerald::Renderer2D& r)
{
    if (m_Overlays.empty())
        return;
    // Darken whatever is behind.
    if (m_White)
        r.DrawSprite(*m_White, kPlayfieldCenter,
                     {.Size = kPlayfieldSize, .Tint = {0.0f, 0.0f, 0.0f, 0.85f}});

    const auto onOff = [](bool value) { return std::string(value ? "ON" : "OFF"); };
    switch (m_Overlays.back()) {
    case Overlay::TitleMenu:
        TitleScreen::DrawLogo(r, m_Edition.Title, 150.0f, 900.0f, m_TitleTime);
        m_TitleMenu.Draw(r, "", 250.0f, {}, m_MenuTime);
        break;
    case Overlay::PauseMenu:
        m_PauseMenu.Draw(r, "PAUSED", 200.0f, {}, m_MenuTime);
        break;
    case Overlay::Options: {
        const std::string values[] = {std::to_string(m_Settings.MasterVolume),
                                      std::to_string(m_Settings.SfxVolume),
                                      onOff(m_Settings.Fullscreen),
                                      onOff(m_Settings.VSync),
                                      onOff(m_Settings.ScreenShake),
                                      onOff(m_Settings.Particles),
                                      "",
                                      ""};
        m_OptionsMenu.Draw(r, "OPTIONS", 130.0f, values, m_MenuTime);
        VectorFont::DrawTextCentered(r, "UP / DOWN: CHOOSE    LEFT / RIGHT: CHANGE    ESC: BACK",
                                     kPlayfieldCenter.x, 600.0f, 14.0f, kMenuHint);
        break;
    }
    case Overlay::Controls:
        DrawControls(r, 190.0f, m_PadLabels);
        VectorFont::DrawTextCentered(r, "ENTER / ESC: BACK", kPlayfieldCenter.x, 560.0f, 16.0f,
                                     kMenuHint);
        break;
    }
}

// --- Settings --------------------------------------------------------------------------------

void AsteroidsApp::ApplySettings()
{
    GetAudio().SetMasterVolume(static_cast<f32>(m_Settings.MasterVolume) / 100.0f);
    m_Effects.SetEnabled(m_Settings.Particles);
    if (GetRenderer().IsVSync() != m_Settings.VSync)
        GetRenderer().SetVSync(m_Settings.VSync);
    if (GetWindow().IsFullscreen() != m_Settings.Fullscreen && m_Options.Frames == 0)
        GetWindow().SetFullscreen(m_Settings.Fullscreen);
}

void AsteroidsApp::SaveSettings()
{
    if (m_Options.Frames == 0) // automated runs leave the player's settings alone
        m_Settings.Save(m_SettingsFile);
}

void AsteroidsApp::ToggleFullscreen()
{
    m_Settings.Fullscreen = !m_Settings.Fullscreen;
    ApplySettings();
    SaveSettings();
}

void AsteroidsApp::AddShake(f32 amount)
{
    m_Shake = Emerald::Min(m_Shake + amount, kMaxShake);
}

} // namespace Asteroids
