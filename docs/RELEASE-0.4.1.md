# Cassian 0.4.1 preview

Takes now recovers the tone saved with a recording or reamp version.

- **Load recorded rig** restores the original starting rig from either Original processed or Dry DI.
- **Load reamp rig** restores the exact snapshot saved with the selected rendered version.
- Input calibration, Master, metronome and Play Along settings remain unchanged. The tone, pedalboard, MIDI assignments and scenes follow the snapshot; save current edits before loading it.
- Snapshot reads run on the take worker. Existing validation, asset resolution and prepared recall protect the current rig when a document or required asset cannot load.
- Recovery is blocked during recording, exports, incomplete takes and rig loading. Older takes without snapshots retain review/export functionality.
- Recovery does not rewrite original audio, reamp audio, rig snapshots or the catalog. Recording-time parameter changes are not stored automation.

See [take workflow and limits](TAKE-LIBRARY.md). This patch adds no new host parameters or document schema.

## Validation

Validated on Windows on 2026-10-06: all 139 UI tests and four native CTest suites passed, including the optional 103-file bank. Release standalone/VST3 builds, binary version checks and isolated installation/upgrade/uninstall passed. Installer checks include optional VST3 placement, shortcuts, version registration, user-data preservation and all private-bank hashes.

Recovery regressions cover exact original/reamp snapshots, schema-1 migration through prepared recall, preserved input/Master/metronome/Play Along controls, byte-identical originals and reamp snapshots, missing-asset atomic rejection, missing/malformed/oversized documents, unknown versions, incomplete takes, outside-folder references, worker-side completion and duplicate pending reads. UI checks cover the selected version, unavailable snapshots, pending reads, recording/export/loading guards and actionable errors.

Live interface audition, Linux, real DAW hosts, fresh-PC installation and Clipchamp camera alignment remain manual checks. Binaries remain unsigned development previews. Public sound distribution still requires per-asset grants; the supplied private bank is excluded from public downloads.
