# Cassian 0.6.0 preview

This recording/review milestone includes three separately tested patches:

- 0.5.5: searchable take notes and failed metadata-save rollback.
- 0.5.6: persistent named take sections, exact-version controls and worker-side saves.
- 0.6.0: copy a review/section A-B range into WAV export, export guitar-only excerpts, and report the actual completed output.

## Review to export

In Takes, listen to a version and recall a saved section or set review A-B. Expand Video soundtrack and press Use review A–B. The range is copied once; later loop changes do not change the export. Switching take/version resets bounds. Unloaded, starting, invalid or mismatched review ranges cannot be copied.

Export range as guitar WAV uses the selected Start/End and edge fade at neutral export gain, without backing. Export for video uses the requested guitar/backing balance. The existing full-version guitar shortcut remains available. All exports use 48 kHz / 24-bit stereo and existing bounded two-pass sample-peak protection. Original audio, live Master/Play Along settings and rig snapshots remain unchanged.

After finalizing the WAV, Takes reports source identity, actual frame-derived duration, backing inclusion and measured attenuation applied for peak headroom. Cancelled/failed exports retain the last successful report/path. Reports last for the running session; they are not loudness or true-peak mastering measurements. Review sections are local content-keyed annotations, not WAV/rig-pack metadata.

## Validation

Validated on Windows on 2026-10-07: all 168 UI tests and four native suites passed with the optional 103-file bank. Release standalone/VST3 and embedded frontend built successfully; binary version checks and isolated installer installation/upgrade/uninstall passed with the final executable, shortcuts, optional VST3, preserved user data and private sound hashes. Final packages are made from the clean committed revision, with root executable, aliases, checksums and sound contents independently checked after packaging. New checks cover review-to-export range copying and independence, extended reamp bounds, mismatched/startup review rejection, neutral guitar excerpt requests, completed-output reporting, measured written attenuation, cancellation, section persistence, metadata rollback and serialized UTF-8 catalog capacity with XML escaping.

Live interface audition, real DAW hosts, Linux, fresh-PC prerequisites and camera/Clipchamp sync remain manual checks. Builds are unsigned previews. Public packages exclude private sound assets until redistribution grants are documented; the owner's separate private build retains the 103-file bank. Catalog writes now enforce the existing 8 MiB read limit before replacing files; failed annotation writes restore the previous metadata. No host parameters or rig schemas changed. This milestone does not publish, push or merge the branch.
