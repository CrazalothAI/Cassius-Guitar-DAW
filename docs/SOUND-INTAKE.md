# Existing sound intake

Read-only checkpoint: 2026-10-05. This is the local intake work for step 5 of [next steps](NEXT-STEPS.md), alongside the serial-board state milestone. It does not add factory sounds, download captures, change an audio device, or establish listening quality or redistribution permission.

## Evidence and locations

All nine previously supplied sound archives remain in `C:\Users\Bmint\Downloads`. Their SHA-256 hashes still match the 2026-10-02 report in `.local/user-pack-inventory.json`. Re-reading their NAM JSON and WAV headers confirms **61 NAM/WAV entries, 55 unique contents: 42 unique NAM captures and 13 unique WAV responses**. The six duplicate entries are the same three TS-9 captures repeated across three archives. ZIP container hashes differ; their captured file contents do not.

No license, licence, readme, or terms files were found in these archives. No new assets were copied into the checkout or packaged. `assets/models` and `assets/irs` contain only their placeholders. The detailed local inventory remains excluded from Git; this document records the useful conclusions without embedding captured weights or WAV data.

Existing extracted copies are available in:

- `C:\Users\Bmint\Downloads\EVH 5150iii Ivory FULL Pack`: 28 NAM files.
- `C:\Users\Bmint\Downloads\Fortin Modded TS-9 Tube Screamer`: three NAM files.
- `C:\Users\Bmint\Downloads\Mesa 90s Dual Rectifier RI - Red Modern FULL RIG`: two NAM files.
- `.local/verification-rig`: `APP-5153-Ivory-Red-I.nam`, `APP-SD1-Boost-I.nam`, and `Mesa Oversized SM57 VR2 5150 Power - jp_is_out_of_tune.wav`. These already existed as local regression fixtures; they are not factory assets.

No managed `Cassian/Library` manifest or capture collection was found under the inspected standard Windows Local/Roaming Cassian locations. `C:\Users\Bmint\AppData\Roaming\Cassian` currently contains `Cassian.settings`. A custom storage root or an embedded session library can still contain references; their absence from these standard locations is not proof that a user's session has no assets. NAM Core/JUCE example models and audio in `build/_deps` are dependency/test fixtures, excluded from this user-sound count.

## Packs and routing

| Supplied archive | Unique sounds | File evidence | Initial routing and next use |
|---|---:|---|---|
| `EVH 5150iii Ivory FULL Pack.zip` | 28 NAM | 8 Green, 10 Blue, 10 Red; 48 kHz; metadata `gear_type: amp` | Amp slot plus a separate cabinet. Start with Green as clean/breakup candidates, Blue for crunch/rhythm, Red for lead/heavy comparison; those are audition priorities, not confirmed sound classifications. |
| `Mesa 90s Dual Rectifier RI - Red Modern FULL RIG.zip` | 2 NAM | Classic and SweetSpot+808; 48 kHz; metadata `gear_type: amp_cab` | Full-rig classification; Cabinet Auto bypasses an additional cabinet. Compare the Classic capture with the capture whose name indicates an included 808 before adding another boost. |
| `Fortin Modded TS-9 Tube Screamer.zip`, `(1).zip`, `(2).zip` | 3 NAM total | `Fortin_TS9_1` through `3`; 48 kHz; metadata `gear_type: pedal`; identical contents across all three ZIPs | Mono pedal before the amp. No physical knob settings or calibration values are supplied; audition each independently before stacking with built-in overdrive. |
| `Boss SD-1 Super Overdrive.zip` | 9 NAM | 6 Drive, 3 Boost; 48 kHz; metadata `gear_type: pedal` | Mono pedal before the amp. Compare Boost for tightening and Drive for breakup; names do not establish output level or exact controls. |
| `Mesa Oversized SM57 and VR2 5150 Power.zip` | 5 WAV | Mono, PCM 24-bit, 48 kHz; Mesa/ENGL/Marshall/V30/mic names | Cabinet stage with amp-only captures. Four responses are about 15.25–15.60 ms; the named 5150 Power response is 82.5 ms and may include more than a speaker response. Verify its capture chain before pairing it with another modeled power amp. |
| `Boss RV-6.zip` | 6 WAV | Stereo, PCM 24-bit, 96 kHz; Spring, Delay, Hall, Plate, Room, Mod names; about 1.88–9.95 s | Fixed ambience responses. They are not NAM pedals or ordinary guitar cabinet responses; a convolution ambience stage would need separate wet/dry and tail handling. |
| `Delay And Reverb Ambient Shoegaze Machine by Tone Charm Audio (free).zip` | 2 WAV | Mono, PCM 24-bit, 48 kHz; slap back 20 s, percussive verb 10 s | Fixed ambience responses. Keep out of cabinet recommendations; the word “free” does not establish bundling permission. |

