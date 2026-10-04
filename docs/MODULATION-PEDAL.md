# Modulation pedal

On **Effects**, enable **Modulation** and choose Phaser, Flanger or Tremolo. This original stereo block sits after cabinet/EQ and before chorus, delay and reverb. It works with built-in amps, NAM and Natural DI. Backing tracks and the metronome bypass it, and switching it off lets downstream delay/reverb tails continue.

| Control | Behavior |
|---|---|
| Voice | Six-stage phaser, short-delay flanger or sine-wave tremolo. Switching voices crossfades over 30 ms. |
| Speed | 0.05–10 Hz, retained while tempo sync is enabled. |
| Motion | Sweep depth for phaser/flanger; attenuation depth for tremolo. At zero, phaser/flanger still apply a fixed filter/comb effect. |
| Blend | Dry-to-effect balance, 0–100%. Phaser/flanger effect signals themselves contain an equal dry/wet blend; 100% does not mean wet-only. |
| Regeneration | 0–70% bounded feedback for phaser/flanger. Hidden and unused by the tremolo output. |
| Spread | 0–100% offsets right-channel movement from 0 to 90 degrees, preserving the incoming stereo channels. Mono has one modulation phase. |
| Tempo / cycle | One full LFO cycle per whole, half, quarter, eighth or dotted-eighth note. At 120 BPM, quarter gives 2 Hz; whole gives 0.5 Hz. |

Tempo sync follows a playing DAW's BPM or the standalone metronome tempo. It matches cycle duration, not bar phase: the LFO runs freely and does not retrigger on host transport/notes. Speed changes are smoothed rather than resetting the oscillator. Rate is capped at 10 Hz. Offline reamping uses the snapshot's saved metronome BPM for both synced modulation and delay; previously offline delay could use the instance's default/stale tempo.

Phaser sweeps six first-order all-pass stages around 570 Hz on an exponential range. Flanger sweeps a linearly interpolated delay centered at 3.5 ms, reaching 0.5–6.5 ms at full Motion. Feedback uses a bounded nonlinear return and level compensation; these are original effects, not hardware pedal models. Tremolo attenuates without boosting: full Motion and Blend sweep between silence and the incoming level. Reverb/delay after it can fill those quiet portions.

**Velvet tremolo** starts with Lumen clean; **Phase lead** uses Ferrum and overdrive; **Jet rock** uses Ferrum with a slow flanger. These are control starting points. Save a complete rig to retain your exact captures and routing. Existing starting presets and older complete rigs keep modulation bypassed. All nine parameters append after the previous 76 automation indices, participate in A/B, rig packs and native session recall, and add no reported processing latency. The existing effect parameter smoothing handles bypass and editing. A settled disabled/zero-Blend block is sample-exact and skips processing; storage is allocated during prepare, not the callback.

**Foot control** offers **Toggle modulation** as a CC/PC action using the existing eight assignments and Learn workflow.

Windows validation on 2026-10-03: 86 UI tests, four native CTest suites, standalone/VST3 Release builds. Native tests measure phaser cancellation, flanger comb filtering, tremolo rate/depth, stereo phase offset, switching steps and maximum-feedback stability at 44.1/48/96 kHz. They also verify exact bypass, mono zero blend, session/legacy rig recall, MIDI bypass and measured 90-BPM offline modulation/delay timing. UI tests cover voices, A/B, retained manual rate, tempo subdivisions and preset defaults. Minimum 860×620 layout uses internal scrolling.

An isolated active-block benchmark at 48 kHz / 128 samples measured 0.011 ms mean and 0.069 ms maximum on the development machine, against a 2.667 ms callback budget. This excludes the rest of the rig, drivers and other applications and is not a hardware reliability guarantee. Live listening, physical MIDI control, DAW transport synchronization and Linux validation remain to perform.
