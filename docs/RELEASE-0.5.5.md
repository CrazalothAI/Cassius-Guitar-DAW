# Cassian 0.5.5 preview

Takes now has optional searchable notes. Keep tuning, tempo, song and performance reminders with a recording; save notes independently of names/favorites. Empty notes clear the annotation. The catalog persists multiline notes across restarts without changing original WAVs or rig snapshots. Notes are limited to 2,000 characters.

Failed note and name/favorite writes restore previous in-memory metadata instead of presenting an unsaved change as successful. Existing catalogs work without migration; notes default to empty. No host parameter or tone schema changed.

## Validation

All 163 UI tests passed on Windows on 2026-10-07. Native verification covers persistence, multiline text, limits, unknown takes, unchanged audio and rollback with an unreadable catalog. All four native suites passed with the optional 103-file bank; Release tests/VST3 and the embedded frontend built successfully. The final 0.6.0 milestone will rebuild/package standalone and run installer checks. Live interface, real DAW and fresh-PC checks remain manual; public sound redistribution remains pending.
