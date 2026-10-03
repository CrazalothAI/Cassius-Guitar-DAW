# Cassian expansion roadmap

The first pass follows the requested scope: searchable library, universal amp selection, and complete rig saving. This does not represent completion of the full 55-item expansion or readiness for a paid commercial release.

## Foundation delivered

- Amps/Pedals/Cabinets/Presets library with search, Factory/User and favorite filters, editable friendly names, creator, source URL, genre/tone/gain tags, and capture/mic/pickup notes.
- Content-based IDs, duplicate detection, validated batch import, missing-file relinking, session persistence, and reference-only rig documents.
- Explicit Lumen, Ferrum, NAM, and Natural DI amp sources. Loaded clean NAMs can use pedals and cabinets; old sessions retain their original routing under Current rig.
- Amp-only/preamp-only/full-rig classification and cabinet Auto/External/Built-in/Off. Full rigs bypass an extra cabinet in Auto.
- Neutral Natural Nylon starting point separate from the electric pickup simulation.
- Named complete rigs and native A/B, preserving global input calibration, listening level, and metronome settings on recall.

The audio quality update adds shared managed storage, portable three-stage rig packs, prepared complete rig activation with a short guitar fade and retained effect tails, independent pedal trims, optional measured A/B matching, adjustable Lumen compression, stereo chorus, tempo-synced delay/feedback, and three reverb voicings with damping/pre-delay. Concurrent loading and stale library writers have regression coverage. See [audio quality update](AUDIO-QUALITY-UPDATE.md).

The current model remains one pre-amp neural pedal, one amp, one cabinet, and global effects. Ordered boards, effect-tail-preserving MIDI scenes, and gapless dual-engine switching remain future work. DAW session restoration keeps the older asynchronous stage-loading path. Search currently covers tags rather than separate category pickers; recommended cabinet pairings and richer variant fields remain to add.

Validation on Windows (2026-10-02): 53 UI tests and all four native CTest suites pass. New native checks cover duplicates/renamed paths, catalog-only batch import, NAM and pedal operation with the old clean flag enabled, neutral DI at 44.1/48/96 kHz, cabinet inclusion/override, smooth amp selection, full rig/session recall, preserved globals, invalid documents, missing references, and content-verified relinking. The foundation checks also pass with the supplied EVH Red I capture; the processor check runs additionally with the supplied SD-1 boost and Mesa cabinet. Standalone and VST3 release builds succeed. The browser layout fits the editor's 860×620 minimum size; long stage controls scroll inside their panel. Live AudioBox listening and Linux host validation were not performed in this pass.

## Existing user packs inspected

Read-only inspection of nine supplied ZIP archives found 61 NAM/WAV entries and 55 unique file contents. No assets were bundled or downloaded for distribution.

| Pack | Available contents | Next use |
|---|---|---|
| EVH 5150III Ivory | 28 NAM captures at 48 kHz: 8 Green, 10 Blue, 10 Red | Audition clean/crunch/rhythm/lead variants; retain original calibration metadata |
| Mesa 90s Dual Rectifier | 2 NAM full-rig captures at 48 kHz | Auto bypass additional guitar cabinets |
| Fortin modified TS-9 | 3 unique NAM captures at 48 kHz across three duplicate archives | Compare boost and overdrive variants |
| BOSS SD-1 | 9 NAM captures at 48 kHz: 6 drive and 3 boost | Compare clean-amp breakup and metal tightening |
| Mesa/ENGL/Marshall cabinet pack | 5 WAV IRs at 48 kHz | Audition with amp-only captures |
| BOSS RV-6 | 6 WAV responses at 96 kHz | These are fixed responses, not an adjustable NAM reverb pedal |
| Tone Charm shoegaze responses | 2 WAV responses at 48 kHz | Evaluate as fixed ambience, not ordinary cabinet IRs |

No license/readme/terms documents were found inside those archives. The detailed local inspection report is `.local/user-pack-inventory.json` and is deliberately excluded from Git. Do not interpret ownership of a download as factory redistribution permission.

## Next implementation phases

