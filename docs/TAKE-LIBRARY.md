# Take library and offline reamping

In standalone, open **Takes**. Finished guitar recordings enter automatically after both WAV headers are finalized. Use **Import take folder** for an earlier Cassian folder containing matching mono `Guitar dry.wav` and stereo `Guitar processed.wav` files. New recordings save `Original rig.json` at the recording request and `Cassian take.json` after finalization. The rig snapshot represents the starting settings; parameter changes during recording are not recorded as automation.

Search names and creation dates, filter Favorites, select a take, edit its name and favorite flag, then press Save. Names do not rename files. The catalog lives at `takes.xml` under the existing shared library directory. It references recording folders rather than copying audio. Keep those folders available; a moved folder must be imported at its new location. Names and favorites persist across app launches. Independent catalog writes preserve other take entries; simultaneous edits to the same entry use the last saved entry.

## Review

Select Original processed, Dry DI or an exported reamp version, then press **Listen**. Review uses its own seek and volume controls, starting at −12 dB. It bypasses guitar processing, pauses backing playback and mutes live guitar and the metronome while playing. Master and the output limiter still protect monitoring. **Stop review** restores live monitoring; backing playback stays paused. Starting practice playback or recording stops pending/active reviews.

Review uses the practice engine's worker-side decoding and filtered resampling. Each decoded track is limited to 256 MiB of stereo float audio at the interface rate; longer takes can still be reamped and opened externally. Practice and review can retain separate decoded buffers. DAW playback and recording remain owned by the host, so this panel's native transport is standalone-only.

## Reamp versions

Set up the desired amp, pedals, cabinets and effects, then choose **Reamp with current rig**. A separate processor prepares the snapshot's assets and streams the original dry file through the guitar chain on a worker. It does not change the live rig or depend on audio callbacks. The current Input gain and guitar processing are included; Master, output limiter, accompaniment and clicks are excluded.

Successful exports add uniquely named `Reamp <id>.wav` and `.json` files in the take folder and a version in the selector. The WAV is stereo 32-bit float with the original dry file's sample rate. Choose **Effect tail** before reamping: no extra tail, 1, 2 (default), 5 or 10 seconds. Extra frames feed silence through the same processor so delays and reverbs ring out; version duration and tail length persist in the catalog. It retains headroom above 0 dBFS, so use suitable playback gain. Original dry and processed files remain untouched. **Cancel export** discards an unfinished audio output. Preparation/read/write failures report an error instead of adding a successful version.

Reamping begins with fresh DSP histories. Model/effect latency remains and is not compensated. The selected tail has a fixed duration; effects with longer decay can still be cut at its end. Existing reamps without duration metadata keep their original duration in the editor. Each take permits up to 64 reamp versions; the catalog permits up to 2,048 takes. Standard WAV size limits apply. Incomplete recordings are marked and cannot be reamped from the panel. Recording must finish before starting review or an export.

Rig snapshots contain references to the selected assets, not a portable asset bundle. Assets must remain available for a future export or recall. Use Library relinking when a capture or cabinet has moved. This update does not add waveform editing, deletion, automatic catalog migration, or latency compensation.

## Recover a saved tone

Select Original processed or Dry DI and press **Load recorded rig** to restore the starting tone. Select a reamp version and press **Load reamp rig** to restore the tone used for that export. Save current edits first: recovery replaces the current tone, board, MIDI assignments and scenes. Input calibration, Master, metronome and Play Along settings stay as they are. Accepted recall stops review and pauses backing playback.

Snapshot reads run on the take worker; full document validation and prepared rig recall use the existing engine path. Missing assets reject before changing the current rig. Corrupt assets may fail during preparation, retaining the current tone and reporting the engine error. Recording, exports, incomplete takes and rig loading block recovery. Older takes without a snapshot can still be reviewed and reamped. Recovery does not change WAV files, snapshots or catalog entries. Recording-time knob changes are not reconstructed.

## Verification

0.4.1 checks on Windows on 2026-10-06: 139 UI tests, all four native suites with the optional 103-file bank, Release standalone/VST3 builds, version checks and isolated installation/upgrade/uninstall passed. Recovery checks cover exact originals/reamps, legacy migration, unchanged listening/calibration controls and files, missing assets, invalid documents, pending reads and worker callbacks. See [0.4.1 evidence and limits](RELEASE-0.4.1.md).

Windows checks on 2026-10-06: all four CTest suites (with the optional 103-file bank) and 134 UI tests pass, with Release VST3 and side-by-side standalone builds. Native regressions cover catalog reopening, metadata edits, stored-audio review, cancellation, automatic recording registration and actual offline WAV rendering. Reamp checks include current Input gain, Master exclusion, exact rig snapshots, byte-identical originals, optional delay tails and persisted extended durations. Soundtrack checks include trimming at 44.1/48/96 kHz, boundary fades, clamping, synchronized backing and tail silence. See [0.3.0 notes](RELEASE-0.3.0.md).

Live interface listening, long-run recording/export stress and Linux/DAW host testing remain outstanding. This is a development update rather than a commercial-release qualification.
