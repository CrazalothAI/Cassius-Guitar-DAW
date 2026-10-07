# Cassian 0.5.1 preview

This patch improves riff and reamp auditioning in Takes.

- Pause/resume retains the playback position. Resume at the natural end starts again from the beginning.
- Set A/B at the current review position, enable Loop A–B and return with Go to A. B must be at least 0.05 seconds after A. Loops use the existing 5 ms edge fades, bounded to a quarter of the loop length.
- The review status identifies the loaded take and version. Selecting something else keeps that label accurate and disables transport/loop controls until Listen loads the selection.
- Native review commands also validate the take/version identity. Pending preparation, Stop, superseded requests and missing audio cannot resume a stale version through the new controls.

Listen reloads the selected file and resets loop bounds. Pause restores live guitar monitoring; backing stays paused. Stop cancels preparation and clears the review identity. Review volume remains global. These are temporary listening controls; recordings, rig snapshots, reamp rendering, soundtrack trim/mix and quick WAV exports remain unchanged.

No host parameters, rig schemas, catalog formats or new sound files were added. The implementation uses the existing worker-side decoder and bounded practice audio path; it adds no audio callback file I/O.

## Validation

Validated on Windows on 2026-10-07: all 154 UI tests and four native CTest suites passed, including the optional 103-file bank. Release standalone/VST3 builds, binary version checks and isolated installer installation/upgrade/uninstall passed. Installer checks cover app/shortcut/VST3 placement, version registration, user-data preservation and private sound hashes.

UI coverage exercises pause/resume, seek, loop commands, loaded identity versus selection, pending/recording states, invalid loops and worker errors without changing export settings. Native coverage renders stored processed/dry audio, loops past the natural end, checks boundary fades and paused position, rejects stale/invalid controls, resets new-version loops and cancels superseded previews. Missing audio leaves preparation cleared and review unavailable. Existing regressions still verify preserved original WAV/snapshot bytes and video export format/content.

Live interface audition, Linux, real DAW hosts and fresh-PC setup remain manual checks. This is an unsigned development preview. Third-party sounds remain private until redistribution grants are recorded.
