# Cassian 0.6.2 preview

Board now includes an original stereo plate-style reverb. Four allpass input diffusers per side feed an eight-line orthogonal feedback tank with frequency damping. It is a distinct algorithm from the existing Room/Chamber/Hall voices and does not use an IR or change their sound.

Decay sets nominal feedback decay time (0.3-8 seconds); Tone damps the tail; Pre-delay (0-150 ms) separates ambience from pick attack; Width spreads the wet field; Blend combines wet and dry. Output trim sets the final block level. Add defaults to after-cabinet placement, with pre placement also available. Two instances have separate tanks/controls and support existing duplication, bypass, routing, Undo/Redo and automation.

Prepare allocates all delay storage. Processing uses fixed arrays, smoothed feedback/filter/mix controls, denormal protection and bounded finite feedback. Zero blend preserves exact dry stereo timing while the tank drains. Stereo width changes the wet field without collapsing the dry signal. This is an original plate-style effect, not a measured physical plate or branded pedal emulation. Spring modeling remains later work.

New host controls append after distortion. Old rigs/scenes default plate controls only when no plate block, including deleted reservations, exists. Plate-containing documents require complete validated controls. Existing rig schema 3 and scene schema 3 remain; older versions cannot play unknown new block types.

## Validation

New native checks cover impulse onset/pre-delay at 44.1/48/96 kHz, callback partitioning, decorrelated stereo, measured decay extension, wet-only width, tone changes, dry timing, appended automation, old rig/scene migration, independent instances, rejected incomplete state, session recall and portable packs. UI checks cover independent decay and after-cabinet addition. All 170 UI tests and four native suites passed on Windows on 2026-10-07 with the optional 103-file bank. Release tests/VST3 and the embedded frontend built successfully. Final standalone/installer builds are part of 0.7.0. Real guitar/DAW audition and host automation remain manual checks.
