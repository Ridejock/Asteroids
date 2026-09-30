#include "Text.h"

#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <Emerald/Core/Log.h>
#include <Emerald/Core/Paths.h>

#include "VectorFont.h"

namespace Asteroids::Text {

namespace {

using Emerald::Font;
using Emerald::Vec2;
using Emerald::Vec4;

Style s_Style; // the current look (default: line font)

// Where a TTF string goes for a requested capital height.
struct Placement {
    const Font* Source = nullptr;
    f32 Scale = 1.0f;
    f32 OffsetY = 0.0f; // from the capitals' top to DrawString's position (its ascent line)
};

Placement Place(f32 height)
{
    const Font* base = s_Style.Fonts.front();
    // The letter 'H' tells how tall capitals are and where they start below the ascent line.
    const Emerald::Glyph* h = base->FindGlyph('H');
    const f32 capHeight = h ? h->Size.y : base->GetAscent();
    const f32 capTop = h ? -h->Offset.y : base->GetAscent(); // above the baseline

    Placement place{.Source = base};
    if (s_Style.PixelSizes) {
        // Whole multiples only; rounding a little down keeps text inside layouts made for the
        // line font (whose letters are as wide as they are tall).
        place.Scale = Emerald::Max(1.0f, std::floor(height / base->GetSize() + 0.45f));
    } else {
        // The em size that makes capitals `height` tall, from the smallest bake that is big
        // enough (scaling a bake down a lot would alias: there are no mipmaps).
        const f32 em = height / capHeight * base->GetSize();
        for (const Font* font : s_Style.Fonts) {
            place.Source = font;
            if (font->GetSize() >= em * 0.95f)
                break;
        }
        place.Scale = em / place.Source->GetSize();
    }
    // Center the capitals in the requested height (a pixel font's snapped size may differ).
    const f32 k = place.Scale * place.Source->GetSize() / base->GetSize(); // base px -> units
    const f32 drawnCap = capHeight * k;
    place.OffsetY = 0.5f * (height - drawnCap) - (base->GetAscent() - capTop) * k;
    return place;
}

std::string Upper(std::string_view text)
{
    std::string upper(text);
    for (char& c : upper)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return upper;
}

// Draws with the TTF style; `at.x` is the left edge, or the center for TextAlign::Center.
void DrawFont(Emerald::Renderer2D& r, std::string_view text, const Vec2& at, f32 height,
              const Vec4& color, Emerald::TextAlign align, bool halo)
{
    const Placement place = Place(height);
    const std::string upper = Upper(text);
    const Vec2 position{at.x, at.y + place.OffsetY};

    // What to draw where: the whole string, or (Monospace) each letter centered in its cell of
    // the line font's grid (a glyph 2/3 of the advance wide, so its center is 1/3 in).
    struct Piece {
        std::string_view Text;
        Vec2 Position;
        Emerald::TextAlign Align;
    };
    std::vector<Piece> pieces;
    if (s_Style.Monospace) {
        f32 left = position.x;
        if (align == Emerald::TextAlign::Center)
            left -= 0.5f * VectorFont::TextWidth(upper, height);
        for (usize i = 0; i < upper.size(); ++i) {
            const f32 center = left + height * (static_cast<f32>(i) + 1.0f / 3.0f);
            pieces.push_back({std::string_view(upper).substr(i, 1),
                              {center, position.y},
                              Emerald::TextAlign::Center});
        }
    } else {
        pieces.push_back({upper, position, align});
    }

    const Emerald::BlendMode previous = r.GetBlendMode();
    if (s_Style.Additive)
        r.SetBlendMode(Emerald::BlendMode::Additive);
    if (halo && s_Style.Halo > 0.0f) {
        // Eight faint copies around the letters, a little wider for bigger text.
        const f32 radius = Emerald::Max(1.0f, 0.07f * height);
        const Vec4 glow{0.55f, 0.8f, 1.0f, color.w * s_Style.Halo};
        for (i32 i = 0; i < 8; ++i) {
            const f32 a = Emerald::TwoPi * static_cast<f32>(i) / 8.0f;
            const Vec2 offset = Vec2(std::cos(a), std::sin(a)) * radius;
            for (const Piece& piece : pieces)
                r.DrawString(*place.Source, piece.Text, piece.Position + offset, glow, place.Scale,
                             piece.Align);
        }
    }
    for (const Piece& piece : pieces)
        r.DrawString(*place.Source, piece.Text, piece.Position, color, place.Scale, piece.Align);
    r.SetBlendMode(previous);
}

} // namespace

void SetStyle(Style style)
{
    s_Style = std::move(style);
}

bool UsesLineFont()
{
    return s_Style.Fonts.empty();
}

void Draw(Emerald::Renderer2D& r, std::string_view text, const Vec2& topLeft, f32 height,
          const Vec4& color, bool halo)
{
    if (UsesLineFont())
        VectorFont::DrawText(r, text, topLeft, height, color);
    else
        DrawFont(r, text, topLeft, height, color, Emerald::TextAlign::Left, halo);
}

void DrawCentered(Emerald::Renderer2D& r, std::string_view text, f32 centerX, f32 top, f32 height,
                  const Vec4& color, bool halo)
{
    if (UsesLineFont())
        VectorFont::DrawTextCentered(r, text, centerX, top, height, color);
    else
        DrawFont(r, text, {centerX, top}, height, color, Emerald::TextAlign::Center, halo);
}

f32 Width(std::string_view text, f32 height)
{
    if (UsesLineFont() || s_Style.Monospace)
        return VectorFont::TextWidth(text, height);
    const Placement place = Place(height);
    return place.Source->MeasureText(Upper(text), place.Scale).x;
}

std::vector<Emerald::Font> LoadFonts(SDL_GPUDevice* device, std::string_view file,
                                     std::span<const f32> sizes,
                                     const Emerald::FontOptions& options)
{
    const std::filesystem::path path = Emerald::Paths::GetBasePath() / "assets" / "fonts" / file;
    std::vector<Font> fonts;
    for (const f32 size : sizes) {
        Emerald::FontOptions bake = options;
        bake.Size = size;
        std::optional<Font> font = Font::Load(device, path, bake);
        if (!font) {
            EM_ERROR("Font {} missing or unreadable; using the line font", path.string());
            return {};
        }
        fonts.push_back(std::move(*font));
    }
    return fonts;
}

std::vector<const Emerald::Font*> Pointers(const std::vector<Emerald::Font>& fonts)
{
    std::vector<const Font*> pointers;
    for (const Font& font : fonts)
        pointers.push_back(&font);
    return pointers;
}

} // namespace Asteroids::Text
