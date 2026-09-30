#pragma once

#include <span>
#include <string>
#include <utility>
#include <vector>

#include <Emerald/Core/Defines.h>
#include <Emerald/Renderer/Renderer2D.h>

namespace Asteroids {

// Menu input for one fixed step (true = pressed this step; Up/Down/Left/Right auto-repeat).
struct MenuInput {
    bool Up = false;
    bool Down = false;
    bool Left = false;
    bool Right = false;
    bool Confirm = false;
    bool Back = false;
};

// What the player did with the selected item this step.
enum class MenuAction : u8 { None, Confirm, Left, Right, Back };

// A vertical list of items drawn with the vector font; Up/Down move the selection (wrapping
// around), everything else is reported to the owner, which knows what the items do:
//
//   Menu pause({"RESUME", "OPTIONS", "QUIT GAME"});
//   if (pause.Update(input) == MenuAction::Confirm && pause.GetSelected() == 0) Resume();
//
// Items can show a value on the right (e.g. "ON", "80"), passed to Draw every frame.
class Menu {
public:
    explicit Menu(std::vector<std::string> items) : m_Items(std::move(items)) {}

    MenuAction Update(const MenuInput& input);

    [[nodiscard]] usize GetSelected() const { return m_Selected; }
    void SetSelected(usize index) { m_Selected = index < m_Items.size() ? index : 0; }
    [[nodiscard]] usize GetCount() const { return m_Items.size(); }

    // Draws `title` and the items centered on the playfield, starting at `top`. `values[i]` is
    // item i's value (empty = none; missing entries count as empty). `time` animates the
    // selection marker.
    void Draw(Emerald::Renderer2D& r, std::string_view title, f32 top,
              std::span<const std::string> values, f32 time) const;

private:
    std::vector<std::string> m_Items;
    usize m_Selected = 0;
};

} // namespace Asteroids
