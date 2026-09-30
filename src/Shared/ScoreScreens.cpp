#include "ScoreScreens.h"

#include <cmath>
#include <string_view>

#include "Playfield.h"
#include "VectorFont.h"

namespace Asteroids {

namespace {

// Letters for the initials, in the order Up steps through them.
constexpr std::string_view kInitialsLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

} // namespace

std::string PadLeft(std::string text, usize width)
{
    return text.size() < width ? std::string(width - text.size(), ' ') + text : text;
}

void InitialsEntry::Begin()
{
    m_Initials = "A  ";
    m_Cursor = 0;
}

bool InitialsEntry::Update(const GameInput& input)
{
    // Up/Down step through A..Z and space (wrapping around).
    char& letter = m_Initials[m_Cursor];
    if (input.MenuUpPressed != input.MenuDownPressed) {
        const usize count = kInitialsLetters.size();
        usize index = kInitialsLetters.find(letter);
        if (index == std::string_view::npos)
            index = 0;
        index = input.MenuUpPressed ? (index + 1) % count : (index + count - 1) % count;
        letter = kInitialsLetters[index];
    }

    if (input.ConfirmPressed) {
        // Next letter (starting at A), or done after the third.
        if (++m_Cursor < HighScoreTable::kInitialsLength) {
            m_Initials[m_Cursor] = 'A';
        } else {
            m_Cursor = HighScoreTable::kInitialsLength - 1;
            return true;
        }
    } else if (input.BackPressed && m_Cursor > 0) {
        m_Initials[m_Cursor] = ' '; // blank again, like before we got here
        --m_Cursor;
    }
    return false;
}

void InitialsEntry::Draw(Emerald::Renderer2D& r, const Prompts& prompts, f32 time) const
{
    const f32 centerX = kPlayfieldCenter.x;
    VectorFont::DrawTextCentered(r, "YOUR SCORE IS ONE OF THE TEN BEST", centerX, 140.0f, 24.0f,
                                 kTextColor);
    VectorFont::DrawTextCentered(r, "PLEASE ENTER YOUR INITIALS", centerX, 185.0f, 24.0f,
                                 kTextColor);

    // Three big letters over underlines; the one being changed blinks its underline.
    constexpr f32 kLetterHeight = 60.0f;
    constexpr f32 kSlotSpacing = 80.0f;
    const f32 letterWidth = VectorFont::TextWidth("A", kLetterHeight);
    for (usize i = 0; i < HighScoreTable::kInitialsLength; ++i) {
        const f32 x = centerX + (static_cast<f32>(i) - 1.0f) * kSlotSpacing - 0.5f * letterWidth;
        if (i <= m_Cursor)
            VectorFont::DrawText(r, std::string(1, m_Initials[i]), {x, 290.0f}, kLetterHeight,
                                 kTextColor);
        const bool current = i == m_Cursor;
        if (!current || std::fmod(time * 3.0f, 2.0f) < 1.4f)
            r.DrawLine({x, 368.0f}, {x + letterWidth, 368.0f},
                       current ? kTextColor : kDimTextColor);
    }

    VectorFont::DrawTextCentered(r, "UP / DOWN: CHANGE LETTER", centerX, 450.0f, 20.0f, kTextColor);
    VectorFont::DrawTextCentered(r, prompts.Confirm + ": NEXT", centerX, 490.0f, 20.0f, kTextColor);
    VectorFont::DrawTextCentered(r, prompts.Back + ": BACK", centerX, 530.0f, 20.0f, kTextColor);
}

void DrawHighScoreTable(Emerald::Renderer2D& r, const HighScoreTable& table, f32 top,
                        usize highlight, f32 time)
{
    const f32 centerX = kPlayfieldCenter.x;
    VectorFont::DrawTextCentered(r, "HIGH SCORES", centerX, top, 24.0f, kTextColor);

    // " 1.  ABC   12340": every row has the same length, so the columns line up when centered.
    const std::vector<HighScore>& entries = table.GetEntries();
    usize noteWidth = 0;
    for (const HighScore& e : entries)
        noteWidth = std::max(noteWidth, e.Note.size());
    for (usize i = 0; i < entries.size(); ++i) {
        std::string row = PadLeft(std::to_string(i + 1) + ".", 3) + "  " + entries[i].Initials +
                          "  " + PadLeft(std::to_string(entries[i].Score), 6);
        if (noteWidth > 0)
            row += "  " + entries[i].Note + std::string(noteWidth - entries[i].Note.size(), ' ');
        // The entry just made blinks.
        if (i == highlight && std::fmod(time * 3.0f, 2.0f) >= 1.4f)
            continue;
        VectorFont::DrawTextCentered(r, row, centerX, top + 50.0f + 32.0f * static_cast<f32>(i),
                                     20.0f, kTextColor);
    }
}

} // namespace Asteroids
