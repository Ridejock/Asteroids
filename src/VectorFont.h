#pragma once

#include <string_view>

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Vec2.h>
#include <Emerald/Math/Vec4.h>
#include <Emerald/Renderer/Renderer2D.h>

namespace Asteroids::VectorFont {

// A tiny stroke font made of straight lines, in the spirit of the arcade original: every glyph
// is drawn on a 4 x 6 grid with Renderer2D lines, no font files or textures involved.
// Supports A-Z (lower case is drawn as upper case), 0-9, space and '-'; anything else is blank.

// Draws `text` with its top-left corner at `topLeft`; glyphs are `height` pixels tall.
void DrawText(Emerald::Renderer2D& r, std::string_view text, const Emerald::Vec2& topLeft,
              f32 height, const Emerald::Vec4& color);

// Same, horizontally centered on `centerX`.
void DrawTextCentered(Emerald::Renderer2D& r, std::string_view text, f32 centerX, f32 top,
                      f32 height, const Emerald::Vec4& color);

// Width in pixels of `text` drawn at `height`.
[[nodiscard]] f32 TextWidth(std::string_view text, f32 height);

} // namespace Asteroids::VectorFont
