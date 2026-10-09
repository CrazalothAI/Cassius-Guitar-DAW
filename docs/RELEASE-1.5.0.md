# Cassian 1.5.0 Preview

This milestone adds reusable practice sets/local session history and completes streamed long-take looping after the 1.4.0 transport foundation.

## Practice workflow

- Reusable named sets with up to eight independent exercises, 1–120 minute targets and 40–240 BPM targets.
- Explicit native-clock start/pause/resume/finish, saved notes, searchable history and finished-session totals. Starting a timer does not change the rig, backing playback or click.
- Explicit metronome-tempo application, confirmed deletions, capacity limits, worker-side atomic persistence and one writable journal per library.
- Thirty-second checkpoints and honest interrupted states; reopening never counts closed-app time or starts a timer.
- Portable additive JSON imports/exports. Personal backups preserve journal copies; recovery uses explicit import while keeping local history.

See [practice sets/history](PRACTICE-JOURNAL.md).

## Long-take loops and diagnostics

The existing sixteen-block streaming cache shares its fixed 2,097,280-byte audio budget between current playback and the loop start. Prefetch readiness requires the loop-head block and its following runway. Saved sections now recall paused at A with looping enabled. Fractional boundaries interpolate into the actual loop start; short/distant loops, EOF boundaries and optional cosine edge fades follow the decoded player's behavior. Missing data holds the cursor and emits silence until ready.

Takes shows loop-start preparation and **Playback buffer waits**, including uncached seeks. Help copies review buffering, cache and loop readiness separately from processor overruns/driver dropouts, without take names, file paths or annotations.

## Validation and release limits

Local validation passed: **235 UI tests across 25 files**, all **five native CTest suites** (48.25 seconds) with independent ZIP64-fixture verification, three reproducible level-10 pluginval runs, matching app/plugin version checks, and isolated Windows install/upgrade/uninstall tests preserving user data. Standalone and VST3 were rebuilt from the same final implementation. Browser visual inspection remains unavailable because the local browser-control runtime failed; automated component tests do not establish human visual acceptance.

Automated coverage includes decoded-versus-streamed mono/stereo loops at 44.1/48/96 kHz source/device rates, fractional boundaries, fades, EOF loops, repeated short loops, edited bounds, deliberately unavailable loop heads, pinned-cache ownership, source failure and continued guitar/click muting during review. Journal tests use an injected native clock to verify paused time, checkpoints, shutdown/reopen, second-window exclusion, import idempotence/conflict rejection, invalid documents, capacity and external-edit preservation. UI tests exercise planner actions, explicit tempo, timer controls, note saving, search, confirmed deletion and transfers. Personal archive tests verify journal retention without replacing local history.

The initial clean GitHub build reported a Windows heap failure after the sound-pack checks. Repeated local Release checks and the complete instrumented processor suite did not reproduce that fault; standard Windows AddressSanitizer completed with empty error output. The follow-up adds sixteen independent sound-pack/ambience lifetimes, focused native commands, clearer phase output, and automatic isolated AddressSanitizer diagnostics after a failed CI test. It does not claim a proven root-cause repair. See [native memory testing](NATIVE-TESTING.md); a failed ordinary CI test still blocks publication.

The release remains an unsigned preview. Sustained physical-interface playing, actual DAW/MIDI use, second-PC transfers, beginner setup, export/camera alignment and cleared third-party sounds remain separate acceptance work. Streamed take review remains normal-speed standard WAV; RF64, damaged-header recovery, pitch processing and gapless scene changes are not added here. The journal records elapsed practice time, not played-note accuracy. No telemetry, account, billing or subscription dependency is introduced.
