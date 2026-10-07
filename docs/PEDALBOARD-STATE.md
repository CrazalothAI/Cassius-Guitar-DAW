# Pedalboards and state compatibility

Implementation checkpoint: 2026-10-07. Cassian supports the original fixed chain and an explicitly enabled serial board. Automatic migration preserves the fixed audio path; conversion is a user action.

## Editing a serial board

In **Board**, choose **Enable serial editing**. Pedals appear in **Before amp** and **After cabinet** lanes. Select a pedal to edit its controls, assign captured files, duplicate, replace, remove or move between lanes. Drag cards or use the accessible arrow buttons to reorder. Bypass ramps over 30 ms; the bypassed signal is exactly dry after the ramp settles. Output trim belongs to each instance.

The runtime supports compressor, built-in overdrive, captured NAM pedal, EQ, modulation, chorus, delay, reverb, recorded ambience, wah, dedicated distortion, plate-style reverb and spring-style reverb. Overdrive, distortion and NAM pedals are restricted to the mono pre-amp lane. Other effects can run mono before the amp or stereo after the cabinet. One amp and the existing parallel cabinet A/B stage remain shared.

There are two kind-qualified automation slots per type, with at most 16 reserved blocks across the board. Removal creates a tombstone and does not release the slot. Replacement reserves a new identity/slot. Reordering never changes parameter identity. If both slots for a type are reserved, Undo can restore a removed pedal; choose another saved rig to start another board. This prevents old automation from silently controlling a newly added effect.

Reset controls restores the selected instance and its trim while preserving bypass/file/identity; Undo restores the settings. Resetting defaults does not reload or alter history.

Undo/Redo keeps the latest 32 structural edits, resets and capture assignments in this processor instance. It restores pedal controls, topology and pedal/ambience files, retaining the current amp, cabinets, scenes and listening settings. Ordinary knob gestures use host automation and are not added to this structural history. Loading another complete rig resets the history; it is not persisted across app restarts.

Conversion moves the neural pedal ahead of the amp's drive/tight processing and turns embedded clean compression into an ordinary pedal. Compressor Off remains bypassed. The serial reverb has unity dry output at zero mix instead of the fixed chain's legacy doubled dry level. Conversion can therefore change sound and level; Undo restores the fixed routing. Built-in starting presets change their existing controls without removing independent duplicate pedals; use a saved complete rig to recall exact topology and files.

## Document and automation contracts

New external documents and saved library rigs write integer `schema: 3` and `AmpSuiteState` XML. Current complete documents contain 215 PARAM rows: the original 87 IDs and host indices are unchanged, and 128 append-only controls follow them. Missing controls from the appended wah/distortion/plate/spring families default only when their block type is absent, including reserved/deleted blocks; other required rows must be present. Slot 0 reuses its original kind's parameters; slot 1 uses IDs such as `BOARD_EQ_1_EQ_FOCUS`. Types lacking an existing switch gain kind-qualified ON controls; every slot has a `BOARD_<TYPE>_<SLOT>_TRIM`. Ambience has new slot-qualified mix controls.

Schema 1 still requires its original first 41 controls and defaults later ones. Schema 2 requires the original complete controls and a fixed board. Both default missing independent controls. Native sparse fixed sessions retain historical handling. New serial sessions validate a complete isolated document and prepare the graph before replacing the live state. Present invalid/future boards never fall back to fixed routing.

The original `PEDALBOARD version=1 runtime="legacy-fixed-v1"` retains exactly its eight bindings and all prior field validation. Its deterministic IDs are `legacy.<type>`, slots are zero, trimDb is zero, and the neural assetKey is `pedal`. Missing board metadata in legacy documents migrates to this description without changing audio.

Serial boards use `version=2 runtime="serial-v1"`. Each BLOCK has exactly `id`, `type`, `automationSlot`, `lane` and `deleted`, with no children. IDs are unique 1–64 character ASCII identities; type/slot pairs remain unique including tombstones. Slots are integers 0/1, lanes pre/post, and deleted is 0/1. Validation rejects duplicate trees, unknown types/properties, unsupported channel placement, oversized state, invalid IDs, fractional/wrapping integers and duplicate automation bindings. XML's exact numeric strings are accepted. PARAM rows remain authoritative for values/bypass/trim.

