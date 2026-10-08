# Cassian TODO

## Current milestone

- [x] Complete 1.0.0 RC2: independent VST3 validation, isolated host-test storage, reproducible reports and CI evidence.
- Signing is deferred at the owner's request on 2026-10-08; continue unsigned candidate builds. The stable-release signature check remains pending rather than being marked passed.
- [ ] Run and record the fresh-install/real-upgrade and actual DAW/MIDI/export acceptance matrix.
- [x] Add the 1.1.0 preview personal backup/recovery workflow with sound/take relocation, verified streaming archives, additive restores and progress/cancellation.
- [x] Add 1.1.1 standalone automatic tone reference snapshots, explicit additive preset recovery, local preferences and 64-entry retention.
- [x] Add 1.1.2 complete/tone-library archive choice, keeping complete backups as the default and excluding recordings only on explicit selection.
- [x] Add 1.1.3 selective take backups with stable identities, matching review sections, preserved snapshots/reamps and unchanged complete/tone-only defaults.
- [x] Add 1.2.0 stored ZIP64 personal archives up to 32 GiB with bounded, cancellable checksum reads and strict format/boundary validation.
- [x] Add 1.2.1 validated optional practice/review section recovery and a local per-file outcome report without blocking rig/audio recovery.
- [x] Add 1.3.0 incomplete recording metadata, disk-worker WAV checkpoints, verified separate recovery copies, review confirmation and one-installer publication with top-level artifact cleanup.
- [x] Add 1.3.1 guided input sampling, explicit bounded trim, clipping/dropout/stale-route protection and live callback diagnostics in Help/setup.
- [x] Add 1.3.2 original starter-tone guide with contrasting clean/crunch/rhythm/lead/ambient/nylon exercises and explicit complete recall into Tone.
- [x] Add 1.4.0 bounded standard-WAV long-take review with normal-speed transport, fixed cache, waveform/sections, buffering and changed-media protection.
- [ ] Add streamed-loop prefetch after sustained real-take acceptance; speed changes/RF64 remain separate work.
- [ ] Validate the first-five-minutes workflow with a new user and sustained long-take review; see [product roadmap](PRODUCT-ROADMAP.md).
- [ ] Validate backup/restore and snapshot recovery on a second PC and large real libraries. Automatic recording/session recovery remains separate work.

