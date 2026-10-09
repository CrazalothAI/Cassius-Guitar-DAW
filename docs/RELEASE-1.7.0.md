# Cassian 1.7.0 Preview — amplifier collection and studio finish

Tone now presents four original covered amplifier heads, each with Cassian branding, its own wordmark, materials, palette and graphical starter switch:

| Head | Character | Finish | Complete starter |
| --- | --- | --- | --- |
| Lumen | Clean | Ivory / nickel | Prism Clean |
| Rubicon | Crunch | Oxblood / brass | Classic Rock |
| Ferrum | Metal | Graphite / steel; tubes behind a protective grille | Iron Rhythm |
| Aurelia | Classical / acoustic | Walnut / linen | Natural Nylon |

These names describe the visual collection and starting rigs. The actual engine/capture identity remains visible. Rubicon uses the existing Ferrum engine at its lowest drive setting; Iron Rhythm uses the existing hard distortion pedal into Natural DI and the built-in cabinet. Aurelia is a pickup/DI starting point, not an electric-to-nylon simulation. The preset menu and Library retain all other original rigs and optional capture recipes.

Graphical switches recall complete rigs through the existing preparation path. Input gain, Master, metronome and Play Along listening controls remain global. Switches are unavailable during rig loading, recording and take playback. Save your edits before loading another head: it replaces the current tone and scenes. Known starter categories, selected engine and live serial pedal state guide the presentation after other recalls; artwork never changes audio by itself.

The original wolf logo stays on the metal faceplate without a ring. Four optimized local WebP cabinet graphics, live typography, machined knob styling, original workspace icons and pedal illustrations form the studio finish. Controls remain keyboard-accessible HTML. Compact heads keep Practice/Takes spacious, and Tone details scroll at smaller supported editor sizes. Graphics are embedded into the native editor and work offline without fetching images.

Help & setup adds a current-route next step and direct links to Tone, Practice and Takes. Missing information remains unknown; reported monitoring-off, inactive callbacks, clipping and recent missed deadlines receive specific guidance. Navigation does not start recording/playback or change the rig. Input monitoring and the input check remain explicit actions. Browser preview and DAW-host routing guidance stay distinct.

For a Natural DI rig with an active distortion pedal, the head's Drive knob controls that pedal's existing Drive parameter. Natural DI bypasses amp saturation, so exposing the old amp Drive there was ineffective. Without a distortion pedal, the direct head exposes Output trim. Other amp engines retain their existing Drive control. Compact heads follow the same rule; automation parameter identities remain intact.

The head's ambience knob also follows the active serial plate, spring, room or recorded-ambience blend rather than an unused compatibility reverb. Dry serial rigs expose the working Presence control in that position. The chosen parameter name/unit remains visible on the knob; use Board for the other effects and additional instances.

Rig, scene, pack, journal, recording and automation contracts are unchanged. Browser-only saved preview rigs retain their gain category for display; this is not native audio state.

## Verification

255 UI checks pass, including complete graphical recall, preserved listening controls, native recording guards, category/engine changes, effective pedal Drive/ambience controls, route unknown/stale states and navigation without transport side effects. Five native CTest entries pass with the owner sound-bank fixtures, including processor/state/recovery and NAM checks. The rebuilt VST3 passes three pluginval strictness-10 runs with distinct seeds. Isolated Windows installer checks pass for the app, shortcut, optional VST3, upgrade, uninstall and preserved user data. Browser renders were inspected at 860×620, 1100×760 and 1440×1000 across the four heads and other workspaces. Package verification is recorded separately with this build; automated browser and installer checks do not establish fresh-PC, physical-interface or actual DAW editor acceptance.

This remains an unsigned Preview. Existing manual acceptance and distribution-review gates stay pending. [Road to 2.0](ROAD-TO-2.0.md) · [Artwork notes and prompts](ARTWORK.md) · [User guide](USER-GUIDE.md).