## Files, scenes, packs and audio

Top-level model/ir/irB references retain the amp and cabinets. pedal/pedal1 identify separate recurrent NAM engines, even when the files have identical content. ambience/ambience1 identify independent convolution responses. Their stable IDs, managed paths and all seven file references travel through rigs, A/B, native sessions, takes and portable packs. Managed copies preserve original capture names for filename-based compatibility detection. Relinking verifies content hashes and rebuilds affected independent slots.

Scenes write JSON version 3 with complete tone parameters and their board XML. Version 1 inherits the shared rig's identities; version 2 defaults new controls. Files remain shared per slot across scenes. Same-topology recall changes parameters without resetting histories. Different topology prepares/replaces the graph; the recalled scene remains selected. Input calibration, Master, metronome and Play Along listening settings stay global.

Portable packs contain one validated document and up to seven deduplicated assets, at most 64 MB per asset and 452 MB expanded total. Invalid board state rejects before extraction or destination replacement. Existing take snapshots and original WAVs are not rewritten by reamping. A pack containing third-party files does not grant redistribution permission.

Graph construction, model preparation, WAV reads and long convolution construction occur off the audio callback. Processing uses cached atomic parameter pointers and bounded scratch buffers. Fully bypassed captured pedals stop running their recurrent engines after the fade; delay/reverb receive silence to drain tails. Publication follows the existing guitar fade and guarded swap. Serial topology changes restart effect tails; independent old-scene tails and gapless switching are not implemented. Returning to the fixed path clears frozen fixed delay/reverb buffers. Host tail reporting is 60 seconds to allow two serial 30-second responses.

Recorded ambience preserves file length, channel timing and amplitude: no automatic trim or normalization. Blend interpolates between dry and the captured response, and trim adjusts its level. Captured decay/repeats are fixed; Size, tempo sync and arbitrary repeat feedback are not recreated from an IR. Adjustable delay/reverb remain separate block types.

The metronome, backing buses, listening-only Guitar balance/Focus, Master and output protection remain outside the recorded/reamped guitar board.

## Verification and limits

0.6.1 adds two independent original distortion slots and 0.6.2 adds two independent plate slots, appending host controls after all prior slots. Absent-family migration applies to rigs and scenes; documents using these types must contain complete controls. See [distortion](RELEASE-0.6.1.md) and [plate](RELEASE-0.6.2.md).

0.4.0 appends two independent wah slots and their controls after the existing host parameters. Pre-0.4 rigs/scenes without wah blocks default the new family; wah-containing boards require complete wah controls. Manual and envelope modes work in both lanes, with linked stereo detection after the cabinet. See [wah compatibility and setup](WAH-PEDAL.md).

Native tests cover independent controls, order-dependent audio, exact settled bypass, kind-slot reservation, reorder/replacement, Undo/Redo, scene topology recall, native restore at an alternate host rate, atomic invalid recall, two independent NAM slots and deduplicated portable round-trip. Synthetic stereo ambience verifies captured timing/gain. Existing legacy migration comparisons remain sample-identical, and Play Along recording isolation remains covered.

The optional supplied-pack manifest validates all 65 October files without embedding them in test fixtures or releases. `CASSIAN_ACTUAL_SOUND_LIBRARY` adds a demanding board with the supplied JCM800, two independent Klon engines, a 20-second ambience response and six other effect types. It reports offline guitar processing at 48 kHz / 128, 256 and 512 samples, including blocks exceeding their time budget. Timing is diagnostic evidence, not a guarantee for every capture or live interface. For heavy boards, try 256/512 samples and monitor the app's dropout/overrun alerts. Real guitar auditions, sustained device tests, Linux and real DAW automation/session workflows remain release checks. Packaging is local and does not publish or merge this branch.

0.7.1 appends two independent spring-style reverb slots after Plate, with absent-family migration and complete-control validation. See [spring](RELEASE-0.7.1.md).
