# Cassian 1.2.1 preview — validated section recovery

Personal archive recovery now validates optional practice/review documents before installing new files. The same read-only validator serves normal playback and recovery: supported document version, matching content key, bounded unique row identities/names, row count, finite numeric endpoints and sensible range lengths. Documents have a 64 KiB read limit and a 64-level nesting cap before recursive JSON parsing. Literal braces, brackets and escaped quotes inside strings remain valid. When matching WAV audio is archived, sections must fit its duration. Sections for unarchived external practice tracks are checked structurally; the actual duration is checked when those tracks load later.

Valid documents are installed atomically from the parsed content that was validated. Existing paths take priority and are preserved. Invalid documents or unavailable additions are skipped with a completion warning while rigs and recordings still recover. Verified incoming copies remain unchanged in the recovered folder, and skipped annotations do not prevent creating fresh sections for recovered audio. Backup creation can preserve damaged optional documents without blocking protection of required audio/rigs.

Each restore writes **Recovery report.json** beside the recovered media. It records rig/take and section counts, plus per-file outcomes (`added`, `kept-existing`, `skipped`, `write-failed`) and reasons. Choose **Show saved files** to locate it. A report-write failure is surfaced as a warning without undoing recovered work. The report is local; nothing is uploaded.

No DSP, rig schema, automation identity, plugin identity or archive schema changes. Existing classic/ZIP64 and tone-only/selected-take archives remain compatible. Public packages exclude private third-party sounds. Signing and actual-device/DAW/second-PC acceptance remain pending.

## Validation

The Release standalone, VST3 and native test executable rebuilt successfully. All 202 UI tests passed across 22 files. Independent pluginval checks passed at strictness 10 with seeds 10002, 10003 and 10004 against the final VST3 binary. Isolated installer installation, upgrade and uninstall checks preserved user data; version and signing-logic regression checks passed. Signing-provider integration is still untested.

All four native suites passed, including classic/ZIP64 compatibility, optional section validation, preserved conflicts, failed writes, deeply nested and oversized JSON, worker completion and recovery of a 4,295,218,061-byte synthetic archive. Python independently verified that archive's CRCs. Local evidence is in `build/backup-validation/f12f8065cab94e908120c0deb8f7a5df/BACKUP-VALIDATION.json`; large scratch media was removed afterward. The fixture exercises archive transport rather than long-recording RIFF support.

Visual inspection was unavailable in the current tool environment; actual-device, second-PC and power-failure acceptance remain pending. Release packaging checks the committed source identity and exact validated plugin binary; package-integrity and readiness reports accompany the local artifacts.
