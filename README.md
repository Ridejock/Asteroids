# Asteroids

A small, readable remake of the classic vector arcade game **Asteroids**, built with the
[Emerald](https://github.com/Ridejock/Emerald) C++20 engine (SDL3 + SDL GPU). Everything on screen —
ship, rocks, bullets, explosions, even the score digits and letters — is drawn with 1 px lines by
Emerald's `Renderer2D`; there are no textures or font files.

![Screenshot](docs/screenshot.png)

## Controls

| Key | Action |
|---|---|
| **A / D** or **← / →** | Rotate left / right |
| **W** or **↑** | Thrust |
| **Space** | Fire (at most 4 shots on screen) |
| **Shift** | Hyperspace: jump to a random spot (1 s cooldown) |
| **Enter** | Restart after *Game Over* |
| **Esc** | Quit |

The keys are bound to named input actions (`Rotate` axis, `Thrust`, `Fire`, `Hyperspace`, `Start`,
`Quit`) in `AsteroidsApp::OnStart` in `src/Main.cpp`; change a binding there, or at runtime with
`GetInput().RebindAction(...)`.

## Rules

- You have **3 ships**; an extra one every 10 000 points.
- Large rocks split into 2 medium ones, medium into 2 small, small ones vanish. Points: 20 / 50 / 100.
- After a crash the ship respawns in the center after 2 s and blinks for 3 s, during which it cannot
  be destroyed.
- Clear the field to start the next wave, which has one more large rock (up to 11).
- Everything wraps around the screen edges.

## Building

Requirements: CMake 3.24+, Ninja, Git and a C++20 compiler (MSVC 2022, GCC 11+, Clang 14+). The engine
and all its dependencies (SDL3, spdlog, stb, and the SDL_shadercross shader compiler) are fetched and
built automatically.

> **First build takes a while** (≈10–20 minutes): Emerald builds SDL_shadercross, including
> Microsoft's DirectXShaderCompiler, from source into `build/_shadercross`. It is shared by all presets
> and only built once. See [Engine development](#engine-development) to reuse an existing Emerald
> checkout's build instead.

### Windows – VS Code (CMake Tools) + MSVC

1. Install **Visual Studio 2022** (or the *Build Tools*) with the *Desktop development with C++*
   workload (includes MSVC, CMake and Ninja), Git, and VS Code with the **C/C++** and **CMake Tools**
   extensions.
2. Open the `Asteroids` folder in VS Code. CMake Tools reads `CMakePresets.json` (same presets as
   Emerald's, so it works exactly like building the engine); pick the configure preset **Debug** (or
   Release / Debug (ImGui debug overlay)) in the status bar.
3. **Build** (F7), then **Run/Debug** the `Asteroids` target (Shift+F5 / Ctrl+F5).
   The executable is `build\<preset>\bin\Asteroids.exe`, with its compiled shaders in
   `build\<preset>\bin\shaders\`.

From a *Developer PowerShell / x64 Native Tools prompt for VS 2022* instead:

```powershell
cmake --preset debug
cmake --build --preset debug
.\build\debug\bin\Asteroids.exe
```

### Linux / macOS

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/bin/Asteroids
```

(On Linux, SDL3 needs the usual X11/Wayland development packages; see Emerald's README.)

### Presets

| Preset | Description |
|---|---|
| `debug` | Debug build |
| `release` | Optimized build |
| `debug-full` | Debug build with Emerald's Dear ImGui overlay (`EMERALD_USE_IMGUI=ON`): FPS, wave, score, line count |

### Command line

```sh
Asteroids --frames 600                        # quit after 600 frames
Asteroids --frames 600 --screenshot shot.png  # save the last frame as a PNG
Asteroids --seed 42                           # repeatable asteroid layout
```

The log is written to `logs/Asteroids.log` next to the executable.

## Engine development

Emerald is pulled in with `FetchContent`, pinned to a commit in `CMakeLists.txt` (`GIT_TAG`). To
develop the engine and the game side by side, point the build at a local Emerald checkout:

```powershell
cmake --preset debug -DASTEROIDS_EMERALD_SOURCE_DIR=C:/dev/Emerald
```

Engine changes are then picked up by the next game build, and the checkout's `build/_shadercross`
is reused (no second shader-compiler build). This is shorthand for CMake's own
`-DFETCHCONTENT_SOURCE_DIR_EMERALD=...`. In VS Code you can put it in a `CMakeUserPresets.json`
(git-ignored), e.g.

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "debug-local",
      "inherits": "debug",
      "cacheVariables": { "ASTEROIDS_EMERALD_SOURCE_DIR": "C:/dev/Emerald" }
    }
  ]
}
```

When the engine change is pushed, update `GIT_TAG` to the new commit hash. (To go back to the pinned
commit, clear the variable: `-DASTEROIDS_EMERALD_SOURCE_DIR=`.)

## Code tour

| File | What it does |
|---|---|
| `src/Main.cpp` | The `Emerald::Application`: binds the controls as input actions in `OnStart`, reads them in `OnFixedUpdate` (120 Hz), fits the playfield into the window and draws it in `OnRender2D` |
| `src/Game.h/.cpp` | Game state and rules: waves, bullets, collisions, lives, score, explosions, HUD |
| `src/Ship.h/.cpp` | Ship movement (rotation, thrust with inertia and drag) and its outline + flame |
| `src/Asteroid.h/.cpp` | Random jagged rocks, sizes, splitting, points |
| `src/Bullet.h` | Bullet data and limits |
| `src/VectorFont.h/.cpp` | A line-segment font (A–Z, 0–9) on a 4 × 6 grid |
| `src/Playfield.h` | The fixed 1280 × 720 logical playfield: wrap-around, wrapped distances, drawing objects on both sides of an edge |
| `src/Random.h` | Tiny `std::mt19937` helper |

The game logic (`Game`) never touches the window or GPU: it gets a `GameInput` per fixed step and
draws into a `Renderer2D` that `Main.cpp` has already set up with the right projection. The playfield
is always 1280 × 720 logical pixels, scaled uniformly to the window with black bars (and clipped) when
the aspect ratio differs, so resizing the window never changes the gameplay.

## License

[MIT](LICENSE) © 2026 Ervin Ashley
