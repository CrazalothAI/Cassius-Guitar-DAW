# Cassian 0.8.1 preview

In Board → Edit scenes, select a stored slot, change Scene name and choose **Rename scene**. Rename changes only the saved name, preserving its parameter snapshot and board topology. Selecting slots while the scene editor is open no longer recalls them, so organizing inactive scenes does not change the live guitar tone. Close scene edit to resume the normal click-to-recall behavior.

Rename keeps the active scene index and its edited status, preserves every live control and file, and marks a saved complete rig edited because its bank metadata changed. Rename never prepares a graph or resets effect tails. Save the complete rig to retain names. Empty slots, blank/overlong names and invalid indices reject without mutation; loading/unresolved assets block edits. Existing rig/scene schemas are unchanged.

## Validation

Native checks verify saved-tone/topology isolation, unchanged live controls, active/edited identity, saved-rig edited status, invalid names/slots, alternate-rate native session restore, portable packs and loading protection. UI tests cover edit-only slot selection, trimmed rename requests, empty/unchanged/blank names and loading. All 176 UI tests and all four native suites passed on Windows on 2026-10-07 with the optional 103-file bank; Release tests/VST3 and embedded frontend built; final standalone/installer packaging follows in 0.9.0. Real interface/DAW audition remains a separate release gate.
