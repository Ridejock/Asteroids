#pragma once

#include <string>
#include <string_view>

#include <Emerald/Core/Defines.h>
#include <Emerald/Renderer/Renderer2D.h>

namespace Asteroids {

class Game;

// Button names on the connected gamepad for the controls page (empty = no pad connected).
struct PadLabels {
    std::string Fire;       // e.g. "A" or "CROSS"
    std::string Hyperspace; // e.g. "Y"
    std::string Thrust;     // e.g. "RT"
    std::string Pause;      // e.g. "MENU", "OPTIONS", "+"
    std::string Mute;       // e.g. "VIEW"
};

// Everything on the title screen that is not the drifting rocks (those are the Game in its
// Attract state). Pages change every few seconds while nobody presses anything.
namespace TitleScreen {

enum class Page : u8 { PressStart, HighScores, Controls };

inline constexpr f32 kPageSeconds = 7.0f;

// Which page shows `seconds` after the title screen appeared (skips the high scores while the
// table is empty).
[[nodiscard]] Page PageAt(f32 seconds, bool haveHighScores);

// The game's name in big vector letters with a soft glow and a slight flicker, centered on
// `centerY`, at most `maxWidth` wide.
void DrawLogo(Emerald::Renderer2D& r, std::string_view title, f32 centerY, f32 maxWidth, f32 time);

// The store page cover: the logo (narrow enough for a 630 x 500 crop of the playfield's middle)
// and a tagline, nothing else.
void DrawCover(Emerald::Renderer2D& r, f32 time);

// The whole overlay: logo, the current page, credits and version. `startPrompt` is e.g.
// "PRESS ENTER" or "PRESS START / ENTER".
void Draw(Emerald::Renderer2D& r, const Game& game, Page page, std::string_view startPrompt,
          const PadLabels& pad, f32 time);

} // namespace TitleScreen

// The controls reference (title screen page, and the options menu's CONTROLS entry), with a
// gamepad column when a pad is connected.
void DrawControls(Emerald::Renderer2D& r, f32 top, const PadLabels& pad);

} // namespace Asteroids
