# Cassian next steps

Planning checkpoint: 2026-10-08. This is an implementation handoff, not an instruction to publish or merge the current branch.

Current milestone: **1.1.3 Preview — selective take backups**, following the personal backup and tone recovery workflow. The owner authorized continued post-1.0 work on 2026-10-08. Verified personal archives, additive rig/take recovery, referenced sound/media relocation and standalone tone snapshots with retention are implemented. Complete and tone-library archives now also support selected whole takes with matching review sections. See [backup and recovery](BACKUP-AND-RECOVERY.md) and [selective archives](RELEASE-1.1.3.md). Signing remains deferred; manual acceptance results are not invented.

Next: try backup/restore and tone recovery on a second PC and larger real libraries, then add ZIP64. Automatic recording/session recovery is separate from tone reference snapshots. Fresh Windows install/real upgrade, actual DAW recall/automation, physical MIDI and export/camera checks still need recorded configurations in the acceptance matrix. Consider advanced routing/pitch after the recovery workflow is validated. The website remains deferred.

Previous candidate: **1.0.0 RC2 reliability** added independent actual-VST3 validation in CI, isolated test storage, reproducible runs and preserved evidence.

Previous candidate: 1.0.0 RC1 adds Help/setup, open-source licensing, matching source bundles, signing support and a stable publication gate. The owner reports most sounds good; remaining acceptance evidence is in [commercial release](COMMERCIAL-RELEASE.md).

Previous milestone: 0.9.0 adds independent footswitch bypass and expression targets for both distortion/plate/spring instances, with missing/deleted-slot protection. It includes 0.8.1 scene renaming and 0.8.2 saved-scene copying. See [0.9.0](RELEASE-0.9.0.md).

Previous patch: 0.8.2 adds saved-scene copies to another slot without changing live tone. See [0.8.2](RELEASE-0.8.2.md).

Previous patch: 0.8.1 adds rename-only scene metadata and selection without recall while editing. See [0.8.1](RELEASE-0.8.1.md).

Previous milestone: 0.8.0 adds four complete file-free spring/blues/fuzz rigs (22 built-in total) and a demanding six-effect processing fixture. It includes 0.7.1 spring and 0.7.2 pedal reset. Next work remains real guitar/interface/DAW audition and the signing, fresh-PC and sound-permission release gates. See [0.8.0](RELEASE-0.8.0.md).

Previous patch: 0.7.2 adds selected-pedal Reset controls with Undo/Redo, preserving bypass, file and stable automation identity. See [reset](RELEASE-0.7.2.md).

Previous patch: 0.7.1 adds an original dispersive spring-style reverb with compatible recall and independent controls. See [spring](RELEASE-0.7.1.md).

Previous milestone: 0.7.0 adds four complete file-free distortion/plate rigs, bringing the starter catalog to 18 built-in tones. See [0.7.0](RELEASE-0.7.0.md).

Previous patch: 0.6.2 adds an original plate-style stereo tank with independent controls, persistent recall and appended automation. See [plate reverb](RELEASE-0.6.2.md).

Previous patch: 0.6.1 adds original oversampled distortion with append-only automation and absent-family rig/scene migration. See [distortion](RELEASE-0.6.1.md).

Previous milestone: 0.6.0 connects saved review sections to trimmed guitar/video WAV export and reports actual output duration and peak attenuation. It includes 0.5.5 take notes and 0.5.6 persistent review sections. See [0.6.0](RELEASE-0.6.0.md).

Earlier patch: 0.5.6 adds persistent take review sections with worker-side saves and stale-version protection. See [take sections](RELEASE-0.5.6.md).

Previous patch: 0.5.5 adds searchable take notes and metadata rollback on failed saves. See [take notes](RELEASE-0.5.5.md).

Earlier patches: 0.5.2 adds take/version review waveforms; 0.5.3 adds sound-file and amp capture-type filters; 0.5.4 exports selected saved rigs without changing the playing tone. See [review waveforms](RELEASE-0.5.2.md), [library discovery](RELEASE-0.5.3.md) and [saved exports](RELEASE-0.5.4.md). Live audition, sustained interface/DAW evidence and cleared sound distribution remain outstanding.

Current implementation: steps 1 and 2 are implemented, and steps 3/4 have a first bounded serial runtime/editor, independent controls, two NAM slots, pre/post lanes, reserved deletion slots, replacement, drag/arrow reorder, Undo/Redo, schema-3/scene-3 migrations and deduplicated seven-asset packs. Recorded ambience and safe sound-ZIP intake are also implemented. Legacy rigs retain fixed routing until explicitly converted. See [pedalboards](PEDALBOARD-STATE.md), [October intake](SOUND-INTAKE-OCTOBER.md), and [Play Along/navigation](PLAY-ALONG-NAVIGATION.md). The website remains deferred in [TODO.md](TODO.md). The complete-rig increment now includes 22 built-in complete rigs, 20 exact-capture recipes, sound discovery filters, automatic packaged-bank intake and video soundtrack WAV export; see [presets/shared sounds](PRESETS-AND-SHARED-SOUNDS.md) and [video audio](VIDEO-AUDIO-EXPORT.md). Public third-party bank distribution awaits per-asset source/permission records.

