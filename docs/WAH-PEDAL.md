# Wah and envelope filter

In **Board**, enable serial editing if needed, select **Wah**, choose its position and press **Add pedal**. Up to two independent wahs share the existing 16 reserved board slots. Each has separate controls and MIDI/host automation identities. Duplicate, bypass, reorder, replacement and Undo/Redo work as with other pedals.

- **Manual:** Position sweeps from a low vowel to a brighter one. Use it for a fixed vocal lead sound, turn it while playing, or assign **Wah 1 position** / **Wah 2 position** to a CC in MIDI settings. Learn, inverted direction and channel selection work with those assignments. They affect only their own instance's Position; add/enable that wah and select Manual to hear the expression sweep.
- **Envelope:** picking opens the filter and note decay closes it. Sensitivity changes how hard you need to pick. Start at 0 dB; reduce it if the sweep stays bright or raise it for quieter pickups. Position is used only in Manual mode.
- **Resonance:** increases the focus of the vowel. **Blend** restores some dry tone; **Output trim** balances the pedal. Start before the amp for rock leads and rhythmic cleans. After-cabinet placement works in stereo and can be useful for clean textures.

**Envelope Clean** and **Vowel Lead** are complete built-in starter rigs. Neither requires a downloaded capture. As of 0.8.0 there are 22 built-in rigs plus the existing 20 exact-capture recipes; imported pedal/amp/cabinet choices remain available.

The filter sweeps about 350–2600 Hz with a linked stereo envelope detector and separate channel histories. Sensitivity, resonance, blend and sweep changes are smoothed. Envelope attack/release are fixed at 8/120 ms. Processing uses prepared filters and bounded storage; it adds no block latency or sound-file dependency. Bypass settles to exact dry audio. These are Cassian's own wah/filter voices; this does not emulate a named hardware pedal.

Existing parameter IDs and their order are unchanged. New controls are appended under `BOARD_WAH_0_…` and `BOARD_WAH_1_…`. Pre-0.4 rigs/scenes without wah blocks receive defaults for the new family. Missing/invalid controls in wah-enabled documents are rejected. Reserved/deleted wah slots still retain their bindings. Rig schema 3 and scene version 3 remain in use; complete 0.4 rig exports include the appended controls and require 0.4 or newer to import, even when the wah is unused. Original takes and snapshots are never rewritten.

Automated checks cover frequency response at 44.1/48/96 kHz, pick/sensitivity-dependent tone, block partitioning, silent stereo-channel isolation, zero blend, independent instances, MIDI targets/inversion, old-document migration, rejected recall and native session recall at a different rate. Live pickup/expression-pedal audition and real DAW host testing remain manual checks.
