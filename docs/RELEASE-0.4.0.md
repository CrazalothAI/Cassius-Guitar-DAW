# Cassian 0.4.0 preview

This milestone adds an expressive wah/envelope filter to the serial pedalboard and two complete starting tones.

- Manual wah sweep with dedicated MIDI expression targets for each instance, including Learn and inverted direction.
- Pick-responsive envelope mode, adjustable sensitivity, resonance, dry blend and output trim.
- Two independent instances, before-amp or stereo after-cabinet placement, bypass, duplication, Undo/Redo, rig/scene/session recall and portable rig support.
- **Envelope Clean** and **Vowel Lead** built-in rigs. The starter collection now includes 14 built-in rigs and 20 exact-capture recipes.
- Migration defaults for pre-0.4 rigs/scenes, preserving all existing automation IDs/order. Incomplete wah-enabled documents reject before recall.

See [wah setup and compatibility](WAH-PEDAL.md). Recording, Play Along isolation, reamp tails and soundtrack trimming/fades remain available.

## Validation

Validated on Windows on 2026-10-06: 136 UI tests and all four native CTest suites passed, including the optional 103-file bank. Release standalone/VST3 builds and binary version checks passed. Isolated installer installation, upgrade from 0.0.1 to 0.4.0, uninstall, registration, shortcut/VST3 placement, user-data preservation and all private-bank hashes passed. Wah tests cover manual frequency response at 44.1/48/96 kHz, envelope sensitivity, block partitioning, silent-channel isolation, zero blend/bypass, independent controls, MIDI/inversion, migrated old rigs/scenes, rejected incomplete recall, alternate-rate native restore and portable-pack round-trip. The calibrated 14 built-in rigs retain a 3.54 dB spread on the synthetic reference signal; that does not establish perceived loudness across real guitars. Live interface/expression-pedal audition, Linux, real DAW hosts, fresh-PC installation and Clipchamp camera alignment remain manual release checks. Third-party captures still require per-asset distribution grants; private supplied sounds are excluded from public source/downloads. Binaries remain unsigned development previews.
