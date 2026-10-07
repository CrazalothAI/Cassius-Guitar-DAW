# Cassian 0.3.0 preview

This milestone improves recording delivery for video editors while keeping original recordings intact.

- **Reamp effect tails:** choose no extra tail, 1, 2 (default), 5 or 10 seconds. The offline processor continues rendering silence through the captured rig. Tail lengths and version frame counts survive catalog reopening. Native callers retain the previous zero-tail default and can request up to 30 seconds.
- **Soundtrack selection:** set start/end seconds for the selected processed, dry or reamped version. Full take restores that version's complete duration. Invalid ranges cannot start an export. Bounds are rounded to source frames; output duration is rounded at 48 kHz.
- **Edge fades:** 0–100 ms raised-cosine fades, with a 10 ms editor default. Very short selections clamp fades to half their length. Both mix/peak-measurement passes use the identical selection and fades.
- **Backing with reamp tails:** original synchronized backing stays aligned and becomes silent past its recorded end. The appended guitar tail remains audible. Unsupported or mismatched stems are rejected.
- **Compatible recording format:** originals, rig snapshots and existing reamp WAVs remain unchanged. Older versions without frame metadata use the original take duration in the editor. Existing native export calls retain full-duration, zero-fade behavior.

The export remains stereo 48 kHz / 24-bit PCM with attenuation only when needed for −1 dBFS sample-peak headroom. These controls do not change guitar tone, live monitoring, DAW parameter identities or recording automation. Reamp latency is still uncompensated; fixed tails can cut off effects that decay longer than the selected duration.

## Validation

Native regressions cover zero-tail compatibility, actual delay energy in appended frames, persisted duration, original-file preservation, synchronized backing and silence past its end, resampled selections at 44.1/48/96 kHz, fade boundaries, short-selection clamping, invalid bounds and cancellation. UI tests cover version-specific ranges, clearing/invalid inputs, range reset, tail selection, recording locks and bridge requests.

Validated on Windows on 2026-10-06: 134 UI tests, all four native CTest suites with the optional 103-file bank, Release standalone/VST3 builds, Windows version-resource checks and isolated installer install/upgrade/uninstall passed. App and VST3 report 0.3.0. The installer retained its test user-data sentinel and verified all private bank hashes. Live interface audition, Linux, real DAW hosts, fresh-PC installation and Clipchamp camera alignment remain manual release checks. Third-party capture redistribution still requires per-asset permission; the private owner bank is excluded from public source/downloads. Binaries remain unsigned development previews.
