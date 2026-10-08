# Cassian 1.1.1 preview — automatic tone recovery

Standalone now checks the current complete tone once a minute and writes changed reference snapshots on a dedicated worker. Open **Library → Automatic tone recovery** to turn automatic snapshots on/off, capture the current tone immediately, or add a recovered preset. Recovery never automatically recalls a tone or overwrites the one being played. Choose the recovered preset from Library Presets when ready.

Up to 64 valid snapshots are retained across runs and app instances. Saves use atomic file replacement and verify a SHA-256 checksum of the state text; damaged documents are reported and omitted from the picker. Unchanged states do not consume additional history during the same run. Enqueued saves finish during normal shutdown. Preference changes persist locally. Catalog writes during recovered-preset import roll back the in-memory library when saving fails.

The once-a-minute capture runs on the message thread and skips recording, loading, exports and personal backup/recovery. The worker performs file writes, scans and retention. This feature is enabled only by standalone startup; VST3 validation and DAW instances do not create automatic recovery storage. No audio processing, host parameter identities or rig schema are changed.

Snapshots reference existing NAM/WAV files and contain tone parameters, boards, scenes and asset references. They do not copy sounds, recordings, devices, MIDI assignments or DAW projects. They cannot restore a deleted/changed capture or failed drive. Use the 1.1.0 personal backup workflow on another drive for those sounds/takes, and save DAW projects separately. Tone snapshot history/preferences are not included in a personal archive; recover important tones into saved presets first.

Files are stored in `Cassian/Library/recovery-tones`. The checksum detects damage; it does not establish authenticity or replace validation. An abrupt power loss can lose changes since the last completed minute check, and atomic replacement does not guarantee survival of a storage-device failure. Damaged snapshot files are retained for inspection rather than silently erased. Clock changes can affect chronological retention; snapshots created at the same millisecond use filename ordering.

Signing and second-PC/real-host acceptance remain deferred/pending. This is a feature preview, not an approved stable commercial release.

## Automated validation on Windows, 2026-10-08

All 196 UI tests in 22 files and all four native suites passed. Native checks cover complete-state round trips, duplicate suppression, corrupt-checksum/path rejection, persistent automatic preferences, the 64-entry retention limit, queued-save completion at shutdown and additive recovery with unchanged current parameters/rig identity. The optional 103-file personal sound bank and October ZIP audit were included. Release standalone/VST3 builds and three level-10 pluginval runs (seeds 10002, 10003, 10004) passed. The isolated Windows install/upgrade/uninstall test also passed.

The timed one-minute path still needs real standalone playing/restart trials, and second-PC recovery is pending. Native scratch tests exercise capture/storage APIs; they do not simulate a physical power failure or establish live-interface performance. Browser layout inspection remains limited by the sandbox initialization error after the power loss.
