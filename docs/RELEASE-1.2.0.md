# Cassian 1.2.0 preview — large personal backups

**Library → Backup & recovery** supports personal archives up to **32 GiB**. Complete, tone-library and selected-take scopes retain their existing behavior. Large archives automatically use stored ZIP64 with 64-bit entry lengths, directory offsets and data descriptors. Small archives retain classic ZIP, and existing schema-1 archives remain readable. Use this release or newer for large ZIP64 recovery. The payload budget reserves 32 MiB for metadata/overhead; file/take count limits remain 8,192/2,048.

Audio is copied in 64 KiB chunks. SHA-256 reads now use a 64 KiB reservoir rather than tiny operating-system reads, with cancellation checks during hashing too. Creation reads back the archive and verifies it before replacement. Restore preserves its existing additive copies, source protection and catalog commit behavior. Allow room for both the archive and expanded verification copy. This improves archive transport, not the supported maximum duration or RIFF format of a recording.

The bundled JUCE ZIP reader does not parse ZIP64 size/offset extensions. A separate bounded reader therefore accepts the single-disk, stored, unencrypted profile Cassian emits. It rejects inconsistent local/central headers, overlapping or displaced entries, truncated descriptors, unsupported flags/attributes, oversized offsets/expansion and invalid UTF-8 names before extraction. ZIP64 data and manifest CRCs are checked alongside manifest SHA-256 payload hashes. Existing safe-path, case-collision, symbolic-link and rig validation still apply. Recompressed or split ZIP64 archives are unsupported; retain the original app-created archive. The format follows [PKWARE's ZIP specification](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT).

No DSP, rig schema, automation identity or plugin identity changes. Public packages exclude private third-party sounds. Signing and actual-device/DAW/second-PC acceptance remain pending.

## Reproduce validation

After building Release native tests, run `scripts/test-backup-archives.ps1` for the normal archive/native suite and optional independent Python ZIP CRC verification. The `-Large` option adds an above-4-GiB synthetic WAV-padding archive through full create/restore, byte-identical comparison and cancellation during checksum preparation. It requires at least 17 GiB free on the scratch volume and runs for several minutes. Evidence/logs go into a unique `build/backup-validation` folder. The retained large archive is removed afterward unless `-KeepArchive` is supplied; these fixtures are excluded from public source/installer packages.

The large fixture tests 64-bit archive lengths/offsets, not long recording duration, RF64 audio support, second-PC recovery or physical power-failure safety. Normal CI keeps the fast ZIP64 fixtures enabled; the large disk-consuming case is opt-in.

## Validation results

On Windows, 2026-10-08, all **201 UI tests in 22 files** and **four native suites** passed. The native run included the optional 103-file personal bank, October sound-pack fixtures, demanding capture board and full large-backup path. Its synthetic archive was **4,295,218,061 bytes**, above the 32-bit boundary. Creation and additive restore succeeded; the recovered large payload matched its source SHA-256. Cancellation during checksum preparation preserved the valid archive. Python's independent `zipfile` verifier checked the archive CRCs. The four native suites took 184.75 seconds with the opt-in large case enabled; this is fixture validation, not a real-library performance guarantee.

Normal tests also cover small classic archives, forced small ZIP64/UTF-8 payloads, oversized declarations, displaced/overlapping local headers, encryption/compression/attribute rejection, out-of-file 64-bit locators, descriptor mismatches, corrupted payloads and unchanged catalogs after rejection. A separate small ZIP64 fixture passed independent Python verification before the large run.

Release standalone/VST3 builds, three independent pluginval level-10 runs (seeds 10002, 10003, 10004), isolated Windows installer install/upgrade/uninstall with preserved user data, version checks and signing-configuration logic passed. Package integrity/readiness reports are regenerated against committed source during packaging. Signing-provider integration and actual acceptance remain pending. Browser layout reinspection is still unavailable after the sandbox initialization failure.
