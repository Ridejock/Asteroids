#include "Screens.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

#include "Game.h"
#include "GameInfo.h"
#include "Playfield.h"
#include "VectorFont.h"

namespace Asteroids {

namespace {

const Vec4 kText{0.95f, 0.97f, 1.0f, 1.0f};
const Vec4 kDim{0.95f, 0.97f, 1.0f, 0.45f};
const Vec4 kFaint{0.95f, 0.97f, 1.0f, 0.3f};

std::string PadRight(std::string text, usize width)
{
    if (text.size() < width)
        text.resize(width, ' ');
    return text;
}

} // namespace

namespace TitleScreen {

Page PageAt(f32 seconds, bool haveHighScores)
{
    const u32 pageCount = haveHighScores ? 3 : 2;
    const u32 index = static_cast<u32>(seconds / kPageSeconds) % pageCount;
    if (index == 0)
        return Page::PressStart;
    return haveHighScores && index == 1 ? Page::HighScores : Page::Controls;
}

void DrawLogo(Emerald::Renderer2D& r, std::string_view title, f32 centerY, f32 maxWidth, f32 time)
{
    const f32 height =
        std::min(110.0f, maxWidth / std::max(VectorFont::TextWidth(title, 1.0f), 0.01f));
    const f32 left = kPlayfieldCenter.x - 0.5f * VectorFont::TextWidth(title, height);
    const Vec2 topLeft{left, centerY - 0.5f * height};

    // Brightness: a slow breathing plus, now and then, a quick dip like an old tube.
    const f32 dip = std::fmod(time, 5.3f) < 0.08f ? 0.55f : 1.0f;
    const f32 brightness = (0.88f + 0.12f * std::sin(time * 1.7f)) * dip;

    // Glow: faint copies in a ring around the letters, then the thick bright core (the font's
    // lines are 1 pixel, so the core is drawn 3 x 3 times, offset by a pixel).
    constexpr i32 kGlowCopies = 12;
    for (i32 i = 0; i < kGlowCopies; ++i) {
        const f32 a = Emerald::TwoPi * static_cast<f32>(i) / static_cast<f32>(kGlowCopies);
        for (const auto& [radius, alpha] : {std::pair{2.5f, 0.16f}, {5.0f, 0.08f}, {8.0f, 0.04f}}) {
            const Vec4 glow{0.45f, 0.8f, 1.0f, alpha * brightness};
            VectorFont::DrawText(r, title, topLeft + Vec2(std::cos(a), std::sin(a)) * radius,
                                 height, glow);
        }
    }
    const Vec4 core{0.9f, 0.97f, 1.0f, brightness};
    for (i32 y = -1; y <= 1; ++y)
        for (i32 x = -1; x <= 1; ++x)
            VectorFont::DrawText(r, title, topLeft + Vec2(static_cast<f32>(x), static_cast<f32>(y)),
                                 height, core);
}

void DrawCover(Emerald::Renderer2D& r, f32 time)
{
    DrawLogo(r, GameInfo::kTitle, kPlayfieldCenter.y - 30.0f, 800.0f, time);
    VectorFont::DrawTextCentered(r, "A VECTOR ARCADE SHOOTER", kPlayfieldCenter.x,
                                 kPlayfieldCenter.y + 70.0f, 24.0f, kDim);
}

void Draw(Emerald::Renderer2D& r, const Game& game, Page page, std::string_view startPrompt,
          const PadLabels& pad, f32 time)
{
    const f32 centerX = kPlayfieldCenter.x;
    DrawLogo(r, GameInfo::kTitle, 190.0f, 980.0f, time);

    switch (page) {
    case Page::PressStart:
        if (std::fmod(time, 1.2f) < 0.8f)
            VectorFont::DrawTextCentered(r, startPrompt, centerX, 400.0f, 28.0f, kText);
        VectorFont::DrawTextCentered(r, "ESC: MENU", centerX, 470.0f, 18.0f, kDim);
        VectorFont::DrawTextCentered(r, "MADE WITH EMERALD AND SDL3", centerX, 600.0f, 16.0f,
                                     kFaint);
        break;
    case Page::HighScores:
        game.DrawHighScoreTable(r, 300.0f);
        break;
    case Page::Controls:
        DrawControls(r, 300.0f, pad);
        break;
    }

    // Footer: copyright on the left, version on the right.
    VectorFont::DrawText(r, GameInfo::kCopyright, {40.0f, 684.0f}, 14.0f, kFaint);
    const std::string version = std::string("V") + GameInfo::kVersion;
    VectorFont::DrawText(r, version,
                         {kPlayfieldSize.x - 40.0f - VectorFont::TextWidth(version, 14.0f), 684.0f},
                         14.0f, kFaint);
}

} // namespace TitleScreen

void DrawControls(Emerald::Renderer2D& r, f32 top, const PadLabels& pad)
{
    struct Row {
        const char* Action;
        const char* Keys;
        std::string Pad;
    };
    const bool hasPad = !pad.Fire.empty();
    const Row rows[] = {
        {"ROTATE", "A D / LEFT RIGHT", "LEFT STICK"},
        {"THRUST", "W / UP", pad.Thrust},
        {"FIRE", "SPACE", pad.Fire},
        {"HYPERSPACE", "SHIFT", pad.Hyperspace},
        {"PAUSE", "ESC / P", pad.Pause},
        {"FULLSCREEN", "F11 / ALT+ENTER", ""},
        {"MUTE", "M", pad.Mute},
    };

    const f32 centerX = kPlayfieldCenter.x;
    VectorFont::DrawTextCentered(r, "CONTROLS", centerX, top, 24.0f, kText);
    // Fixed-width columns (monospaced font), centered as a block.
    for (usize i = 0; i < std::size(rows); ++i) {
        std::string line = PadRight(rows[i].Action, 12) + PadRight(rows[i].Keys, 16);
        line += hasPad ? PadRight(rows[i].Pad, 10) : std::string();
        VectorFont::DrawTextCentered(r, line, centerX, top + 50.0f + 34.0f * static_cast<f32>(i),
                                     18.0f, kText);
    }
}

} // namespace Asteroids
