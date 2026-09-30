#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Emerald/Emerald.h>

#include "GameInfo.h"
#include "GameMode.h"
#include "Menu.h"
#include "ParticleEffects.h"
#include "Random.h"
#include "Screens.h"
#include "Settings.h"
#include "Sounds.h"

namespace Asteroids {

// Command line options (handy for automated runs): --frames N quits after N frames,
// --screenshot out.png saves the last frame, --seed N makes the asteroids repeatable.
// For testing and store screenshots:
//   --saucer large|small   start a game with a saucer right away
//   --game-over SCORE      start a game and end it at once with that score (initials entry)
//   --screen NAME          start on: title, scores, controls (title screen pages), play,
//                          pause, options, or logo (just the logo and a tagline: store cover)
//   --crt on|off           CRT effect for this run (games that offer it), whatever was saved
struct Options {
    u64 Frames = 0;
    std::string ScreenshotPath;
    u32 Seed = 0; // 0 = random
    std::string Saucer;
    std::optional<u32> GameOverScore;
    std::string Screen;
    std::optional<bool> Crt;
};

[[nodiscard]] Options ParseOptions(i32 argc, char** argv);

// The window, log file etc. every version uses, for `edition` (title, per-user folder, log
// file name). The window starts fullscreen / with vsync as saved in `settings`. The caller still
// sets ShaderFormats (EMERALD_SHADER_FORMATS is defined for the executable).
[[nodiscard]] Emerald::ApplicationSpec MakeSpec(const Options& options, const Settings& settings,
                                                const GameInfo::Edition& edition);
// The seed for the game: --seed, or a random one.
[[nodiscard]] u32 MakeSeed(const Options& options);

// Up/Down auto-repeat while held (for cycling through letters and menus): one step on every
// press (even a very short tap), more after a short delay while the button stays down.
class RepeatingPress {
public:
    bool Update(bool pressed, bool down, f32 dt);

private:
    static constexpr f32 kDelay = 0.4f;     // seconds before repeating starts
    static constexpr f32 kInterval = 0.09f; // seconds between repeats
    f32 m_HeldTime = 0.0f;
    f32 m_NextRepeat = kDelay;
};

// Everything both versions share: controls (input actions), the fixed-step game update, the
// title screen, pause and options menus, settings, sounds (events, loops, overrides), high score
// loading/saving, fitting the playfield into the window, and the debug panel. A version brings
// its rules (a GameMode: the classic Game or the roguelike), says how the world looks
// (DrawWorld) and may load its own assets (OnLoadAssets).
class AsteroidsApp : public Emerald::Application {
public:
    // `highScoresFile`: file name in the edition's per-user folder.
    AsteroidsApp(const Emerald::ApplicationSpec& spec, const GameInfo::Edition& edition,
                 Options options, Settings settings, std::string highScoresFile,
                 std::unique_ptr<GameMode> mode);

protected:
    // Called at the end of OnStart (textures etc.).
    virtual void OnLoadAssets() {}
    // Draws the playfield's contents in playfield coordinates (0..kPlayfieldSize, inside the
    // Begin/End batch). The HUD is drawn afterwards, on top.
    virtual void DrawWorld(Emerald::Renderer2D& r) = 0;
    // The HUD; the default is the shared vector font HUD.
    virtual void DrawHud(Emerald::Renderer2D& r) { m_Game->DrawHud(r); }
    // How the particle effects look (drawn right after DrawWorld); the default is glowing
    // streaks, which suits the vector look.
    [[nodiscard]] virtual Emerald::ParticleDrawOptions GetParticleDrawOptions() const { return {}; }

    // The rules this version runs (the executable knows the concrete type).
    [[nodiscard]] const GameMode& GetMode() const { return *m_Game; }
    [[nodiscard]] GameMode& GetMode() { return *m_Game; }
    [[nodiscard]] const GameInfo::Edition& GetEdition() const { return m_Edition; }
    // <per-user folder>/<name>, or empty if there is no per-user folder.
    [[nodiscard]] std::filesystem::path GetUserFile(std::string_view name) const;
    // Called after every fixed step in which the game was updated (e.g. to save progress).
    virtual void OnGameStepped() {}
    // Whether this version offers the CRT post-process (Options > CRT EFFECT, F9).
    [[nodiscard]] virtual bool HasCrtOption() const { return false; }

