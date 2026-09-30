# Changelog

## Unreleased

- The pixel version is now its own game, **ROCK BLASTER ROGUE** (`RockBlasterRogue.exe`, its own
  per-user folder and zip): a roguelike with runs of 3 sectors x 4 waves and a boss per sector,
  permadeath, an upgrade pick after every wave (10 stacking upgrades), explosive / metal /
  splitting rocks, three bosses with a health bar, scrap and a hangar (4 ships, upgrades for the
  pool) that keeps its progress in `meta.txt`, and a high score table that shows how far each run
  got. New sprites for all of it.
- ROCK BLASTER (vector) plays exactly as before.
- Text is now drawn with TrueType fonts through Emerald's new `Font` and
  `Renderer2D::DrawString` (Emerald updated to `fe1847b`, which also fixes loading fonts that
  lack some of the requested characters). ROCK BLASTER ROGUE uses Press Start 2P (crisp, nearest
  filtering, whole-number scales) for its menus, HUD, upgrade picker, hangar, results, high
  scores and boss names; ROCK BLASTER uses Share Tech Mono, glowing additively like the lines, on
  the old letter grid so every layout stays the same. Both fonts are under the SIL Open Font
  License; each zip has its font and license in `assets/fonts/` and in
  `THIRD_PARTY_LICENSES.txt`.
- ROCK BLASTER: *Options > CRT EFFECT* (and **F9**), off by default and saved in `settings.txt`:
  Emerald's new CRT post-process (Emerald updated to `781eb01`) with curved glass, phosphor
  bloom, a short afterglow, a slight chromatic fringe and a vignette; no scanlines, like a vector
  monitor. `--crt on|off` sets it for one run. ROCK BLASTER ROGUE is unchanged.

## 1.1.0 (2026-09-30)

- Particle effects: sparks and dust when rocks split (more for bigger rocks), a fireball and shock
  ring when the ship explodes, debris from a hit saucer, and an exhaust trail while thrusting.
- *Options > Particles* switches them on or off (saved in `settings.txt`, on by default).

## 1.0.0 (2026-09-30)

- First release of Rock Blaster.
