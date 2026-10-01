#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Vec2.h>
#include <Emerald/Renderer/Renderer2D.h>

#include "HighScores.h"
#include "Saucer.h"
#include "Ship.h"

namespace Asteroids {

// Player input for one fixed step. The "pressed" flags are true only in the step the key went
// down (Emerald's Input takes care of that), so holding Space does not auto-fire.
struct GameInput {
    ShipControls Ship;
    bool FirePressed = false;
    bool FireHeld = false; // auto-fire (the roguelike; the classic game fires per press)
    bool HyperspacePressed = false;
    bool StartPressed = false;
    // Menu input for entering initials and a mode's own screens (Main repeats the directions
    // while held).
    bool MenuUpPressed = false;
    bool MenuDownPressed = false;
    bool MenuLeftPressed = false;
    bool MenuRightPressed = false;
    bool ConfirmPressed = false;
    bool BackPressed = false;     // initials: previous letter
    bool MenuBackPressed = false; // leave a mode's screen (only sent while IsInMenuScreen)
};

// Texts that depend on the device (keyboard vs. gamepad and its button labels); set by Main.
struct Prompts {
    std::string Start = "PRESS ENTER";    // game over screen
    std::string Confirm = "FIRE / ENTER"; // initials entry: next letter
    std::string Back = "BACKSPACE";       // initials entry: previous letter
};

// Sounds the game asks for. The game only reports what happened; Main.cpp plays the sounds, so
// the game logic stays free of audio (and testable without a device).
enum class SoundEvent : u8 {
    Fire,
    ExplosionLarge,
    ExplosionMedium,
    ExplosionSmall,
    ShipExplosion,
    ExtraLife,
    Hyperspace,
    BeatHigh, // the two alternating heartbeat tones
    BeatLow,
    SaucerFire,
    SaucerExplosion,
    // Only the roguelike (pixel version) uses these.
    ShieldHit,     // the shield absorbed a hit
    Upgrade,       // an upgrade was picked
    Purchase,      // something bought in the hangar
    Pickup,        // scrap collected
    MetalHit,      // a shot bounced off armor (metal rock, boss)
    Blast,         // an explosive rock or the hyperspace blast went off
    MissileLaunch, // a homing missile left the ship
    BossAlarm,     // a boss appears
    BossExplosion, // a boss is destroyed
};

// Something that makes a noise. The app also spawns its particle effects from these.
struct GameSound {
    SoundEvent Event;
    f32 Pan = 0.0f;  // -1 (left edge) .. +1 (right edge), from where it happened
    Vec2 Position{}; // where it happened
    Vec2 Velocity{}; // of the thing that made it (e.g. a rock that broke), if it moved
};

// What AsteroidsApp runs: the rules of one kind of game. The vector version plays the classic
// arcade game (Game), the pixel version the roguelike (RogueGame). The app owns the window, the
// menus, sound and effects, and talks to the game only through this interface; the executable
// draws the world from the concrete class.
class GameMode {
public:
    virtual ~GameMode() = default;

    // The title screen's background (the app draws the logo and menus on top).
    virtual void ShowTitle() = 0;
    [[nodiscard]] virtual bool IsOnTitle() const = 0;
    virtual void StartGame() = 0;
    virtual void Update(const GameInput& input, f32 dt) = 0;

    // True unless a game is being played (title, initials entry, game over / results).
    [[nodiscard]] virtual bool IsGameOver() const = 0;
    // On the final screen of a game, where Start begins the next one.
    [[nodiscard]] virtual bool IsOnGameOverScreen() const = 0;
    // A screen of the mode's own that takes menu input (e.g. a shop): the app then neither
    // pauses nor opens its menus, and sends MenuBackPressed instead.
    [[nodiscard]] virtual bool IsInMenuScreen() const { return false; }
    // Extra title menu entries, shown after START GAME (e.g. "HANGAR"); OpenTitleExtra(i) is
    // called when the i-th one is chosen.
    [[nodiscard]] virtual std::vector<std::string> GetTitleExtras() const { return {}; }
    virtual void OpenTitleExtra(usize /*index*/) {}

    // Score, banners and the mode's screens, in playfield coordinates.
    virtual void DrawHud(Emerald::Renderer2D& r) const = 0;
    // The high score table at `top` (playfield y), for the title screen.
    virtual void DrawHighScoreTable(Emerald::Renderer2D& r, f32 top) const = 0;

    [[nodiscard]] virtual const Ship& GetShip() const = 0;
    [[nodiscard]] virtual bool IsThrusting() const = 0;
    // A saucer on screen (the app loops its siren).
    [[nodiscard]] virtual std::optional<SaucerSize> GetSaucerSize() const = 0;
    // Counts destroyed ships, so the app can react (rumble) when it changes.
    [[nodiscard]] virtual u32 GetShipsLost() const = 0;
    // Sounds requested since the last ClearSounds (the app calls both after every Update).
    [[nodiscard]] virtual const std::vector<GameSound>& GetSounds() const = 0;
    virtual void ClearSounds() = 0;
    virtual void SetPrompts(Prompts prompts) = 0;

    // High scores: the app loads them at startup and saves them whenever they change.
    virtual void SetHighScores(HighScoreTable table) = 0;
    [[nodiscard]] virtual const HighScoreTable& GetHighScores() const = 0;
    [[nodiscard]] virtual bool ConsumeHighScoresChanged() = 0;

    // For the debug panel.
    [[nodiscard]] virtual u32 GetScore() const = 0;
    [[nodiscard]] virtual u32 GetWave() const = 0;
    [[nodiscard]] virtual usize GetAsteroidCount() const = 0;
    // Testing helpers (command line / debug panel).
    virtual void SpawnSaucer(SaucerSize size) = 0;
    virtual void ForceGameOver(u32 score) = 0;
    // A --screen name the mode handles itself (e.g. "hangar"); false if it doesn't know it.
    virtual bool OpenDebugScreen(std::string_view /*name*/) { return false; }
    // The ROCK BOUNCE option: rocks bounce off one another (see RockBounce.h).
    virtual void SetRockBounce(bool /*on*/) {}
};

} // namespace Asteroids
