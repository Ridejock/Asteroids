#pragma once

#include <string>

#include <Emerald/Core/Defines.h>
#include <Emerald/Renderer/Renderer2D.h>

#include "GameMode.h"
#include "HighScores.h"

namespace Asteroids {

// Entering three initials after a high score, like the arcade: Up/Down change the letter,
// Confirm goes to the next one, Back to the previous one.
class InitialsEntry {
public:
    // Starts over: the first letter is A, the others still blank.
    void Begin();
    // True once the third letter was confirmed (GetInitials is then final).
    bool Update(const GameInput& input);
    [[nodiscard]] const std::string& GetInitials() const { return m_Initials; }
    // The "enter your initials" screen; `time` makes the current underline blink.
    void Draw(Emerald::Renderer2D& r, const Prompts& prompts, f32 time) const;

private:
    std::string m_Initials = "A  "; // always 3 characters
    usize m_Cursor = 0;             // which of them is being changed
};

// The top 10 at `top` (playfield y). Row `highlight` blinks (kMaxEntries = none). Entries with
// a note (e.g. how far a run got) get it as a fourth column.
void DrawHighScoreTable(Emerald::Renderer2D& r, const HighScoreTable& table, f32 top,
                        usize highlight, f32 time);

// Right-aligns `text` in `width` characters (the font is monospaced, so columns line up).
[[nodiscard]] std::string PadLeft(std::string text, usize width);

inline const Vec4 kTextColor{0.95f, 0.97f, 1.0f, 1.0f};
inline const Vec4 kDimTextColor{0.95f, 0.97f, 1.0f, 0.35f};

} // namespace Asteroids
