#pragma once

#include <Emerald/Assets/Image.h>
#include <Emerald/Core/Defines.h>

namespace Asteroids {

// The game's icon, drawn in code: the vector ship pointing up-right with a soft glow, on a dark
// rounded square. `size` x `size` RGBA pixels. Used for the window icon at runtime, and by
// tools/IconGen to write assets/icon/icon.ico for the Windows .exe.
[[nodiscard]] Emerald::Image MakeIcon(u32 size);

} // namespace Asteroids
