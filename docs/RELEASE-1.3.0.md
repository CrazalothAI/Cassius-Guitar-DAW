# Cassian 1.3.0 preview — recording recovery and simpler downloads

This increment adds three recording safeguards and removes duplicate installer choices.

## Recording checkpoints

Before a recording is armed, an atomic metadata write marks the new take incomplete. On its disk worker, Cassian updates and flushes all three WAV headers about every two seconds when audio has been written. The audio callback adds no file I/O or locks. A checkpoint/stream failure stops the take as incomplete; successful finalization atomically replaces the initial metadata. A per-folder interprocess lock remains held while the recording is active.

## Explicit recovery into copies

Takes → Recover interrupted recording accepts readable uncompressed RIFF dry/processed stems at the same rate. Physical chunk bounds are checked independently of the decoder so truncated payloads cannot silently become zero-filled successful copies. Recovery copies the common prefix into private staging, optionally preserves synchronized backing and a bounded rig snapshot, compares source checksums before/after copying, writes a local outcome report, then publishes a unique recovered folder. Original files are unchanged. Cancellation cleans only its new staging; failed catalog registration rolls back memory and removes only that copy.

Recovered copies await review. Listen to Original processed, acknowledge review and Confirm recovered take. For audio beyond the existing 256 MiB review-buffer limit, Open take folder and explicitly acknowledge review in another player. Both paths validate all copied stems against recorded checksums before enabling export/reamping. Approval lives in the take catalog and survives reopening; changed recovered audio loses approval on reimport. Missing/unequal/short backing is omitted with an explanation rather than blocking usable guitar recovery.

The workflow recovers a readable checkpoint prefix, not unchecked raw tails or arbitrary damaged headers. RF64 input, automatic recording discovery/resumption and session automation reconstruction are not implemented. Processed output is bounded to approximately 2 GiB with a free-space check. Locks protect new Cassian recordings; checksum comparisons detect source changes from other writers. Actual power-failure survival and second-PC behavior remain untested.

## One current installer

Normal packaging puts packages and evidence in `build/releases/1.3.0` and leaves the current `Cassian-Setup.exe` and runnable `Cassian.exe` in the project root. Recognized old top-level generated installers, ZIPs and package reports are removed only after successful packaging, with no recursive scan. Explicit custom output destinations do not clean the project root.

GitHub artifact upload and release publication use exact current-version paths. The latest release contains one `Cassian-Setup.exe`, a versioned portable ZIP, matching AGPL source, build metadata, readiness report and checksums. Historical installers are not swept in with wildcard uploads; the existing current-main/concurrency guards remain. Private sound files stay excluded. Code/signing/installer identities and existing rig/automation identities are unchanged.

## Validation

The Release standalone, VST3 and native test executable rebuilt successfully. All **206 UI tests across 22 files** and **four native suites** passed. Native recovery checks cover actual checkpoint headers while a writer is active, active-folder rejection, unequal stems, omitted/aligned backing, cancellation cleanup, truncated WAV rejection, source preservation, copied-stem checksum validation, changed backing, external review, approval persistence/invalidation, actual WAV export and failed registration rollback. The normal classic/ZIP64 archive suite and independent Python CRC verification also passed; local evidence is in `build/backup-validation/11914dc92b504161ae8f344a4821ca18/BACKUP-VALIDATION.json`. The separate above-4-GiB transport test passed for 1.2.1; it was not repeated for this recording change.

Independent pluginval passed at strictness 10 for seeds 10002/10003/10004 against the final VST3. Isolated installer install/upgrade/uninstall preserved user data. Version, signing-logic and release-layout tests passed. Layout tests check exact-file cleanup, retained current/unrelated files, no recursion, repeatability and custom/default output paths. Package integrity/readiness reports accompany the committed-source download; signing-provider integration and visual inspection remain untested in this environment.

The owner reports practical sound tests tried so far are passing. Real interface/DAW/MIDI/camera, fresh-PC and power-failure evidence remains separate. This remains an unsigned preview, with no signing provider or private sound redistribution approval.
