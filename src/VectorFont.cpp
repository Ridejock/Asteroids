#include "VectorFont.h"

namespace Asteroids::VectorFont {

namespace {

using Emerald::Vec2;

constexpr f32 kGlyphWidth = 4.0f;  // grid units
constexpr f32 kGlyphHeight = 6.0f; // grid units
constexpr f32 kAdvance = 6.0f;     // glyph width + 2 units of spacing

// Each glyph is a list of strokes separated by spaces. A stroke is a polyline written as digit
// pairs "xy" on the 4 x 6 grid (x = 0..4 left to right, y = 0..6 top to bottom). For example
// "7" is "004046": from (0,0) to (4,0) to (4,6) - the top bar, then down the right side.
std::string_view GlyphStrokes(char c)
{
    // clang-format off
    switch (c) {
    case '0': return "0040460600";
    case '1': return "2026";
    case '2': return "004043030646";
    case '3': return "00404606 0343";
    case '4': return "000343 4046";
    case '5': return "400003434606";
    case '6': return "400006464303";
    case '7': return "004046";
    case '8': return "0040460600 0343";
    case '9': return "464000034303";
    case 'A': return "0602204246 0444";
    case 'B': return "0006 003041423303 033344453606";
    case 'C': return "40000646";
    case 'D': return "00204244260600";
    case 'E': return "40000646 0333";
    case 'F': return "400006 0333";
    case 'G': return "400006464323";
    case 'H': return "0006 4046 0343";
    case 'I': return "0040 2026 0646";
    case 'J': return "40460604";
    case 'K': return "0006 400346";
    case 'L': return "000646";
    case 'M': return "0600224046";
    case 'N': return "06004640";
    case 'O': return "0040460600";
    case 'P': return "0600404303";
    case 'Q': return "0040460600 2446";
    case 'R': return "0600404303 1346";
    case 'S': return "400003434606";
    case 'T': return "0040 2026";
    case 'U': return "00064640";
    case 'V': return "002640";
    case 'W': return "0006244640";
    case 'X': return "0046 4006";
    case 'Y': return "002340 2326";
    case 'Z': return "00400646";
    case '-': return "0343";
    case '+': return "0343 2125";
    case '=': return "0242 0444";
    case '_': return "0646";
    case '.': return "2526";
    case ',': return "2516";
    case ':': return "2122 2425";
    case '!': return "2024 2526";
    case '?': return "0040422224 2526";
    case '/': return "0640";
    case '<': return "400346";
    case '>': return "004306";
    case '\'': return "2021";
    case '(': return "30121436";
    case ')': return "10323416";
    default: return "";
    }
    // clang-format on
}

} // namespace

void DrawText(Emerald::Renderer2D& r, std::string_view text, const Vec2& topLeft, f32 height,
              const Emerald::Vec4& color)
{
    const f32 unit = height / kGlyphHeight; // pixels per grid unit
    Vec2 origin = topLeft;
    for (char c : text) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');

        // Walk the strokes: consecutive points of a stroke are joined by lines.
        const std::string_view strokes = GlyphStrokes(c);
        bool hasPrevious = false;
        Vec2 previous;
        for (usize i = 0; i + 1 < strokes.size();) {
            if (strokes[i] == ' ') { // a space starts a new stroke
                hasPrevious = false;
                ++i;
                continue;
            }
            const f32 x = static_cast<f32>(strokes[i] - '0');
            const f32 y = static_cast<f32>(strokes[i + 1] - '0');
            const Vec2 point = origin + Vec2(x, y) * unit;
            if (hasPrevious)
                r.DrawLine(previous, point, color);
            previous = point;
            hasPrevious = true;
            i += 2;
        }
        origin.x += kAdvance * unit;
    }
}

void DrawTextCentered(Emerald::Renderer2D& r, std::string_view text, f32 centerX, f32 top,
                      f32 height, const Emerald::Vec4& color)
{
    DrawText(r, text, {centerX - 0.5f * TextWidth(text, height), top}, height, color);
}

f32 TextWidth(std::string_view text, f32 height)
{
    if (text.empty())
        return 0.0f;
    const f32 unit = height / kGlyphHeight;
    // Every glyph advances kAdvance units; the last one only needs its own width.
    return (static_cast<f32>(text.size() - 1) * kAdvance + kGlyphWidth) * unit;
}

} // namespace Asteroids::VectorFont
