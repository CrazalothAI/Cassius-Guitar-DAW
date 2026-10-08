# Cassian 1.1.0 preview — backup and recovery

This post-1.0 feature preview adds **Library → Backup & recovery**, verified personal archives and recovery as additional rig/take copies. It includes referenced sound files, recorded stems/reamps, original tone snapshots, saved boards/scenes, names/notes/favorites and practice/review sections. The current tone and existing entries are preserved. The editor displays the Preview channel explicitly.

Copying, extraction and catalog reload run on the take worker, serialized with existing take operations. Progress, cancellation, missing-file/error reports and output reveal are available in the library. Recorded audio uses a streaming ZIP writer rather than buffering whole takes in memory. Current limits are below 2 GiB, 8,192 payload files and 2,048 takes.

Public packages retain the original built-in tones and exclude uncleared third-party sounds. No DSP/host parameter IDs, plugin identity or existing rig schema are changed. Version numbers remain consistent across app, editor, VST3, installer and source package. Signing remains deferred; preview is not a signed stable commercial release.

See [backup instructions and recovery limits](BACKUP-AND-RECOVERY.md). DAW sessions, MIDI/device preferences and external media projects must still be saved separately. Large-library ZIP64/subset backups and automatic crash/snapshot recovery are planned follow-ups.

## Validation on Windows, 2026-10-08

All 193 UI tests in 21 files and all four CTest suites passed. Native validation included the optional 103-file personal sound bank, the supplied October sound ZIP audit and the demanding JCM800/two-capture/long-ambience board. The final standalone and VST3 Release builds succeeded. The rebuilt VST3 passed pluginval 1.0.4 at level 10 with seeds 10002, 10003 and 10004.

Isolated installer installation, upgrade, uninstall, shortcut/VST3 checks and preservation of test user data passed. Backup tests include an external-recording destination collision, which must reject without replacing the recording, and fast restore completion between UI polls. Preview package integrity is checked separately when packaging the committed revision.

These automated checks do not establish second-PC recovery, fresh-PC prerequisites, actual DAW/hardware acceptance or signing. The final browser layout recheck was unavailable after the power loss because the sandbox runtime could not initialize; the earlier minimum-size inspection informed the scroll/flex and keyboard fixes. The heavy capture benchmark exceeded the callback budget in some 128-frame blocks, while 256/512-frame runs remained within budget in that run. It is an offline timing measurement, not proof of interface latency or uninterrupted live playback.