All inspected NAM entries use file format version `0.7.0` and architecture `SlimmableContainer`. Current runtime classification agrees with the amp/pedal/full-rig metadata above. The phrase “FULL Pack” in the EVH archive name does not mean those captures include a cabinet: their embedded type is `amp`. This remains a metadata-based classification pending creator/capture-chain confirmation.

The existing cabinet import path describes WAVs as cabinets and loads a trimmed, normalized convolution. It does not recognize a long WAV as an ambience pedal from its filename, and it does not supply a modeled RV-6's adjustable controls. A dedicated ambience asset type and convolution block should precede recommending these long responses to beginners. Their tails and CPU/memory cost require separate validation; this intake made no such measurement.

## Calibration, variants, and attribution

| NAM family | Embedded creator | Input reference | Output reference | Missing evidence |
|---|---|---|---|---|
| EVH Ivory | `ampspedalspickups` | `input_level_dbu: 13` on all 28 | Absent | Exact channel/gain/EQ/volume settings per Roman-number variant, capture hardware/reference method, source URL, confirmation that power amp is included |
| SD-1 | `ampspedalspickups` | `input_level_dbu: 13` on all nine | `output_level_dbu: 4` on all nine | Drive/Tone/Level knob positions, source URL and complete capture calibration procedure |
| Fortin TS-9 | `colossaldave` | Absent | Absent | Physical settings, input/output reference, source URL and modification details |
| Mesa full rigs | `Mad Hatter Amp Lab` | `input_level_dbu: 16.3` on both | `output_level_dbu: 21.1` on both | Amp/808 settings, cabinet/speaker/mic chain and positions, source URL and reference procedure |

Preserve the original embedded metadata and exact content identity when importing. The numeric metadata `gain` field is not a documented front-panel gain knob position. Green/Blue/Red and Roman numbers identify filenames; they do not provide a complete capture settings sheet. Mesa's SweetSpot+808 filename suggests a pedal in the captured chain, but the specific pedal, settings and signal order are unverified.

Every NAM includes a loudness value. Observed ranges are approximately −13.72 to −8.97 for EVH, −18.97 to −15.77 for SD-1, −31.59 to −24.78 for TS-9, and −22.16 to −21.20 for Mesa. These are capture metadata, not measured guitar listening loudness. Cassian's optional amp matching targets −18 dB using that field; it does not automatically calibrate an interface's ADC sensitivity from `input_level_dbu`. Pedal output trims must preserve useful intentional boost. Do not normalize every pedal to the same loudness and assume that reconstructs the original hardware chain.

For a reproducible audition, record the interface input calibration, guitar/pickup, raw DI level and capture input reference separately from output listening trim. Do not assume every interface has the same dBu-to-dBFS mapping. Captures at 48 kHz can be resampled by the current NAM wrapper, but other host rates still require audio and performance checks.

