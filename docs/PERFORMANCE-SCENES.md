# Performance scenes

Four scenes store variations of the current rig: for example Rhythm, Lead, Clean and Ambient. Open **Edit scenes** in the stage panel, select a slot, name it and press **Store current tone**. For an occupied slot, **Replace with current tone** overwrites that snapshot. **Clear scene** removes only the snapshot and leaves the live sound unchanged. Names accept 1–48 characters.

Click an occupied scene to recall it. The active scene is highlighted; **Edited** appears after changing its tone controls. Selecting an empty slot only selects where to store. Recall discards subsequent tone edits unless you replace the snapshot first. A restored rig/session starts without an active highlight until you recall or store a scene.

## What is saved

Scenes include every guitar-chain parameter: amp source/routing, drive and EQ, pedal controls, cabinet blend/alignment, compression, gate and effects. All four share the currently loaded amp, neural pedal and cabinet A/B files. They do not load alternate captures. Switching a built-in source still uses the current preallocated processing paths; selecting NAM requires an already loaded capture.

Scene banks now save version 2 with stable identities for the existing effects. Version-1 banks acquire the current rig's identities during migration. Board identity changes participate in Edited status; the audio chain remains fixed and scene recall still retains running effect histories. Unsupported board metadata rejects before changing a tone or native session. See [pedalboard state](PEDALBOARD-STATE.md).

Input gain, Master, all metronome settings and Play Along Guitar balance/Mix focus are global and stay unchanged. Practice transport, track level and recording/review are also independent. Use Amp Output or pedal Output for a scene's relative level; listen at a conservative Master setting while balancing the four tones.

The bank travels with native app/DAW state, saved complete rigs, A/B snapshots, take/reamp rig snapshots, JSON rig exports and portable packs. Save the complete rig after updating scenes to persist those edits in its library entry. Parameter-only starting presets change the live tone, leaving the bank available; **Edited** indicates a difference. Recalling another complete rig replaces the entire bank. Older rigs/sessions use an empty bank. Browser preview cannot store native scenes.

## MIDI recall

Open **Performance**, choose **Recall scene**, choose scene 1–4, then Apply and optionally Learn a controller. CC switches recall on the rising crossing into 64–127; PC recalls on each matching message. An empty slot can be assigned in advance but returns an error until populated. Assignments reference slot numbers in the current rig, so loading a different rig changes their target tones. MIDI assignments remain session settings and are excluded from rig exports.

The existing control worker handles MIDI recall with the editor closed. Host parameter notifications and scene operations occur outside the audio callback. They are asynchronous, not sample-accurate. Scenes use existing smoothing and the amp-source fade rather than preparing new engines. Parameters are notified individually, so this does not promise an atomic or gapless transition. Scene commands are rejected while a rig or stage asset is loading or unresolved.

Existing delay/reverb buffers are not reset during scene recall. Keeping their controls unchanged preserves their running history; changing mix, time, voice or bypass can alter or fade audible tails. Independent old-scene tail rendering and dual-engine crossfades remain future work.

## Validation

Windows automated checks on 2026-10-03 cover every saved parameter at its minimum/maximum, preserved globals, edited status, names/empty slots, native and complete-rig round trips, legacy sessions, malformed/incomplete/global/out-of-range scene rejection, failed asset rollback, pending-load blocking, MIDI scene selection and older mappings, portable pack persistence, and delayed-impulse continuity against an uninterrupted processor. UI tests cover storing/replacing/clearing, MIDI feedback, draft retention, loading controls, native errors and read-only browser preview. The production editor fits 860×620 with internal panel scrolling.

Physical foot controllers, live guitar listening, DAW-host automation/scene transitions and Linux have not been validated by these tests.

## Rename without replacing the tone (0.8.1)

Open Edit scenes, select a stored slot, edit Scene name and choose Rename scene. Slot selection in editing mode does not recall it. Rename changes only its saved title, preserving live controls and saved settings; the active scene and its edited status remain. A saved rig becomes edited because its scene bank changed. Save the complete rig to retain the new names. Close editing before clicking to recall.
