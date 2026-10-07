# Cassian 0.5.4 preview

Select a saved rig in Library → Presets to export its saved tone without recalling it:

- **Export saved references** writes a Cassian JSON document containing the saved settings and sound references. Missing files can remain referenced for later relinking.
- **Export saved pack** bundles the saved settings and distinct referenced sounds into a portable ZIP. Missing dependencies or an invalid saved document disable this action; native packaging independently validates files and content hashes.

Current unsaved tone edits, playing identity and library annotations remain intact. Export uses the saved snapshot and its current saved name, migrating older valid schemas on an isolated tree. Tags/notes/styles on the library entry are not part of the rig document. The save picker starts with a legal filename derived from the saved name. Reference documents require their original sounds; packs include up to seven deduplicated referenced assets. Share sound files only when you have distribution permission.

The picker captures a snapshot before opening for both current-rig and saved-rig exports. Portable export passes that snapshot to the existing asset worker rather than recapturing the live rig when the picker closes. The queued document is copied so later caller changes cannot affect it. Saved packs resolve missing paths through managed stable IDs; older path-only references receive verified content identities during packaging. Missing/changed/invalid files reject without replacing an existing destination.

No host parameters, sound files or rig schemas were added. This patch completes the three-patch request following 0.5.2 review waveforms and 0.5.3 library discovery.

Portable documents omit the local comparison identity/baseline and library deletion bookkeeping. Their asset paths stay archive-relative; importing creates a new library identity. Reference JSON retains paths for relinking as expected.

## Validation

Validated on Windows on 2026-10-07: all 161 UI tests and four native suites passed with the optional 103-file bank. Release standalone/VST3 builds, binary version checks and isolated installer installation/upgrade/uninstall passed. Installer checks include shortcuts, optional VST3 placement, version registration, preserved user data and private sound hashes.

New checks cover saved versus unsaved tone settings, current saved identity/name, isolated legacy migration, missing-reference export, rejected incomplete packs, relinked stable IDs, worker snapshot preservation, legacy path-only assets and UI format selection without recalling a tone. Portable-document checks confirm local comparison paths/identities are omitted. Existing regressions cover stored-audio review, actual WAV exports, complete recall, board/scene/session migration and all exact sound recipes.

Live interface audition, real DAW hosts, Linux and fresh-PC prerequisite setup remain manual checks. Builds remain unsigned development previews; third-party sounds remain private until redistribution grants are recorded.
