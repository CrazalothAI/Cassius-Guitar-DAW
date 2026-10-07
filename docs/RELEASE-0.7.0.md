# Cassian 0.7.0 preview

This milestone combines the 0.6.1 dedicated distortion and 0.6.2 original stereo plate with four complete file-free starter rigs. The app now includes 18 built-in rigs alongside the unchanged 20 exact-capture recipes. New tones are available in the rig selector and Library; recalling a starter replaces the whole amp/board/scenes while preserving calibration and listening/performance controls.

| Starter | Sound and routing |
| --- | --- |
| Iron Rhythm | Hard clipping, controlled bass, built-in 4x12 and post EQ for dry rhythm |
| Velvet Lead | Asymmetric clipping into Lumen, short stereo delay and plate for melodic leads |
| Prism Clean | Lumen, gentle post compression and short stereo plate; no drive or gate |
| Midnight Space | Light compression, Lumen, chorus, delay and a warm wide plate; no drive or gate |

These are original built-in tones, included in source and every app package without extra downloads. Existing presets retain their identities/settings. Synthetic output trims are checked across all starters with finite-output/headroom assertions and a six-dB maximum reference-level spread; this is not perceptual matching or real guitar audition.

## Effects and compatibility

Distortion has Hard/Asymmetric/Fuzz voices with 4x oversampled clipping, DC removal, bass control, tone, blend and output trim. Plate uses an original eight-line stereo feedback tank with input diffusion, damping, nominal decay, wet pre-delay, width, blend and output trim. It is not a physical-plate measurement or branded emulation. See [0.6.1 details](RELEASE-0.6.1.md) and [0.6.2 details](RELEASE-0.6.2.md).

Both families have two independent slots; new host IDs append after prior controls. Rig/scene migrations default absent new families and require complete controls when a new block exists. Existing IDs, legacy audio paths and schemas remain unchanged. Older apps cannot play documents using unknown new effect types. New rigs support editing, Save, scenes, sessions and reference/portable export through the existing complete-rig workflow.

## Validation and downloads

Validated on Windows on 2026-10-07:

- All 171 UI tests and all four native CTest suites passed with the optional 103-file bank. The earlier 0.6.1/0.6.2 patches separately passed 169/170 UI tests and all four native suites.
- Release standalone, VST3 and the embedded frontend built; binary version/input checks passed for 0.7.0.
- Distortion/plate checks cover common sample rates, callback partitioning, audible mode/drive/tone/decay differences, finite bounded output, dry blend, stereo width/pre-delay, appended automation, independent instances, old-state migration, rejected incomplete recall, scene/session recall and portable packs.
- All 18 built-in starters loaded without a sound library and rendered with headroom. The reference RMS spread was 3.545 dB; the four new tones measured -23.59, -23.50, -24.39 and -24.47 dBFS respectively. The separate relocated bank check verified all 103 files and all 20 exact recipes.
- The isolated installer passed install/upgrade/uninstall, current-version registration, app/shortcut/optional-VST3 placement, bank hashes and preservation of user data.

Packaging emits the root Cassian.exe, setup/portable aliases, versioned 0.7.0 copies, checksums and clean-checkout metadata. Public packages omit the private bank; a separately marked local development package contains all 103 supplied sounds. Integrity verification checks alias/metadata hashes, ZIP root layout, executable identity, third-party notices and private sound hashes.

Windows binaries remain unsigned development previews. Sustained real interface playing, real DAW automation/recall, Linux, fresh-PC prerequisite testing and Clipchamp/camera audition remain manual release checks. The private 103-file bank is tested separately and requires per-asset redistribution grants before public shipping; public packages include all original DSP/starters and no private captures. This milestone does not publish, tag or merge a GitHub release.
