# Cassian 1.1.3 preview — selective take backups

Open **Library → Backup & recovery**, keep **Include recorded takes and reamps** enabled, then enable **Choose specific takes**. Select one or more recordings and create the archive. Complete backups remain the default; clearing the recording option still creates a tone-library archive.

Selected recordings include their whole take folders, metadata, dry/processed WAVs, backing stems, reamps and rig snapshots. Take review sections are included only when their audio content key matches a WAV in a selected folder. All library rigs, sounds and practice sections remain included. This is whole-take selection; per-version and range selection are not part of backup creation.

Selection uses stable catalog identities rather than folder paths or displayed names. Empty, duplicate, missing and ambiguous identities fail instead of silently widening or shrinking the archive. Refreshing the list prunes removed takes. Missing excluded recordings do not block protection of selected work. A missing selected source, changed catalog, cancellation or other failure preserves the previous destination archive.

The default filename, manifest scope (`selected-takes`), take count and completion message identify selective archives. They retain the existing schema-1 format and additive restore workflow: media is verified and relocated, recovered rig/take identities are new, and existing work/current tone stay intact. Older complete/tone-library archives remain readable. The streaming archive limit remains below 2 GiB; select smaller recording groups when needed. ZIP64 is still planned.

No DSP, rig schema, automation or plugin identity changes. Public packages exclude the private third-party sound bank. Signing, physical hardware/DAW acceptance and second-PC recovery trials remain pending.

## Validation

On Windows, 2026-10-08, all **201 UI tests in 22 files** and **four native suites** passed. Native validation included the optional 103-file personal sound bank, October archive intake fixtures and demanding capture board. The new regression cases cover selection boundaries, matching section keys, excluded missing files, exact recovered audio/reamps, catalog changes during copying, preservation of an existing archive and public worker completion. UI checks cover explicit identities, empty selection, refresh/pruning, tone-only switching and list errors/retry.

Release standalone and VST3 builds succeeded. Three independent pluginval level-10 runs passed with seeds 10002, 10003 and 10004. The isolated Windows installer passed app/shortcut/optional VST3 installation, upgrade, uninstall and preserved-user-data checks. Release version and signing-configuration logic tests passed; signing-provider integration is still pending. Package integrity and readiness reports are regenerated against the committed source during packaging.

Automated scratch tests do not establish second-PC recovery, fresh-PC prerequisites, physical power-failure safety, real hardware or DAW acceptance. Browser layout reinspection remains unavailable after the sandbox initialization failure.
