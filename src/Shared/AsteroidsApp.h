#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <Emerald/Emerald.h>

#include "Game.h"
#include "Sounds.h"

namespace Asteroids {

// Command line options (handy for automated runs): --frames N quits after N frames,
// --screenshot out.png saves the last frame, --seed N makes the asteroids repeatable.
// For testing: --saucer large|small spawns a saucer right away, --game-over SCORE ends the game
// at once with that score (to try the initials entry and the table).
struct Options {
    u64 Frames = 0;
    std::string ScreenshotPath;
    u32 Seed = 0; // 0 = random
    std::string Saucer;
    std::optional<u32> GameOverScore;
};

[[nodiscard]] Options ParseOptions(i32 argc, char** argv);

// The window, log file etc. every version uses; `title` and `logFile` differ per version. The
// caller still sets ShaderFormats (EMERALD_SHADER_FORMATS is defined for the executable).
[[nodiscard]] Emerald::ApplicationSpec MakeSpec(const Options& options, const char* title,
                                                const char* logFile);

// Up/Down auto-repeat while held (for cycling through letters): one step on every press (even a
// very short tap), more after a short delay while the button stays down.
class RepeatingPress {
public:
    bool Update(bool pressed, bool down, f32 dt);

private:
    static constexpr f32 kDelay = 0.4f;     // seconds before repeating starts
    static constexpr f32 kInterval = 0.09f; // seconds between repeats
    f32 m_HeldTime = 0.0f;
    f32 m_NextRepeat = kDelay;
};

// Everything both versions share: controls (input actions), the fixed-step game update, sounds
// (events, loops, overrides), high score loading/saving, prompts, fitting the playfield into the
// window, and the debug panel. A version only says how the world looks (DrawWorld) and may load
// its own assets (OnLoadAssets).
class AsteroidsApp : public Emerald::Application {
public:
    // `highScoresFile`: file name in the per-user folder, so each version keeps its own table.
    AsteroidsApp(const Emerald::ApplicationSpec& spec, Options options, std::string highScoresFile);

protected:
    // Called at the end of OnStart (textures etc.).
    virtual void OnLoadAssets() {}
    // Draws the playfield's contents in playfield coordinates (0..kPlayfieldSize, inside the
    // Begin/End batch). The HUD is drawn afterwards, on top.
    virtual void DrawWorld(Emerald::Renderer2D& r) = 0;
    // The HUD; the default is the shared vector font HUD.
    virtual void DrawHud(Emerald::Renderer2D& r) { m_Game.DrawHud(r); }

    [[nodiscard]] const Game& GetGame() const { return m_Game; }

    void OnStart() override;
    void OnFixedUpdate(f32 dt) override;
    void OnUpdate(f32 dt) override;
    void OnRender2D(Emerald::Renderer2D& r) override;
    void OnImGui() override;

private:
    void BindControls();
    void LoadSoundOverrides();
    void LoadHighScores();
    void UpdateBackgroundSounds();
    void PlayGameSounds();
    void UpdatePrompts();

    Options m_Options;
    std::string m_HighScoresName;
    Game m_Game;
    u32 m_ShipsLost = 0;
    Sounds m_Sounds;
    Emerald::VoiceHandle m_ThrustVoice;
    bool m_WasThrusting = false;
    Emerald::VoiceHandle m_SaucerVoice;
    std::optional<SaucerSize> m_SaucerSound; // which siren is playing
    RepeatingPress m_MenuUp;
    RepeatingPress m_MenuDown;
    std::filesystem::path m_HighScoresFile; // empty = can't save
    static constexpr f32 kMusicVolume = 0.35f;
    f32 m_AmbienceVolume = 0.25f;
    Emerald::VoiceHandle m_MusicVoice;
    Emerald::VoiceHandle m_AmbienceVoice;
    bool m_BackgroundStarted = false;
    bool m_WasGameOver = false;
    std::string m_Overrides; // names of the loaded overrides, for the log and the debug panel
};

} // namespace Asteroids