1. **Serial boards and storage:** ordered pre/post blocks, independent levels and bypass per block, stereo post-cab processing, replace/duplicate/reorder, keyboard alternatives, undo/redo, Play and Board views; library management for unused assets and future pack migrations. Managed/shared storage and schema-1 portable packs are delivered. Measure practical neural-block limits before promising a count.
2. **Effects and cabinets:** phaser/flanger/tremolo, separate plate/spring algorithms, dual-IR level/polarity/alignment, and actual power-amp processing for preamp-only captures. Adjustable clean compression, chorus, sync delay/feedback/divisions, and Room/Chamber/Hall voicings are delivered. WAV ambience responses do not supply adjustable pedal controls.
3. **Performance:** scenes, MIDI footswitch/expression, harmonizer/pitch, parallel paths and dual amps with latency compensation and controlled switching levels/tails.

## Core gear sourcing queue

Treat these as search/commission targets, not confirmed distributable captures. Favor dry amp-only captures including the power amp, with separate cabinets and useful clean, breakup, crunch, rhythm, and lead variants.

- **Clean/roots:** dry JC-120 (chorus implemented separately), Deluxe Reverb, Twin Reverb, '59 Bassman, AC30 Top Boost, optional pickup-matched nylon body IRs.
- **Rock/lead:** Plexi/1959, JCM800 2203, SLO-100.
- **Heavy:** existing EVH 5150III variants, amp-only Dual Rectifier, one Mesa Mark representative, Rockerverb.
- **Pedals:** existing TS-9 and SD-1, Klon family, Bluesbreaker or Timmy, BD-2, RAT, Green Russian or Ram's Head Muff, Fuzz Face, HM-2.
- **Cabinets:** American open-back clean/tweed voices, British chime and Marshall-style rock voices, Mesa/EVH-style modern metal cabinets, appropriate doom cabinets, and acoustic body IRs.
- **Starting presets after sourcing:** Natural Nylon, Warm Jazz, Jazz Chorus, Tweed/Texas/British Blues, Country/Funk Clean, Classic Rock, Singing/Neoclassical Lead, Ambient Clean, Doom, Thrash, Death Metal, Metalcore, Djent. Label each preset's actual loaded assets; do not relabel Lumen as a captured brand-name amp.

Later gear: AER/Grace acoustic preamps, Polytone, Tweed Deluxe, Dumble family, JTM45, Peavey 5150/6505, OR120/Sunn, ENGL, Diezel, Revv, Friedman, Op-Amp Muff, MT-2, and Fortin 33/Grind. Prioritize distinct sounds over overlapping file counts.

## Sourcing evidence and release gates

Checked 2026-10-02. Maintain a signed license record per bundled asset, creator/source, redistribution scope, calibration, original settings, capture type, sample rate, and cabinet/mic information. Commissioning original captures or direct written vendor agreements is the clearest factory-pack route.

- [TONE3000 terms](https://www.tone3000.com/terms): commercial redistribution requires written permission from both the creator and platform. [API integration](https://www.tone3000.com/api) requires a separate commercial agreement.
- [Celestion EULA](https://www.celestionplus.com/eula/): redistribution requires prior written consent.
- [Amalgam free captures](https://amalgamcaptures.com/free-captures/): personal-use downloads do not authorize bundling. [Contact the creator](https://amalgamcaptures.com/contact-us/) for licensing.
- [York Audio](https://www.yorkaudio.co/) and [OwnHammer helpdesk](https://ownhammer.com/pages/helpdesk): investigate direct agreements; current redistribution terms were not independently confirmed in this inspection.
- JC-120 candidates on TONE3000 include [a dry preamp capture](https://www.tone3000.com/tones/roland-jc120-jazz-chorus-90182), [DI/full-rig variants](https://www.tone3000.com/tones/rr-orland-j120-60377), and [a full-rig capture](https://www.tone3000.com/tones/roland-jc-120-jazz-chorus-6530). These are discovery leads requiring file/type, sound, and rights verification; the preamp-only option needs power-amp processing.

Before a paid release, resolve the applicable JUCE and ASIO distribution licenses, obtain written factory-asset redistribution permission, and test older-session migration, host automation, complete recall/relink, bypass, sample-rate conversion, loading, loudness, and demanding boards at useful buffers. Include listening tests with nylon/piezo, single coils, humbuckers, and extended-range guitars; synthetic checks alone do not validate sound quality or interface reliability.
