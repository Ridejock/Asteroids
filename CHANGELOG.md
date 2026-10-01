# Changelog

## 1.2.1 (2026-10-01)

- itch app manifest: each zip has a `.itch.toml` at its root, so the itch.io app's Play button
  launches the right game (`RockBlaster.exe` / `RockBlasterRogue.exe`).

## 1.2.0 (2026-09-30)

- New game: **ROCK BLASTER ROGUE** (`RockBlasterRogue.exe`, its own per-user folder and zip), a
  roguelike on the same controls: runs of 3 sectors x 4 waves with a boss per sector, permadeath,
  an upgrade pick after every wave (10 stacking upgrades), explosive / metal / splitting rocks,
  three bosses with a health bar, scrap and a hangar (4 ships, upgrades for the pool) that keeps
  its progress in `meta.txt`, and a high score table that shows how far each run got. New
  sprites for all of it.
- TrueType text in both games (Emerald's new `Font` and `Renderer2D::DrawString`):
  - ROCK BLASTER ROGUE uses Press Start 2P, crisp (nearest filtering, whole-number scales), for
    its menus, HUD, upgrade picker, hangar, results, high scores and boss names.
  - ROCK BLASTER uses Share Tech Mono, glowing additively like the lines, on the old letter grid
    so every layout stays the same.
  - Both fonts are under the SIL Open Font License; each zip has its font and license in
    `assets/fonts/` and in `THIRD_PARTY_LICENSES.txt`.
- ROCK BLASTER: optional **CRT effect** (*Options > CRT EFFECT*, or **F9**; off by default, saved
  in `settings.txt`): curved glass, phosphor bloom, a short afterglow, a slight chromatic fringe
  and a vignette, with no scanlines, like a vector monitor. F9 is listed on the controls page.
  `--crt on|off` sets it for one run. ROCK BLASTER plays exactly as before.
- Engine (Emerald, updated to `781eb01`): the CRT post-process (`CrtEffect`), and a font loading
  fix: fonts that lack some of the requested characters now load (the missing ones are skipped)
  instead of failing.

## 1.1.0 (2026-09-30)

- Particle effects: sparks and dust when rocks split (more for bigger rocks), a fireball and shock
  ring when the ship explodes, debris from a hit saucer, and an exhaust trail while thrusting.
- *Options > Particles* switches them on or off (saved in `settings.txt`, on by default).

## 1.0.0 (2026-09-30)

- First release of Rock Blaster.