    void OnStart() override;
    void OnEvent(const SDL_Event& event) override;
    void OnFixedUpdate(f32 dt) override;
    void OnUpdate(f32 dt) override;
    void OnRender2D(Emerald::Renderer2D& r) override;
    void OnImGui() override;

private:
    // Screens drawn over the game; the last one gets the input. While any is open during a game,
    // the game is frozen (paused); on the title screen the rocks keep drifting behind them.
    enum class Overlay : u8 { TitleMenu, PauseMenu, Options, Controls };
    // The options menu's items, in order (CRT EFFECT only where HasCrtOption).
    enum class OptionItem : u8 {
        MasterVolume,
        SfxVolume,
        Fullscreen,
        VSync,
        ScreenShake,
        Particles,
        Crt,
        Controls,
        Back
    };

    void BindControls();
    void LoadSoundOverrides();
    void LoadHighScores();
    void UpdateBackgroundSounds();
    void PlayGameSounds();
    void UpdatePrompts();
    void ApplyStartScreen();

    void OpenOverlay(Overlay overlay);
    void CloseOverlay();
    void UpdateOverlay(const MenuInput& input);
    void UpdateOptions(MenuAction action);
    void DrawOverlay(Emerald::Renderer2D& r);
    void Pause();
    void QuitToTitle();

    void ApplySettings();
    void SaveSettings();
    void ToggleFullscreen();
    void ToggleCrt();
    void BuildOptionsMenu();
    [[nodiscard]] f32 GetSfxVolume() const
    {
        return static_cast<f32>(m_Settings.SfxVolume) / 100.0f;
    }
    void AddShake(f32 amount);

    [[nodiscard]] static Menu MakeTitleMenu(const GameMode& mode);

    const GameInfo::Edition& m_Edition;
    Options m_Options;
    Settings m_Settings;
    std::filesystem::path m_SettingsFile; // empty = can't save
    std::string m_HighScoresName;
    std::unique_ptr<GameMode> m_Game;
    u32 m_ShipsLost = 0;
    ParticleEffects m_Effects;

    std::vector<Overlay> m_Overlays;
    Menu m_TitleMenu; // START GAME, the mode's extras, OPTIONS, QUIT GAME
    Menu m_PauseMenu{{"RESUME", "OPTIONS", "QUIT TO TITLE", "QUIT GAME"}};
    std::vector<OptionItem> m_OptionItems; // set in OnStart (BuildOptionsMenu)
    Menu m_OptionsMenu{std::vector<std::string>{}};
    RepeatingPress m_MenuUp;
    RepeatingPress m_MenuDown;
    RepeatingPress m_MenuLeft;
    RepeatingPress m_MenuRight;
    f32 m_TitleTime = 0.0f; // seconds on the title screen (pages, blinking)
    f32 m_MenuTime = 0.0f;  // for menu animation
    Prompts m_Prompts;
    PadLabels m_PadLabels;
    std::optional<Emerald::Texture> m_White; // 1 x 1, to darken the game behind menus

    f32 m_Shake = 0.0f; // screen shake amplitude in playfield pixels, decays quickly
    Random m_ShakeRandom{1234};

    Sounds m_Sounds;
    Emerald::VoiceHandle m_ThrustVoice;
    bool m_WasThrusting = false;
    Emerald::VoiceHandle m_SaucerVoice;
    std::optional<SaucerSize> m_SaucerSound; // which siren is playing
    std::filesystem::path m_HighScoresFile;  // empty = can't save
    static constexpr f32 kMusicVolume = 0.35f;
    f32 m_AmbienceVolume = 0.25f;
    Emerald::VoiceHandle m_MusicVoice;
    Emerald::VoiceHandle m_AmbienceVoice;
    bool m_BackgroundStarted = false;
    bool m_WasGameOver = false;
    std::string m_Overrides; // names of the loaded overrides, for the log and the debug panel
};

} // namespace Asteroids
