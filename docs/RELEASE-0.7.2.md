# Cassian 0.7.2 preview

Select a pedal in Board and choose **Reset controls** beside Replace pedal. Reset restores only that instance's controls and output trim to their parameter defaults. It preserves bypass, assigned capture/ambience file, identity, automation slot, lane, order, other pedals, amp/cabinets, scenes and listening/calibration controls.

Reset participates in the existing 32-entry processor-local board Undo/Redo history. Undo restores the pre-reset pedal settings; Redo reapplies defaults. A pedal already at defaults causes no reload and preserves Redo history. Reset is disabled while a rig/board prepares and unavailable for missing/deleted identities. Like other prepared board edits, a real reset briefly fades the guitar and restarts tails; it is not gapless. Ordinary knob gestures are still host automation rather than board-history edits.

## Validation

Native integration checks cover independent duplicate isolation, default controls/trim, unchanged bypass and all other parameters, stable board topology, Undo/Redo with subsequent amp/listening edits, no-op Redo preservation, captured file retention and atomic rejected identities. UI checks exercise the selected identity and loading protection. All 173 UI tests and all four native suites passed on Windows on 2026-10-07 with the optional 103-file bank; Release tests/VST3 and the embedded frontend built. Final standalone/installer packaging follows in 0.8.0.
