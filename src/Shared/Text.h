#pragma once

#include <span>
#include <string_view>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include <Emerald/Core/Defines.h>
#include <Emerald/Math/Vec2.h>
#include <Emerald/Math/Vec4.h>
#include <Emerald/Renderer/Font.h>
#include <Emerald/Renderer/Renderer2D.h>

// All of the games' text (menus, HUD, screens) goes through here, so each executable picks its
// own look: the built-in line font (VectorFont, the default) or a TTF font baked by Emerald.
// Sizes are the height of capital letters in playfield units, whatever the font; text is drawn
// in capitals like the line font.
namespace Asteroids::Text {

// ' ' to '_': digits, capitals and the punctuation the games use (text is drawn in capitals).
inline constexpr Emerald::GlyphRange kCapitalsAndSymbols{32, 64};

struct Style {
    // Bakes of one TTF font at increasing em sizes (not owned; they must outlive their use).
    // Empty: the line font.
    std::vector<const Emerald::Font*> Fonts;
    // Pixel font: sizes snap to whole multiples of Fonts[0], so its pixels stay square and sharp.
    bool PixelSizes = false;
    // Lay proportional letters out on the line font's grid (each centered in a cell as wide as
    // the text is tall): layouts made for the line font keep their columns and widths.
    bool Monospace = false;
    // Additive blending: text lights up like the vector lines.
    bool Additive = false;
    // Alpha of a faint bluish halo around each letter (0 = none), like the lines' glow.
    f32 Halo = 0.0f;
};

// Installs a look. Call SetStyle({}) before the fonts it points at are destroyed.
void SetStyle(Style style);
[[nodiscard]] bool UsesLineFont();

// `topLeft` is the top-left of the capitals. `halo` = false skips the style's halo (for text
// that draws its own glow, like the title logo).
void Draw(Emerald::Renderer2D& r, std::string_view text, const Emerald::Vec2& topLeft, f32 height,
          const Emerald::Vec4& color, bool halo = true);
// Same, horizontally centered on `centerX`.
void DrawCentered(Emerald::Renderer2D& r, std::string_view text, f32 centerX, f32 top, f32 height,
                  const Emerald::Vec4& color, bool halo = true);
// Width of `text` drawn at `height`.
[[nodiscard]] f32 Width(std::string_view text, f32 height);

// Loads assets/fonts/<file> (next to the executable) once per em size in `sizes`, the rest of
// `options` shared. Empty if any fails (logged), so the caller keeps the line font. The fonts
// hold GPU textures: keep them in the app and release them before the device.
[[nodiscard]] std::vector<Emerald::Font> LoadFonts(SDL_GPUDevice* device, std::string_view file,
                                                   std::span<const f32> sizes,
                                                   const Emerald::FontOptions& options);
// Style::Fonts for fonts loaded with LoadFonts.
[[nodiscard]] std::vector<const Emerald::Font*> Pointers(const std::vector<Emerald::Font>& fonts);

} // namespace Asteroids::Text
