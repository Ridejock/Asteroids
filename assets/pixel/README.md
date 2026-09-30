# Pixel sprites

Sprites for `AsteroidsPixel`, generated for this project (AI-assisted) and free to use with it.

- `atlas.png` + `atlas.json`: everything in one texture; this is what the game loads
  (`Emerald::TextureAtlas`). The JSON maps each name to its rectangle in pixels:
  `{ "ship": { "x": 1, "y": 1, "w": 38, "h": 80 }, ... }`.
- The individual PNGs are the same sprites one per file, for editing.

| Name | Size | Notes |
|---|---|---|
| `ship` | 38 x 80 | points up; the engine flames are the bottom rows (from y = 67), cropped off when not thrusting |
| `enemy` | 80 x 46 | the saucer, seen from above |
| `asteroid_large_1/2`, `asteroid_medium_1/2`, `asteroid_small_1/2` | 40 x 36 ... 22 x 20 | two looks per size |

The build copies `assets/` next to the executables (`build/<preset>/bin/assets/pixel/`).
