# Cassian 1.1.2 preview — choose backup scope

**Library → Backup & recovery** now offers **Include recorded takes and reamps in new backups**, enabled by default. Clear it to create a tone-library archive of saved/current rigs, sounds, boards, scenes and practice sections. Take WAVs, reamps, take metadata and take review sections are excluded. A distinct default filename and archive manifest scope identify the choice; completion text distinguishes complete and tone-library backups.

This makes the backup workflow useful when a recording folder would exceed the current archive limit, or when recordings are already protected elsewhere. The tone-library archive still needs its own sounds to fit below 2 GiB. It is not selective take export or ZIP64. It does not read or require the excluded take catalog/audio, so a missing recording cannot prevent a tone-library backup. Complete backups retain their existing missing-source protection.

Both scopes use streaming writes, checksum verification, source-change detection, safe extraction and additive recovery. Restore reads the archive's actual catalogs; a tone-library archive adds rigs/sounds without importing recordings. Existing schema-1 complete backups remain compatible. Standalone automatic tone snapshots from 1.1.1 remain reference-only and separate from these personal archives.

Recovery also clears deletion markers for the sound identities explicitly restored from the archive, preserving unrelated deletions. This prevents a later normal library save from removing a recovered sound because it had previously been deleted locally.

No DSP, automation identity, plugin identity or rig schema changes. Signing and real-device/second-PC acceptance remain pending. Private third-party sounds are excluded from public packages.

## Automated validation on Windows, 2026-10-08

All 197 UI tests in 22 files and all four native suites passed. New checks cover complete-backup defaults, explicit tone-only requests, excluded recording/review payloads, valid tone-only recovery, an unreadable excluded take catalog, and recovered sounds surviving a subsequent normal library save while unrelated deletion markers remain intact. The optional 103-file personal bank, October ZIP audit and demanding capture board were included.

Final Release standalone/VST3 builds, three level-10 pluginval runs (seeds 10002, 10003, 10004), and isolated installer install/upgrade/uninstall with preserved user data passed. Package integrity/readiness are checked against the committed revision during packaging. This does not establish fresh-PC prerequisites, real DAW/MIDI/interface acceptance, second-PC recovery, signatures or physical power-failure safety. Browser layout reinspection remained unavailable after the sandbox initialization failure.
