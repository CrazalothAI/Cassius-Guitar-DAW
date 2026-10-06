# October user sound intake

Inspected and validated on 2026-10-06. The eleven supplied ZIPs contain 65 unique files: 39 NAM captures and 26 WAV responses. Seventeen files duplicate content from the earlier intake, so the combined archive inventory has 103 unique files. All October files pass the actual NAM loader or supported WAV checks. The October and prior packs are now combined into the owner's managed library (103 unique sounds), with original names and pack metadata. A private 103-file bank has been prepared for relocated-library and installer tests. Public bank distribution remains pending creator/platform grants; no third-party sound data has been added to Git. See [complete presets and shared sounds](PRESETS-AND-SHARED-SOUNDS.md).

| Archive | Files | Library category | Use and limits |
| --- | ---: | --- | --- |
| Boss RV-6 (1) | 6 WAV | Ambience | Recorded modes, including captured delay; longest response about 9.95 s. |
| Strymon Flint V1 – Reverb Pack | 9 WAV | Ambience | Captured reverb variants; longest about 5.41 s. |
| Roland Space Echo RE-201 | 10 NAM | Pedal | Metadata says studio; pack context identifies instrument/mic circuit captures. Use before the amp for the captured coloration. These files do not expose repeat time/feedback. |
| Classic Drive Pedal Collection | 17 NAM | Pedal | KOT boost/OD/distortion, Guv'nor, RAT, three Muff variants, Tube Screamer, Blues Driver, Klon, Fuzz Face and Timmy. Settings are fixed capture variants. |
| Orange V30 4×12 – XR IR PACK | 4 WAV | Cabinet | Short cabinet responses, up to about 0.50 s. |
| Tone Charm ambient/shoegaze collection | 2 WAV | Ambience | Captured 10/20 s responses. Both fit the new 30-second ambience limit. |
| Boss SD-1 Super Overdrive (1) | 9 NAM | Pedal | Six drive and three boost captures; duplicate earlier content. |
| Legendary Metallic Thrash IRs | 5 WAV | Cabinet | Short contrasting thrash cabinet responses, up to about 0.19 s. |
| Friedman Brit 50 teaser | 1 NAM | Amp | Archive title mentions a larger pack, but this ZIP contains one capture. Metadata says amp; filename contains CAB. Confirm cabinet inclusion by audition and override Capture type if needed. |
| Marshall Jcm800 + V30 1960 | 1 NAM | Amp | Metadata says amp_cab. Start with Cabinet Auto to avoid stacking another cabinet. |
| Fender Super Reverb 1965 – Edge of Breakup – A2 | 1 NAM | Amp | Metadata says amp_cab. Useful breakup/clean candidate; start with Cabinet Auto. |

Across the files, sample rates are 44.1, 48 and 96 kHz. NAM playback uses Cassian's existing rate adaptation, and convolution prepares WAV responses at the host rate. File validation does not prove musical quality or live performance.

## Using these sounds

Open **Library** for the three new amp choices, drive/circuit captures, and nine cabinet IRs. **Import sound ZIPs** can import future supported collections directly, preserving names and deduplicating by kind/content hash. NAM gear metadata supplies amp/pedal classification; unknown captures require explicit manual import. WAV classification uses pack/file names and response length. The Ambience tab contains the 17 recorded ambience files.

For a pedal capture or reverb response, open **Board → Enable serial editing**, add/select **Captured pedal** or **Ambience**, choose its file in the inspector and switch it on. Each kind supports two independent slots. A recorded response keeps its original decay/repeat pattern; Blend and Output trim are the useful controls. Use the regular Delay/Reverb blocks for adjustable time, sync, feedback and algorithmic room controls.

Suggested audition queue: the Super Reverb capture for dynamics/clean breakup; low-gain Timmy/Klon/Blues Driver variants into Lumen or a clean capture; JCM800 full rig for British rock and lead; selected drive captures into the existing EVH amp-only models with contrasting Orange/thrash cabinets; Flint/RV-6 and the longer ambient responses after the cabinet. These are audition candidates, not level-matched factory rigs or claims to recreate every named device.

## Validation and rights

The optional `CASSIAN_SOUND_PACK_MANIFEST` native-test input points to a JSON array of absolute ZIP paths. It runs the real archive importer on all supplied files without adding third-party assets to fixtures. Synthetic tests additionally verify stereo ambience gain/timing, nested entries, safe path rejection, duplicate entry rejection and inert document skipping.

ZIP intake reads bounded streams into flat temporary files. It rejects unsafe/duplicate names and oversized entries before import, with limits of 1,024 entries, 64 MB per entry and 512 MB expanded total. It does not execute archive contents. Per-file errors are reported while valid files can still enter the library.

### Demanding-board processing check

The optional `CASSIAN_ACTUAL_SOUND_LIBRARY` test uses the managed JCM800 full rig, two independent Klon capture engines, the 20-second Tone Charm response, compressor, EQ, modulation, chorus, delay and reverb. The built-in overdrive block is present but bypassed. The Windows Release run on 2026-10-06 measured 750 offline guitar blocks after 50 warm-up blocks per buffer size:

| Buffer at 48 kHz | Mean processing | Maximum | Block time budget | Blocks over budget |
| --- | ---: | ---: | ---: | ---: |
| 128 samples | 0.614 ms | 3.083 ms | 2.667 ms | 9 / 750 |
| 256 samples | 1.190 ms | 3.788 ms | 5.333 ms | 0 / 750 |
| 512 samples | 2.368 ms | 7.675 ms | 10.667 ms | 0 / 750 |

Increasing the recorded-ambience convolution tail partition from 256 to 2,048 samples reduced the 128-sample mean from 1.973 to 0.614 ms (about 69%) in this case, while the immediate response retains zero added convolution latency. The stereo timing/gain test still passes. Fully bypassed NAM pedals also stop processing after their fade.

These are diagnostic wall-clock measurements of the offline guitar path, excluding device/DAW scheduling and the complete monitoring/recording path. They do not establish dropout-free live operation. Start demanding boards at 256/512 samples, watch the existing overrun/dropout alerts, and complete sustained interface and DAW checks before release.

No license/readme/redistribution terms were present in these eleven archives. Every imported asset remains User-owned in the library's categorization with unverified redistribution rights; this does not transfer copyright or establish permission. Record creators, source URLs and actual terms before any factory bundling or public pack sharing. Existing third-party notices and Crazaloth project provenance remain intact.