## Historical baseline inspected on 2026-10-05

Milestone follow-up: 0.3.0 adds reamp tails and soundtrack trimming/fades; 0.4.0 adds manual/envelope wah, independent MIDI expression targets and two built-in clean/lead starters. 0.4.1 adds recovery of saved original/reamp tones from Takes; see [take recovery](TAKE-LIBRARY.md). 0.4.2 adds saved-rig renaming, searchable categories/notes and exact saved-tone duplication; see [organization](SAVED-RIG-ORGANIZATION.md). 0.5.0 adds saved-rig header recall, dependency inspection/relinking, named reamps, take sorting and quick guitar WAV export; see [the five-part update](RELEASE-0.5.0.md). The wah portion of step 9 is implemented. See [0.4.0 notes](RELEASE-0.4.0.md). Sustained interface/DAW validation, sound permissions and signing remain priorities; other advanced routing/pitch work stays on the roadmap.

- Current checkout: `codex/guitar-mix-focus`, committed HEAD `ab08c87` (Windows installer and named practice sections), plus existing uncommitted Play Along / GuitarMix work.
- Remote `main` was checked directly and remains `c5e6586`, eight commits behind this branch. Local feature work is not yet represented by the main-branch download.
- Delivered: searchable asset library, managed storage/relinking, universal amp selection, full-rig cabinet handling, complete rigs/A-B/packs, dual cabinets, overdrive/compression/modulation/chorus/delay/reverb, MIDI, four scenes, practice sections, recording, take review/reamping, and a Windows installer.
- Still absent: ordered multi-instance pedalboards and a curated, redistributable third-party factory sound library.
- Current verification: all 103 UI tests pass; the frontend production build succeeds; `AmpSuiteTests` rebuilt from current source and all four native CTest entries pass.
- The production browser preview was inspected at the native editor's 860 x 620 minimum. The large amp remains visible in Practice, leaving the waveform/recording work in a short scrolling panel.
- Existing app, plugin, setup, and portable downloads have not been rebuilt or republished during this inspection. Passing checks do not establish live audio quality, DAW compatibility, or installer readiness on a fresh PC.

## Implementation order

1. **Finish the current listening-mix milestone.** Verify Guitar balance and Mix focus at neutral and changed settings, across mono/stereo and 44.1/48/96 kHz. Add focused integration coverage proving they affect monitored guitar while leaving dry/processed recordings, offline reamps, backing, and metronome signals unchanged. Check preservation across control starting points, complete rigs, A/B, and scenes. Test the Play Along controls and native error states. `outputLimited` currently reports a held pre-limiter peak threshold, not measured gain reduction; make its warning accurately describe that evidence. Complete this work before committing and producing a new playable build.

2. **Simplify navigation and expose the active rig.** Use explicit Tone, Board, Practice, and Takes destinations with one active destination, replacing competing panel booleans. Keep Mix/Audio controls compact and consistently available; place MIDI setup under performance/settings. Retain the large amp treatment in Tone, and use a compact rig strip in Practice/Takes so waveform and recording controls have room. Show the active saved rig name, edited status, Save/Save As, and complete-rig A/B in the main rig control. Show the selected amp's friendly name instead of only `NAM CAPTURE`. Keep control starting points clearly distinguishable from saved complete rigs. Verify keyboard access and minimum-size layout without page overflow or contradictory active buttons.

3. **Define the serial-board state before building its editor.** Introduce a versioned rig representation with stable block identities, types, parameter state, asset IDs, bypass, output trim, and ordered pre/post lanes. Keep one amp and the existing cabinet stage for this milestone. Establish a bounded block/automation-slot design and measure supported active NAM counts; reordering must not silently reassign host automation to another effect. Preserve existing parameter IDs and migrate old fixed rigs, sessions, scenes, take snapshots, reference documents, and schema-1 packs. Packs currently contain at most the amp, one pedal, and cabinet A/B; board packs must support a validated, deduplicated set of referenced block assets. Reject incomplete or invalid recalls before changing the active rig.

