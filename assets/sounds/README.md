# Optional sound overrides

Put your own sounds here to replace the generated ones. Files in this folder (except this
README) are ignored by git, so nothing you add gets committed. The build copies the folder next to
the executable (`build/<preset>/bin/assets/sounds/`); you can also put files there directly.

Name each file `<name>.mp3` or `<name>.wav` (if both exist, the `.mp3` is used):

| Name | Replaces | Notes |
|---|---|---|
| `fire` | shot | |
| `thrust` | engine | looped while thrusting: use a clip that loops seamlessly |
| `bang_large` | large rock explosion | |
| `bang_medium` | medium rock explosion | |
| `bang_small` | small rock explosion | |
| `ship_explode` | ship explosion | |
| `extra_life` | extra ship | |
| `beat1` | heartbeat, first tone | |
| `beat2` | heartbeat, second tone | |
| `hyperspace` | hyperspace jump | |
| `music` | *(nothing by default)* | loops on the game over screen, never during play |
| `ambience` | *(nothing by default)* | loops quietly (25%) under the gameplay, stops on game over |

Missing files simply keep the generated sound. The log says which overrides were loaded, e.g.
`Sound overrides from ...: fire, music`.
