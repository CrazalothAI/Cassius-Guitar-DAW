# Sound foundation update

This update expands the sound controls while retaining the six-knob amp screen. It is a development update, not a claim of commercial readiness or a completed listening comparison with paid products.

## What changes

- **Cassian overdrive:** an original mid-forward drive before the existing neural pedal and amp. Drive controls soft clipping; Tone filters its top end; Level trims output; Low cut tightens the signal before clipping. The nonlinear stage runs at 4× sample rate. Bypass and controls ramp. No capture is required, and the existing NAM pedal has its own bypass.
- **Universal compression:** the existing mix, threshold, ratio, attack, release, and makeup controls can operate before the amp chain or after the cabinet. Post-cab detection links stereo channels so a louder channel does not shift the image. Compression at zero mix preserves dry samples, regardless of makeup. The default Lumen-only routing retains the existing compressor and older clean tones. Gain-reduction telemetry describes the new pre/post compressor, not the legacy Lumen compressor.
- **Dual cabinets:** two independent convolutions receive the same amp output, then mix in parallel. Blend uses linear weights to keep identical responses at the original level. Each has gain, stereo balance, polarity, and 0–10 ms manual alignment. The common low/high cuts also apply to the built-in speaker; Cabinet Off bypasses them. B defaults off. One available enabled response plays at full blend weight.
- **Complete recall:** both cabinet identities and paths participate in managed storage, full rig preparation, DAW state, relinking, and portable packs. Packs allow four unique assets and deduplicate a WAV used by both cabinets. Missing or changed second-cabinet references reject complete recall before changing the active rig.
- **Starting points:** Articulate lead uses built-in overdrive into Ferrum with light pre-amp compression; Studio clean uses Lumen with gentle post-cab compression. They require no external files. Earlier starting points retain bypassed additions.

Full-rig NAM captures in cabinet Auto bypass both cabinet responses. The compatibility Current rig source retains its old Clean/Lead cabinet behavior. Individual A/B controls affect external responses, not the built-in speaker. Existing IR energy normalization and silence trimming remain; alignment is relative to the trimmed responses. Pan balances existing left/right channels and becomes neutral in mono. There is no automatic phase alignment or modeled microphone movement.

## Compatibility and implementation

The previous 58 parameter IDs and indices remain in place. Eighteen controls append to them. Earlier schema-1 rigs and sessions receive default values: overdrive and cabinet B off, unity A level, centered balance, zero added delay, cuts bypassed, and legacy Lumen-only compression. Rig schema remains 1.

New DSP buffers, oversampling, and delay memory are allocated during preparation. Processing uses bounded chunks, parameter ramps, cached overdrive filter coefficients, and cabinet cutoff updates at a reduced control rate. The existing loader/convolution handoff continues to serialize JUCE processing and publication; complete rigs prepare both convolutions off the callback before the existing short activation fade.

Overdrive's oversampling filter reports **4.43267 base-rate samples** of algorithmic delay, approximately **0.092 ms at 48 kHz**. Bypass adds no overdrive delay. This figure excludes frequency-dependent tone filtering, the amp, model resampling, cabinets, driver buffers, and converter delay. It is not a measured interface round-trip latency or new host latency compensation. Automated alignment across different amp paths remains future work.

## Validation

Windows Release validation on 2026-10-03:

- 66 UI tests in eight files pass, including independent pedal bypass, compressor/cabinet A/B state, cabinet B native pickers, and library placement.
- All four native CTest suites pass. New renders at 44.1/48/96 kHz check exact dry bypass, overdrive harmonic generation, static compressor ratio, stereo linking, dual-IR level preservation, polarity cancellation, stereo balance, and manual delay timing. Integration covers every explicit amp source, older-rig defaults, four-asset portable packs, and duplicate A/B assets.
- Standalone and VST3 Release builds succeed. The browser layout fits the native editor's 860×620 minimum with vertical stage scrolling and no horizontal overflow.

Local timing uses synthetic picked phrases, two 4096-sample stereo WAV responses, overdrive, post-cab compression, chorus, delay, and reverb. The NAM profile additionally uses the NAM example WaveNet fixture in the amp and pedal slots; it is a computational fixture, not a curated pedal tone. Each case measures 240 warmed callbacks. Background applications were not isolated.

Representative results from the passing run:

| Profile | Rate / buffer | Mean | p99 | Maximum | Callback budget |
|---|---|---:|---:|---:|---:|
| Ferrum + dual IR + effects | 48 kHz / 64 | 0.070 ms | 0.131 ms | 0.288 ms | 1.333 ms |
| Ferrum + dual IR + effects | 48 kHz / 128 | 0.139 ms | 0.174 ms | 0.223 ms | 2.667 ms |
| NAM amp/pedal + dual IR + effects | 48 kHz / 128 | 0.107 ms | 0.143 ms | 0.184 ms | 2.667 ms |
| Ferrum + dual IR + effects | 96 kHz / 64 | 0.092 ms | 0.252 ms | **1.591 ms** | **0.667 ms** |

The 96 kHz / 64-sample case had an isolated wall-clock measurement exceeding its deadline. These short offline timings demonstrate headroom in the tested 48 kHz cases but do not guarantee dropout-free hardware playback, arbitrary capture complexity, or sustained performance. Use 48 kHz / 128 as a starting point and check the actual interface's dropout telemetry. Timing remains diagnostic rather than a flaky pass/fail test threshold.

## Remaining sound work

Live listening with the owner's guitar/interface, recorded DI comparisons, and long-duration device/DAW testing remain necessary. No real guitar recording was supplied for this pass; synthetic regression phrases do not establish clean feel, pick response, or a reduction in perceived fuzz. Algorithmic oversampling does not repair interface clipping, driver dropouts, or loopback feedback. General ordered pedalboards, automatic cabinet alignment, seamless MIDI scenes, and additional reverb algorithms remain on the expansion roadmap.