- [x] Prepare 1.0.0 RC1: in-app Help/setup, AGPL licensing, matching source, publisher attribution, signing support and stable-release gate.
- [ ] Complete the acceptance evidence and signatures in [commercial release](COMMERCIAL-RELEASE.md) before stable sale publication.
- [x] 0.9.0: independent distortion/plate/spring footswitch bypass and CC drive/blend targets for both slots, with reorder stability, missing/deleted-slot errors and session persistence.
- [x] 0.8.2: copy saved scenes into another slot without recalling or capturing live edits; explicit replacement labels, independent snapshots and session/pack coverage.
- [x] Finish Play Along: listening-only tests, accurate output-peak warning, playable build.
- [x] Simplify navigation: Tone, Board, Practice and Takes; compact practice/take amp strip; saved rig identity and direct saving.
- [x] Add the compatibility foundation for pedalboards: bounded identities, fixed automation bindings, schema-2 exports, legacy migrations and rejected-recall protection.
- [x] Inventory existing sound files and document calibration, routing, coverage gaps and redistribution evidence.
- [x] Complete independent block parameters/assets, ordered pre/post lanes, reserved automation slots and broader pack traversal.
- [x] Build the first serial audio runtime/editor with duplication/replacement, drag/arrow reordering, bypass and undo/redo.
- [x] Add recorded ambience responses and safe sound-ZIP intake; validate/import the October user packs.
- [x] Measure a demanding actual-capture board at 128/256/512 samples and optimize long-response convolution; see [October intake](SOUND-INTAKE-OCTOBER.md).
- [x] Owner reports most sounds tested and sound checks good on 2026-10-07; coverage/configurations were not supplied.
- [ ] Document sustained interface configurations and real DAW/MIDI/export-sync evidence for advertised release scope.
- [x] Add 12 complete built-in rigs and 20 exact-capture recipes; remove blanket Red-I/Blue-I preset routing and add structured sound-library filters.
- [x] Prepare and verify a relocated 103-file shared bank and installer/portable bank support.
- [ ] Obtain per-asset redistribution grants and ship the approved bank in the GitHub release; private supplied sounds are not publicly cleared.
- [x] Record synchronized backing stems and export selected takes/reamps as 48 kHz / 24-bit stereo audio for video editing.
- [ ] Verify a real exported take with camera footage in Clipchamp.
- [x] Unify app/plugin/editor/installer versions; add versioned download copies, checksums, build metadata and installer registration checks.
- [x] Add 0.3.0 reamp effect tails and non-destructive soundtrack ranges/fades, including synchronized backing with extended versions.
- [x] Add 0.4.0 manual/envelope wah, two independent MIDI expression targets, compatible rig/scene migration, and built-in clean/lead starters.
- [x] Add 0.4.1 recovery of original/reamp rigs from Takes while preserving calibration/listening controls and recorded files.
- [x] Add 0.4.2 saved-rig renaming, searchable genre/gain/tags/notes, exact saved-tone duplication and persistence-failure rollback.
- [x] Complete the 0.5.0 five-part workflow update: saved-rig header recall, dependency inspection/relinking, named reamp versions, take sorting and quick guitar-only WAV export.
- [x] Add 0.5.1 take audition pause/resume and A/B loops with loaded-version identity protection and unchanged recordings/export settings.
- [x] Add 0.5.2 take/version review waveforms with pointer/keyboard seeking and stale-data protection.
- [x] Add 0.5.3 sound-file availability and amp capture-type filters with relink refresh and routing explanations.
- [x] Add 0.5.4 direct saved-rig reference/portable exports, picker-time snapshot preservation and relinked/legacy asset resolution.
- [ ] Sign and validate a versioned preview on a fresh PC; complete sustained interface and real DAW checks before a paid release.
- Validation and remaining limits: [Play Along and navigation](PLAY-ALONG-NAVIGATION.md).
- Pedalboard contract and next stage: [Pedalboard state](PEDALBOARD-STATE.md). Capture evidence: [Sound intake](SOUND-INTAKE.md).
- Follow the implementation order in [NEXT-STEPS.md](NEXT-STEPS.md) for pedalboards, sounds, curated rigs and real-use validation.

- [x] Add 0.5.5 searchable take notes with persistent metadata and failed-save rollback.
- [x] Add 0.5.6 persistent take review sections with worker-side storage, captured ranges and stale-request rejection.
- [x] Add 0.6.0 review-to-export selections, guitar excerpt WAVs and measured export completion reports.
- [x] Add 0.6.1 original 4x oversampled distortion, independent duplicate controls and compatible old rig/scene recall.
- [x] Add 0.6.2 original stereo plate-style reverb with independent instances, old-state migration and complete rig/scene/pack recall.
- [x] Add 0.7.0 four file-free distortion/plate starter rigs with complete recall and reference-level checks.
- [x] Add 0.7.1 original spring-style reverb with separate dispersion controls, appended automation and compatible recall.
- [x] Add 0.7.2 selected-pedal Reset controls with isolation, preserved bypass/files and Undo/Redo.

- [x] Add 0.8.0 four spring/blues/fuzz starters and finite-output/performance checks for a six-effect board.

- [x] Add 0.8.1 scene renaming with saved-tone isolation and selection without recall during editing.

## After the app is more mature

- Plan and build a polished Cassian website for promotion, demos, easy Windows downloads, documentation, support and suggestions.
- Include a public roadmap, release notes, contributor information and transparent project/dependency attribution; keep development open and community participation straightforward.
- Start with repository-backed suggestions/support rather than duplicating a community database. GitHub offers [Discussions categories and moderation](https://docs.github.com/en/discussions/managing-discussions-for-your-community) and [structured discussion forms](https://docs.github.com/discussions/managing-discussions-for-your-community/creating-discussion-category-forms).
- Keep user downloads separate from source archives. Connect the site to tested versioned release assets; GitHub documents [direct release download links](https://docs.github.com/en/repositories/releasing-projects-on-github/linking-to-releases).
- Define visual design, content ownership, moderation, accessibility, privacy, hosting, administration and maintenance requirements when website work begins.
- Website work is deferred at the owner's request. No site, domain purchase, deployment, account or paid service is authorized by this TODO.