4. **Implement ordered serial boards and the Board view.** Start with the existing overdrive, neural pedal, compressor, EQ, modulation, chorus, delay, and reverb as reusable block types. Support add, replace, duplicate, remove, bypass, and reorder, with drag and keyboard/menu alternatives, undo/redo, and a selected-block inspector. Offer musically useful pre/post placement while explicitly enforcing channel compatibility; a mono NAM stage must not unexpectedly collapse a stereo effects path. Prepare graph changes and captured assets off the audio callback, retain bounded buffers, and transition without raw-DI bursts or abrupt level jumps. Test order-dependent sound, independent instances, stereo preservation, state recall, automation, and CPU headroom using actual captures. This is the main remaining complex-pedalboard deliverable.

5. **Source contrasting sound families in parallel.** First close the jazz/blues/classical gaps: dry JC-120, Deluxe Reverb, Twin Reverb, tweed Bassman, AC30, American open-back/Jensen cabinets, Alnico Blue cabinet, and pickup-matched nylon body IRs. Then add Plexi/JCM800, SLO, Mesa Mark, Rockerverb, and contrasting rock/metal cabs. Audition existing EVH Green/Blue/Red, Rectifier full rigs, TS-9, SD-1, and cabinet files before sourcing duplicates. Expand pedals with Klon, Bluesbreaker/Timmy, BD-2, RAT, Muff, Fuzz Face, and HM-2 families. Track exact capture settings, calibration, speaker/mic data, creator/source, and redistribution rights. Commission or directly license assets for factory distribution; supplied archives currently have no documented bundling permission. The complete target queue remains in `EXPANSION-ROADMAP.md`.

6. **Turn those assets into useful complete rigs.** Curate Natural Nylon, Warm Jazz, Jazz Chorus, Tweed/Texas/British Blues, Country/Funk Clean, Classic Rock, Singing/Neoclassical Lead, Ambient, Doom, Thrash, Death Metal, Metalcore, and Djent rigs. Include dry and embellished options where useful. Verify input calibration and consistent listening loudness without flattening intentional pedal boosts. Display actual loaded assets and capture variants, recommend appropriate cabinets, and add structured style/gain/speaker filters. Do not label a built-in fallback as a captured brand-name amp. The factory collection must work after installation without asking beginners to find their first file.

7. **Run real-use validation before calling the sounds finished.** Use owner guitar DI fixtures and sustained live interface tests with nylon/piezo, single coils, humbuckers, and extended-range playing. Start at 48 kHz / 128 and 256 samples, then test supported alternate rates/buffers and heavy boards. Check clean dynamics, pick attack, decay/noise, preset changes, recording alignment, scene changes, and dropout telemetry. Exercise VST3 scanning, mono/stereo buses, automation, tempo sync, and session restore in real DAW hosts. Have a new user complete input setup, choose a sound, edit/save a board, reopen it, record a take, and recover a missing asset. Record tested configurations and remaining limits.

8. **Prepare a versioned paid release.** Rebuild standalone/VST3 and package the exact tested revision; use consistent app/plugin/installer version numbers and stable versioned release artifacts alongside development builds. Complete dependency distribution decisions and per-asset rights records. Add publisher signing, test missing-WebView2 installation on a fresh Windows environment, and test upgrades from actual older app versions while preserving settings, libraries, and takes. Resolve any live/DAW regressions before publishing. Preserve all Crazaloth provenance markers and third-party notices throughout.

9. **Expand advanced DSP after serial boards are stable.** Prioritize wah/envelope filtering, distinct plate/spring algorithms, pitch/harmonizer, and actual power-amp processing for preamp-only captures according to user demand. Parallel paths, dual amps, automatic cabinet alignment, independent old-scene tails, and gapless switching require separate routing/latency/performance work. Keep these outside the first serial-board milestone.

## Verification limits and reproducibility

The earlier GuitarMix/output-warning coverage gap is closed by step 1. The current native suite also verifies board validation/migration, independent serial controls and order-dependent audio, exact settled bypass, atomic rejected recall, scene/session recall and multiple captured blocks. Optional owner-library tests validate all October files and report a demanding actual-NAM board at 128/256/512 samples. Live hardware and real DAW automation still require their own verification.

MSBuild initially failed because its child environment contained conflicting `Path`/`PATH` keys. The native-test rebuild succeeded with a process-local normalized `Path` key and worker reuse disabled; no system environment setting was changed. The standard Vite development launch also hit sandbox directory-access errors; visual inspection used the successfully built production preview instead. These are local tool-environment observations, not established product defects.

Earlier sound-work recommendation (owner now reports most sounds good): audition complete rigs using the supplied clean/breakup, drive, British amp, contrasting cabinet and ambience choices. Extend the demanding real-NAM benchmark with sustained interface/DAW evidence, then improve sound-library discovery and curate level-consistent genre starting rigs with documented asset rights. Keep parallel amps, harmonizers and gapless scene tails for their later milestone.
