#include "Menu.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "Playfield.h"
#include "VectorFont.h"

namespace Asteroids {

namespace {

constexpr f32 kTitleHeight = 40.0f;
constexpr f32 kItemHeight = 24.0f;
constexpr f32 kRowSpacing = 46.0f;
constexpr usize kLabelWidth = 16; // characters; labels are padded so values line up
constexpr usize kValueWidth = 6;

const Emerald::Vec4 kBright{0.95f, 0.97f, 1.0f, 1.0f};
const Emerald::Vec4 kDim{0.95f, 0.97f, 1.0f, 0.45f};

} // namespace

MenuAction Menu::Update(const MenuInput& input)
{
    const usize count = m_Items.size();
    if (count == 0)
        return MenuAction::None;
    if (input.Up != input.Down)
        m_Selected = input.Up ? (m_Selected + count - 1) % count : (m_Selected + 1) % count;
    if (input.Back)
        return MenuAction::Back;
    if (input.Confirm)
        return MenuAction::Confirm;
    if (input.Left != input.Right)
        return input.Left ? MenuAction::Left : MenuAction::Right;
    return MenuAction::None;
}

void Menu::Draw(Emerald::Renderer2D& r, std::string_view title, f32 top,
                std::span<const std::string> values, f32 time) const
{
    const f32 centerX = kPlayfieldCenter.x;
    VectorFont::DrawTextCentered(r, title, centerX, top, kTitleHeight, kBright);

    const bool anyValues = !values.empty();
    for (usize i = 0; i < m_Items.size(); ++i) {
        const std::string value = i < values.size() ? values[i] : std::string();
        // Rows with values: "LABEL........  VALUE" at fixed widths (the font is monospaced, so
        // the columns line up); without: just the label, centered.
        std::string row = m_Items[i];
        if (anyValues) {
            row.resize(kLabelWidth, ' ');
            row += std::string(kValueWidth - std::min(value.size(), kValueWidth), ' ') + value;
        }
        const f32 y = top + kTitleHeight + 40.0f + kRowSpacing * static_cast<f32>(i);
        const bool selected = i == m_Selected;
        VectorFont::DrawTextCentered(r, row, centerX, y, kItemHeight, selected ? kBright : kDim);
        if (selected) {
            // Pulsing arrows either side of the selected row.
            const f32 half = 0.5f * VectorFont::TextWidth(row, kItemHeight);
            const f32 nudge = 4.0f * std::sin(time * 6.0f);
            VectorFont::DrawText(r, ">", {centerX - half - 44.0f + nudge, y}, kItemHeight, kBright);
            VectorFont::DrawText(r, "<", {centerX + half + 28.0f - nudge, y}, kItemHeight, kBright);
        }
    }
}

} // namespace Asteroids