Cabinet filenames provide useful leads: Mesa Oversized, ENGL XXL, Marshall 1960BV, V30, SM57, SM58 and VR2/5150 Power. The suffix `jp_is_out_of_tune` is a filename attribution lead, not verified authorship or a license record. No embedded settings/creator manifest was supplied for these WAV packs. Speaker identity, mic combinations, positions, power-amp contribution and polarity/latency should remain marked unverified until confirmed.

## Coverage and sourcing priorities

The local collection is strongest in modern high gain and mid-forward boosts. EVH Green offers a useful first clean/breakup comparison, but no dry JC-120, Deluxe Reverb, Twin Reverb, tweed Bassman or AC30 capture was found in the inspected collection. No American open-back/Jensen, Alnico Blue, pickup-matched nylon body, or acoustic preamp response was found. Lumen and Natural DI remain the app's honest built-in starting options; they should retain their own identities.

Audition existing material before seeking duplicates:

1. EVH Green across all eight variants with a consistent cabinet, dry and with restrained compression. Compare dynamics, pick attack, chords, decay/noise and headroom using real guitar DIs.
2. EVH Blue/Red against both Mesa full rigs for classic rock, neoclassical lead and metalcore. Keep Mesa's included cabinet intact in Auto. Compare at matched listening level after establishing capture input calibration.
3. TS-9 and SD-1 variants into the selected clean and high-gain amp. Compare single-pedal boost/drive first; add stacking only when it has a useful musical result.
4. The five short cabinet responses with a fixed amp-only capture. Check phase/polarity, dynamics and sustain, then the dual-cab blend. Confirm the 5150 Power response's chain before treating it as an interchangeable speaker IR.

Then source contrasting clean/blues/classical families first: dry JC-120, Deluxe/Twin, Bassman, AC30; suitable open-back/Jensen and Alnico Blue cabinets; nylon IRs matched to the pickup/instrument. Plexi/JCM800, SLO, Mesa Mark and Orange voices follow. Long fixed ambience responses can wait for an appropriate convolution effect path. Distinct coverage matters more than another overlapping high-gain pack.

## Redistribution status

**No supplied third-party capture or response is cleared for Cassian factory redistribution by the inspected evidence.** The user supplied these downloads for local development/testing; no written grant, receipt with bundling terms, license document or exact source URL was found inside any of the nine archives. Personal-use permissions themselves were not verified in this intake. Embedded creator and trainer fields identify leads, not permission grants. NAM/Core software licensing does not establish rights to a particular model's weights or a cabinet WAV.

Keep these as user imports/local fixtures until a rights record exists for the exact contents. Record creator/contact, original source, content hash, signed permission or license document, permitted app/release/distribution scope, commercial and open-source redistribution scope, required attribution, modification/derivative permission, and any restrictions. Mark uncertain fields as unknown. Check the applicable written terms directly when sourcing; this read-only inventory did not review current website terms or contact creators.

Existing code notices and the owner's Crazaloth provenance markers remain intact. They do not replace third-party permissions or hide audit findings.

## Audition readiness and limits

`tests/ModelCheck.cpp` and the existing local `build/CassianModelCheck_artefacts/Release/CassianModelCheck.exe` can load a NAM, render a 0.05-peak 220 Hz sine at its native rate, reject silence/non-finite output, and report output peak/RMS plus elapsed rendering time. That is a useful load/synthetic baseline. It does not include a cabinet, exercise a whole rig, test several DI input levels, measure hardware round-trip latency, or establish sound quality. Its current output is not an audition WAV.

The existing take-library reamp path is better suited to a real dry guitar fixture and a complete rig snapshot, with separate outputs for blind comparison. No new ModelCheck run, offline take render, listening audition or audio device test was performed in this intake. Before calling a curated sound finished, retain the raw DI, selected capture/cab identities and settings, input calibration, render revision/rate, listening trim and owner feedback. The existing regression fixture checks in [expansion roadmap](EXPANSION-ROADMAP.md) establish earlier compatibility evidence, not a completed audition of all 55 unique sounds.
