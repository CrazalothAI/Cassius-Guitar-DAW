# MIDI foot control

Open **Foot control** above the amp to configure eight assignments. Mapping starts disabled. Standalone and VST3 accept CC and program-change messages; notes, clocks and SysEx do not control the rig. Cassian produces no MIDI output.

## Connect and assign

1. In standalone, connect the controller and enable its checkbox under **MIDI inputs**. Port selection and mapping enable are separate. Ports use JUCE's saved device settings. If no input appears, check the connection and Audio settings.
2. In VST3, route MIDI to Cassian in your DAW. The host owns input devices; support for MIDI into an audio-effect plugin varies.
3. Select an assignment, choose an action, and press **Apply assignment**. Recall requires a complete rig saved in the Library, rather than a header starting preset.
4. Press **Learn controller**, then press a switch or move the expression pedal. Learn stores its channel and CC/PC number without executing an action. Release the switch before testing. Learn also works while mapping is disabled. Cancel learn, closing the panel or closing the editor ends learning.
5. Enable mapping and try the control. **Last input** shows incoming CC/PC messages even while disabled. **Last action** identifies the most recent accepted request; rig preparation can still fail afterward.

Manual channels are 1–16 or **All channels**. Numbers are the MIDI wire values 0–127, including PC; a controller displaying programs 1–128 may require a one-number adjustment. Actions cannot overlap on the same message type/number/channel, including overlap with All channels. Unassigned rows do not reserve a message.

Apply edits before Learn. Selecting another row discards unapplied edits. Conflicting or incompatible learned messages display an error and leave Learn waiting.

## Actions

| Action | Behavior |
|---|---|
| Recall saved rig | Existing off-thread complete rig preparation and short guitar fade; preserves global input calibration, Master and metronome settings. |
| Recall scene | One of four guitar-parameter snapshots sharing the current files. Preserves Input, Master and click settings; an empty slot reports an error. See [performance scenes](PERFORMANCE-SCENES.md). |
| Toggle overdrive / NAM pedal / EQ / gate / metronome / modulation | Changes that enable parameter. An empty NAM pedal slot remains empty. |
| Master expression | Absolute listening level, −60 to 0 dB, affecting guitar, backing and click together. |
| Drive expression | Main amp Drive, 0 to 24 dB; separate from the overdrive pedal's Drive dial. |
| Reverb / Delay expression | Absolute wet mix, 0 to 100%. |

CC toggles fire on a rising crossing into 64–127. Release below 64 before the next press; repeated held values do not retrigger. Use a momentary controller sending press/release, or PC. A latching controller alternating 0/127 toggles only on its high-value presses. Each matching PC triggers its action.

Expression requires CC and supports reversed direction. It follows absolute pedal position with existing DSP smoothing; there is no soft takeover. Moving the pedal can change a value previously set by the UI or a rig.

## Persistence and timing

Assignments and enable state belong to the native app/DAW session. They survive session restore and rig recall, but are excluded from complete rigs, JSON exports and portable packs. They reference shared-library rig IDs. Removing an assigned rig leaves a missing binding; recall reports an error and preserves the current sound. Legacy sessions start disabled. Invalid saved MIDI configuration resets assignments and disables mapping with an error.

Scene assignments reference slot numbers in the currently loaded rig. The scene bank itself is part of the complete rig/session/pack; changing rigs replaces its bank while retaining MIDI assignments. Older MIDI configurations without a scene field remain supported.

The audio callback scans at most 512 messages per block and passes commands through a preallocated queue with 127 usable entries. Mapping locks, rig preparation, host notifications and file work run outside the callback. A dedicated worker polls every 10 ms, including with no editor open. Commands are asynchronous and ignore within-block sample offsets. Rig preparation adds time and a short activation fade; this is not gapless scene switching. Non-rig controls during rig preparation are rejected with a retry message.

Changing assignments, restoring settings, cancelling Learn or disabling mapping invalidates older queued commands. An already executing action cannot be cancelled this way. Excess traffic is dropped and reported; reduce redundant expression messages if the warning appears. Bindings use channel/number rather than device identity, so two enabled controllers sending the same message address the same assignment.

## Validation

Windows checks on 2026-10-03: 83 UI tests, four native CTest suites, standalone and VST3 Release builds pass. Coverage includes disabled defaults, channel matching, switch edges, range/overlap rejection, Learn/cancel, overflow/stale-command rejection, callback failure recovery, session round trips and malformed/legacy state, expression limits/inversion, ignored messages, complete rig recall with preserved globals and missing-target rollback. The browser layout fits 860×620 with internal panel scrolling.

Physical MIDI controllers, DAW MIDI routing and Linux MIDI backends have not been validated in this update. Tests inject MIDI into the processor and do not establish end-to-end hardware latency or compatibility.
