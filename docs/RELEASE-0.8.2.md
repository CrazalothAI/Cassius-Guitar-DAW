# Cassian 0.8.2 preview

Board → Edit scenes now offers **Scene copy destination**. Select a saved source, enter a name, choose another slot and use **Copy saved scene**. Occupied targets explicitly say **Replace scene N with copy**. Copy uses the source’s saved parameters and board, even if you have since changed the playing tone.

Copy leaves the live sound, files, global listening controls and effect histories unchanged. Source and destination are independent snapshots. Replacing the active saved slot clears its highlight; replacing another slot retains the current scene’s active/edited status. Save the complete rig to persist copies. Invalid slots, same-slot copies, empty sources, blank/overlong names and unresolved/loading assets reject before mutation. Rig/scene schemas and host parameter IDs are unchanged.

## Validation

Native checks cover exact saved-tone copying versus live edits, unchanged live controls/topology, active replacement, independent subsequent source edits, invalid requests, alternate-rate session restoration, portable packs and loading protection. UI tests cover destination selection, explicit occupied-slot replacement and empty-source disabling. All 177 UI tests and all four native suites passed on Windows on 2026-10-07 with the optional 103-file bank. Release tests/VST3 and embedded frontend built; final standalone/installer packaging follows in 0.9.0. Real guitar/interface/DAW audition remains a separate release gate.
