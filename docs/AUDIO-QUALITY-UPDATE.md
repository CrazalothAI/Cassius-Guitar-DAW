# Audio quality update — 2026-10-03

This update addresses callback safety, complete rig activation, gain staging, clean effects, and reliable asset storage. It preserves the owner attribution and existing parameter indices.

## Audio and recall

- IR reading and allocation happen on the loader. A pending buffer reaches JUCE convolution through its wait-free loading API on the processing thread, serialized with `process()`. Emptied handoff objects are retired to the loader for destruction.
- Complete saved rigs prepare and prewarm both NAM slots plus an isolated cabinet convolution before activation. A 20 ms guitar fade surrounds the commit; global delay/reverb processors retain their histories. Failed preparation retains the previous rig. A changed audio device during preparation rejects recall with an error, allowing retry with the new settings.
- New requests supersede older pending rigs. The final commit is serialized with requests. Direct NAM preparation retries off the DSP lock if the audio settings change.
- Rare DSP-lock contention preserves accompaniment and metronome instead of clearing the whole mix. It still mutes guitar for that callback and counts the event. This is not a zero-dropout guarantee.
- Tone controls and NAM output gains are smoothed. Resonance filter detector constants are cached and notch coefficients update every 16 samples. Diagnostic hum/resonance values use atomic publication.

## Sound controls

The 17 appended parameters add independent NAM pedal input/output trims, Lumen compressor threshold/ratio/attack/release/makeup, stereo chorus, delay feedback/sync/division, reverb voicing/damping/pre-delay, and optional NAM metadata output matching. Defaults retain prior settings and bypass new chorus/sync behavior. Existing 41-parameter rig JSON receives the missing defaults during validation.

Chorus provides quadrature stereo modulation with a centered dry path. Delay supports 0–85% feedback and six musical divisions using a playing host BPM or standalone tempo. Its preallocated storage covers slow whole-note repeats and stereo offsets. Room, Chamber, and Hall are voicings of JUCE's existing feedback reverb network. These are not separately modeled plate or spring pedals. Wet pre-delay leaves the dry attack in place.

Optional native A/B matching compares recent guitar/input RMS ratios and adjusts recalled amp output by at most ±12 dB. It does not move Master. Comparable playing and a usable signal are required; this is not calibrated perceptual loudness normalization.

## Library and packs

Native instances share an application-data library manifest and content-addressed managed NAM/WAV copies. Original names and user metadata remain in the catalog. Local/interprocess locks and atomic manifest replacement protect concurrent writes; stale writers merge their changed entries, and deleted-rig tombstones prevent resurrection.

Rig JSON stays reference-only. Portable ZIP packs include one rig document and its selected three stage files. Import checks the parameter schema, expected entry names, sizes, content hashes, and supported model/IR format before adding a rig. It never performs general archive path extraction. Invalid packs do not activate a rig; a failed import can leave already validated, unused managed files in storage.

## Verification and limits

Windows verification includes 59 UI tests and all four native CTest suites. New native renders exercise 44.1/48/96 kHz IR publication, chorus and delay, continuous callbacks during complete rig recall, malformed capture rollback, latest-request selection, measured reverb onset/decay, gain/compression response, old rig defaults, shared/stale writers, moved originals, three-stage pack transfer, checksums, and unexpected ZIP paths.

Automated tests cannot establish the perceived tone or prove absence of live AudioBox crackle. Listen with the actual interface and guitar, monitor input clipping/callback load, and test sustained playback before a commercial release. Linux hosts and multiple DAW vendors remain unverified in this update. DAW session state restores through the existing asynchronous stage path; prepared activation applies to saved complete rigs and native A/B. Ordered pedalboards, dual cabinets/amps, compensated parallel paths, MIDI scenes, and true gapless switching remain future work.
