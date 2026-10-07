# Cassian 0.8.0 preview

This milestone combines 0.7.1 original spring-style reverb, 0.7.2 selected-pedal Reset controls and four complete starting rigs. The catalog now includes 22 original file-free rigs and the unchanged 20 exact-capture recipes.

| Starter | Sound |
| --- | --- |
| Copper Blues | Low-drive Overdrive into Lumen, warm mids and restrained spring |
| Country Spring | Articulate Lumen clean, gentle compression and short spring |
| Surf Clean | Open Lumen clean and a brighter, drippier spring tail |
| Fuzz Orbit | Dedicated Fuzz mode, built-in 4x12, EQ and small warm plate |

Both cleans have no drive pedal and keep the gate off. Complete starter recall replaces the whole amp/board/scenes while preserving Input calibration, Master, metronome and Play Along. Existing starter IDs/settings and capture recipes are unchanged. Every original starter works without sound files. Output trims use the same synthetic guitar reference and headroom checks as prior rigs; measured levels do not establish real guitar or perceptual matching.

Spring uses four dispersive feedback rings with six delayed allpasses each, separate from Room/Chamber/Hall and Plate. Decay sets nominal feedback time; Tone damps returns; Drip changes dispersion; Pre-delay delays only the wet onset; Blend preserves the original dry timing at zero. Two independent slots append after Plate with absent-family migrations and strict validation for spring-containing documents. This is an original digital spring-style effect, not a physical-tank measurement. See [0.7.1](RELEASE-0.7.1.md).

Reset controls restores only the selected pedal controls/trim, preserving bypass, assigned file, identity, slot, lane and everything outside the pedal. Undo/Redo restores the settings; resetting defaults leaves history untouched. Like structural edits, a real reset uses a prepared graph swap, briefly fades the guitar and restarts effect tails. See [0.7.2](RELEASE-0.7.2.md).

## Validation

Validated on Windows on 2026-10-07: all 174 UI tests and all four native suites passed with the optional 103-file bank. The earlier 0.7.1/0.7.2 patches separately passed 172/173 UI tests and all four native suites. Release standalone/VST3 and embedded frontend built, and binary version/input checks passed for 0.8.0.

Every file-free starter loaded on an empty library and rendered finite output with headroom. The overall synthetic RMS spread was 3.545 dB. Copper Blues/Country Spring/Surf Clean/Fuzz Orbit measured -23.33/-24.71/-23.87/-22.49 dBFS respectively. All 103 relocated bank assets and 20 exact capture recipes also passed.

The isolated installer passed installation/upgrade/uninstall, current-version registration, app/shortcut/optional-VST3 placement, all private bank hashes and user-data preservation. Packaging produces root Cassian.exe, setup/portable aliases, versioned 0.8.0 copies, checksums and clean-checkout metadata. Integrity verification checks alias/metadata hashes, ZIP root layout, executable identity, third-party notices and private sound hashes.

A new six-effect fixture chains two distortions, two plates and two springs at 48/96 kHz and 128/256/512 samples. It asserts finite audio/headroom and reports average/max processing cost after warmup against callback duration. Timing is diagnostic, not a CI pass/fail threshold or a guarantee of live interface performance; it excludes a real amp/cab/host/interface. Spring unit/integration checks and all original/capture starter recalls also run in the full suite.

| Rate / buffer | Mean effect processing | Maximum observed | Callback duration |
| --- | --- | --- | --- |
| 48 kHz / 128 | 0.050 ms | 0.209 ms | 2.667 ms |
| 48 kHz / 256 | 0.099 ms | 0.285 ms | 5.333 ms |
| 48 kHz / 512 | 0.197 ms | 0.428 ms | 10.667 ms |
| 96 kHz / 128 | 0.051 ms | 0.277 ms | 1.333 ms |
| 96 kHz / 256 | 0.100 ms | 0.350 ms | 2.667 ms |
| 96 kHz / 512 | 0.201 ms | 0.476 ms | 5.333 ms |

These are this Windows machine's synthetic six-effect measurements from the final native suite, not a full-application latency or dropout measurement.

Public packages include all original DSP/starters and omit private captures; a separate local development package can include the tested 103-file bank. These files need per-asset redistribution permission before public shipping. Windows binaries remain unsigned previews. Sustained guitar/interface playing, real DAW automation/recall, Linux, fresh-PC prerequisites and Clipchamp/camera audition remain manual checks. This milestone does not push, merge or publish a GitHub release.
