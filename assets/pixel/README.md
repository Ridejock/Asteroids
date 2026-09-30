# Pixel sprites

Sprites for the pixel version (ROCK BLASTER ROGUE), made for this project and free to use with it.
The original ship, saucer and rocks are AI-assisted; the roguelike's sprites (in `rogue/`) are made
in code in the same style by `tools/PixelSprites/rogue.py` (recolored rock kinds and ship variants,
procedural bosses, hand-drawn 16 x 16 icons).

- `atlas.png` + `atlas.json`: everything in one texture; this is what the game loads
  (`Emerald::TextureAtlas`). The JSON maps each name to its rectangle in pixels:
  `{ "ship": { "x": 1, "y": 1, "w": 38, "h": 80 }, ... }`.
- The individual PNGs are the same sprites one per file, for editing.

| Name | Size | Notes |
|---|---|---|
| `ship` | 38 x 80 | points up; the engine flames are the bottom rows (from y = 67), cropped off when not thrusting |
| `enemy` | 80 x 46 | the saucer, seen from above |
| `asteroid_large_1/2`, `asteroid_medium_1/2`, `asteroid_small_1/2` | 40 x 36 ... 22 x 20 | two looks per size |
| `rock_explosive_*`, `rock_metal_*`, `rock_splitter_*` (`large`, `medium`, `small`) | same as the rocks | the roguelike's rock kinds |
| `ship_bulwark`, `ship_wasp`, `ship_lancer` | 50 / 34 / 30 x 80 | the hangar's ships; flames from y = 67 like `ship` |
| `boss_monolith`, `boss_mothership`, `boss_station` | 86 x 94, 194 x 68, 110 x 110 | the three bosses |
| `upgrade_<key>` | 16 x 16 | upgrade icons (keys as in `src/Rogue/Upgrades.cpp`) |
| `scrap`, `missile` | 7 x 7, 5 x 9 | scrap pickup, homing missile (points up) |

The build copies `assets/` next to the executables (`build/<preset>/bin/assets/pixel/`).
