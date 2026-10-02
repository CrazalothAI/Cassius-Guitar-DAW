# Cassian

The Project A guitar workstation and VST3 plugin: JUCE 8 / C++20 audio engine, Neural Amp Modeler captures, cabinet convolution, and an embedded React editor.

## Download

Every push to `main` publishes **Latest build** on the repository's Releases page: `Cassian-Windows.zip` holds `Cassian.exe` (standalone) and the `Cassian.vst3` bundle. Executables are not committed to Git; building locally puts them under `build/AmpSuite_artefacts/Release/` (see below).

## Run the interface preview

Requires Node.js 20.19+ (Node 22 LTS recommended).

```powershell
cd ui
npm ci
npm run dev
```

Open the local URL printed by Vite. Preview controls work locally; audio and native file pickers are available only in the compiled application/plugin. `npm test` runs UI checks; `npm run build` produces `ui/dist/index.html`, a single self-contained resource embedded in the native binary by CMake. No dev server is required by the compiled app.

## Build Windows standalone + VST3

Install Visual Studio 2022 Build Tools with **Desktop development with C++**, MSVC v143, Windows 10/11 SDK, and CMake 3.22+. Install Node.js and Git, and ensure CMake, Node/npm, and Git are on PATH. Windows also needs the Microsoft Edge WebView2 Evergreen Runtime (separate from the SDK).

From the repository root in PowerShell:

```powershell
./scripts/build-windows.ps1
```

The helper finds CMake inside Visual Studio Build Tools even when it is not on PATH. Close a running Cassian app before rebuilding. If the npm lockfile changes, also stop the Vite preview before rebuilding so Windows can replace esbuild. The equivalent manual steps are:

```powershell
./scripts/setup-webview2.ps1
./scripts/setup-asio.ps1
cmake --preset windows
cmake --build --preset release --parallel 2
ctest --preset release
```

The setup script downloads the pinned WebView2 SDK into `.deps`. CMake fetches pinned JUCE 8.0.6 and NAM Core 0.5.4 revisions, installs the locked npm dependencies, builds the UI, and embeds it. The NAM repository at this revision defines tools, so Cassian builds its own `nam_core` static target from the core sources.

If dependencies already exist in `.deps/JUCE` and `.deps/nam`, use:

```powershell
cmake --preset windows -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/.deps/JUCE" -DFETCHCONTENT_SOURCE_DIR_NAM="$PWD/.deps/nam"
```

Outputs:

- `build/AmpSuite_artefacts/Release/Standalone/Cassian.exe`
- `build/AmpSuite_artefacts/Release/VST3/Cassian.vst3`

Copy the entire VST3 bundle to your host's plugin location. The build does not modify installed plugins. The GitHub Actions workflow builds both formats and runs tests when pushed to a repository with Actions enabled.

## Play

1. Open `build/AmpSuite_artefacts/Release/Standalone/Cassian.exe`. On first launch, Cassian selects an installed AudioBox ASIO driver at 48 kHz / 128 samples, with input 1 and stereo output. Later launches recall your saved setup. Use **Options → Audio/MIDI Settings** to change it; a DAW uses the host's audio settings. Cassian uses AudioBox input 1 for guitar even if the host exposes a stereo input bus, so unused input 2 noise is not mixed into the amp.
2. Choose a **Starting point**: Tight metal, Singing lead, Glass clean, Warm clean, or Ambient clean. Metal and Singing lead use your loaded `.nam` capture; the three clean tones use the independent built-in Lumen path. Load a capture with **Load amp model**. Optional cabinet WAVs and detailed effects are in **Shape & Effects**. Leave the cabinet off for full-rig captures. No proprietary captures are bundled.
3. Cassian resamples NAM captures and pedal captures to the host sample rate when their embedded rate differs. The Rig page shows the capture's rate while conversion is active. Old models with no sample-rate metadata run at the host rate.
4. Bring up Master gradually (default −12 dB). Mouse/keyboard controls and DAW automation share APVTS state. Drag a dial up or down to turn it (hold Shift for fine control); clicking never moves it. Arrow keys step 1% of the range (Shift: the finest step), Page Up/Down 10%, Home/End the ends. Double-click any dial to reset it.

NAM Core 0.5.4 supports `.nam` file versions 0.5.x through 0.7.0, including newer A2 / SlimmableContainer captures. Model failures appear in a banner above the amp. A capture labeled **FULL RIG** may already contain cabinet coloration; start without a separate cabinet IR for such captures.

Signal path: input gain → gate → parallel amp paths → amp output → bass/middle/treble/presence → high cut → stereo delay → reverb → master → output limiter. Metal uses a two-stage low cut, pre-drive, NAM and optional cabinet convolution. Clean uses parallel compression, gentle saturation and speaker-style rolloff, bypassing NAM and the external cabinet. Channel changes crossfade over 30 ms; the capture remains loaded.

### AudioBox USB 96

Connect the guitar to instrument input 1, and headphones/speakers to the AudioBox. Turn its **Mixer** knob toward **Playback** to hear the processed output instead of direct dry monitoring. Start with headphone/speaker volume low and raise it gradually. Adjust input 1 gain so its clipping indicator stays off. Keep phantom power off for a directly connected guitar.

The Clean channel works without a `.nam` or cabinet file. Metal uses the loaded capture when available and a built-in high-gain amp when it is not, so the distortion channel works before any capture is loaded. Captures are level matched from their loudness metadata. Loading captures is a separate step from verifying the interface. If no AudioBox opens and the app falls back to another device, automatic monitoring stays muted; select the intended device in audio settings before enabling input.

To check the interface independently, close Cassian/other apps holding the driver and run `build/CassianAudioCheck_artefacts/Release/CassianAudioCheck.exe`. It opens ASIO for three seconds, reports device timing, callback count, peak input amplitude, and xruns, and sends silence to the outputs. It does not record audio.

Mono and stereo buses and mono/stereo outputs are supported; stereo host input buses use input 1 as the guitar source. Without a cabinet IR, a built-in 4×12 speaker voicing is used when there is no capture or the capture's metadata marks it as amp-only; full-rig and unlabelled captures pass through unchanged. For a loaded NAM capture, Drive pushes the capture input directly instead of adding a second unrelated waveshaper. Tight at 20 Hz and High cut at 20 kHz are off. Clean Compression blends a 2.5:1 compressor (15 ms attack, 140 ms release, 3 dB makeup) with the dry signal. Delay feedback is fixed at 35%; Width offsets right-channel repeats up to 25% and leaves dry guitar centered. Reverb has adjustable mix and room size. EQ is post-amp/cab shaping, not controls inside the neural capture. Starting points preserve input calibration, master volume, and loaded files. Channel buttons change the amp path and adapt the gate threshold/release for metal or clean; use a starting point to change EQ and effects together. Cabinet files must be mono/stereo and no longer than 10 seconds. PCM 24-bit WAVs are supported by JUCE's WAV reader.

## State and threading

DAW/standalone state includes every parameter and absolute model/IR paths. Assets are referenced, not embedded; keep them at those locations for recall. Missing/invalid assets report errors and preserve the current stage. Native file selection is asynchronous; parsing and model warm-up run on a loader thread. Captures are prepared and warmed and IRs are read off the audio thread; a DSP lock is held only for the final pointer swap (tens of microseconds). The audio callback only tries this lock; on the rare contention it outputs one silent block and counts the event instead of blocking or passing the raw DI through. Models are destroyed on the loader thread.

DSP buffers are allocated in preparation, captures are warmed with five silent blocks, and host callbacks are split into a bounded 256-sample internal quantum. Capture and pedal stages use fixed-size resampling buffers, so 44.1 kHz and 48 kHz rigs can share the same host session. Stage meters expose input, pre-pedal, post-amp, post-cab, and output peaks. An optional stereo backing-track bus is mixed after the amp/cab chain, keeping playback out of the gate and high-gain stages. EQ coefficient changes use fixed storage but are not interpolated. This is a first implementation requiring native audio/host validation before release.

## Validation status

The frontend production build and six UI tests pass, the browser preview has been visually checked, and the installed npm dependency audit reports zero vulnerabilities. Vite reports an `eval` warning from the unchanged official JUCE native interop shim.

On 2026-09-27, Windows Release builds of the standalone and VST3 succeeded with MSVC 19.44 / JUCE 8.0.6. Native processor tests passed (pass-through, finite bounded output, mono-to-stereo routing, oversized blocks, parameter recall, and invalid state).

The connected AudioBox USB 96 opened through its native ASIO driver at 48 kHz / 128 samples. A three-second diagnostic received 1,125 callbacks and reported zero xruns, 205 input-latency samples and 117 output-latency samples (about 6.7 ms combined driver-reported latency). This is a short connectivity check, not a long-run performance or measured analog round-trip benchmark. The standalone app and embedded WebView2 editor were opened and inspected. Its native AudioBox settings were verified at 48 kHz / 128 samples with input monitoring enabled. The user confirmed the Mesa capture produces guitar sound. The newer clean/lead tones still require listening feedback; VST3 testing inside a DAW remains a separate check.

After updating NAM, all four native tests passed: processor behavior plus SlimmableContainer, WaveNet, and LSTM loading/rendering. The user-supplied `90sDualRec-FullRig-Red-Modn-Classic.nam` (0.7.0, SlimmableContainer, 48 kHz) also loaded and rendered finite nonzero audio, and the reopened native editor visibly confirmed **MODEL LOADED** with the correct capture name. On this PC, rendering 48,000 synthetic test samples took about 123 ms. This is an offline model-only benchmark, not an end-to-end latency measurement. The user's model stays at its original location and is not distributed with the project.

## Source layout

- `Source/`: processor, WebView editor, NAM/IR wrappers, tone stack, parameter definitions
- `ui/`: React/Vite/Tailwind editor and JUCE JavaScript bridge
- `tests/`: native processor smoke tests
- `scripts/`: local WebView2 SDK setup
- `.github/workflows/`: Windows compile/test/artifact workflow

JUCE provides the standalone entry point; `Source/Standalone.cpp` supplies the application and first-run AudioBox configuration. `Source/Main.cpp` supplies `createPluginFilter()`.

## Dependencies and licensing

JUCE is available under AGPLv3 or a commercial JUCE licence. NAM Core is MIT licensed. The vendored JavaScript glue in `ui/src/juce/index.js` and `check_native_interop.js` is copied unchanged from JUCE 8.0.6 and retains its upstream notices. Review JUCE's licence terms and any model/IR redistribution terms before distributing binaries. See `THIRD_PARTY.md`.

### Sound and interface update — 2026-09-28

The amp-head editor has ten primary knobs, Metal/Clean channel buttons, a Cassian wolf badge on the chrome faceplate, instant A/B compare, one-click AudioBox Auto Trim, and a collapsible Shape & Effects panel. Starting points cover tight rhythm, singing lead, neoclassical lead, progressive clean, wide thall, glass clean, warm clean, and ambient clean. The independent clean channel was verified against a processor with a loaded WaveNet capture: identical clean output, with finite nonzero audio and state recall. Signal-based regressions verify bass attenuation with retained midrange attack, high-cut attenuation, compression dynamic-range reduction, and distinct stereo delay repeats. All four native CTest checks and eight UI tests pass. Standalone and VST3 Release builds succeed; the native Glass clean starting point was exercised and its parameter values verified, with Master unchanged at -12 dB.

Backing tracks continue to play in your existing player or DAW; this build does not include a backing-track player or tempo sync. Use Singing lead as a starting point and adjust delay time/mix to suit the track. Clean presets use a softer gate threshold for sustained notes.


### EVH rig and noise cleanup

The optional pedal NAM stage runs before the amp NAM and uses a smoothed bypass. Clean bypasses both captures and the external cab. Pedal file paths and bypass state are recalled with the rest of the rig. Load/change the pedal under Shape & Effects, then enable its switch on Metal. User-supplied packs are extracted under `.local/rigs/` and excluded from Git.

The gate now detects the dry input and controls both pre-amp and post-amp gain, with 6 dB hysteresis, a short hold, fast opening and adjustable release. Post-amp gating suppresses capture self-noise before delay/reverb, preserving their tails. Noise gate is labeled on the main panel and has an on/off switch and live open/closed indicator. Metal channel switching no longer inherits a clean preset's very low threshold. Rhythm and lead starting points avoid stacking extra built-in overdrive onto a high-gain capture.

The footer reports audio block size, DSP load and processing overruns. An input-clipping banner helps identify excessive interface gain. The user's AudioBox buffer was changed from 128 to 256 and then 512 samples at 48 kHz after observing repeated DSP overruns with two NAM captures. This trades additional latency for scheduling headroom; listening feedback is still required. The previous settings are backed up in `build/Cassian.settings.before-noise-fix`.

All four native checks and seven UI tests pass. The supplied EVH Red I + Fortin TS9 1 + Mesa Oversized SM57/VR2 chain also passed the complete processor test: finite bounded output, played notes, exact idle suppression with the gate closed, and clean-channel isolation from the loaded rig. Synthetic full-rig output peak was 0.158192 at the test settings. This does not prove the hardware crackling is resolved.

Standalone launch supports explicit `--amp "absolute path.nam" --cab "absolute path.wav" --pedal "absolute path.nam"` for loading a complete rig. Ordinary launches recall saved state. The amp flag must refer to an existing file to apply the import and conservative starting settings. Input gain and master remain unchanged.

UI status polling now reads atomic audio snapshots instead of holding the DSP lock. Previously, an unfortunate polling/callback overlap could silence an entire audio block. A concurrent polling regression renders 3,000 blocks and verifies no silent interruptions. If the swap lock is ever busy, that block is silent and counted in status.


### Streamlined playing view

The main amp now has six controls: Drive, Bass, Middle, Treble, Space (reverb mix), and Master. Five tone-family buttons offer Clean, Ambient, Rock, Lead, and Metal. More tones retains warm cleans and the previous starting points. Rig & Tone opens one of three pages: Shape (input, gate, filters, presence and amp output), Space (delay and room size), or Rig (amp/pedal/cab files).

With the supplied EVH Ivory pack active, Rock selects Blue I; Lead and Metal select Red I from the same folder. Other captures remain in place when the expected EVH sibling is unavailable. Clean and Ambient use Lumen independently of the loaded captures. Modern metalcore enables the loaded pedal; 80s rock and Singing lead bypass it. These are starting points, not emulations of a particular artist's commercial plugin. Choosing tones preserves input calibration and master level.

Seven UI tests and all four native checks pass. Native Rock selection was verified to load APP-5153-Ivory-Blue-I.nam and update its tone controls with Master unchanged.

### Interface polish

Dials turn by relative drag instead of jumping to the pointer: previously a single click near a dial's edge could move Master from −12 dB to +6 dB. A drag is one host automation gesture, and a click without movement sends none. Readouts follow the DSP: Tight at 20 Hz and High cut at 20 kHz read **Off**, high cut reads in kHz, micro-delay shows hundredths of a millisecond, and EQ/trim arcs fill outward from 0 dB.

The tuner opens over the amp grille, so the controls stay in place, and it reports in tune only for a detected note. Load failures can be dismissed, and the Rig page shows when a capture is resampled to the host rate. The tone-family row names the active preset, including those chosen from More tones, recognises it from the current settings after the editor reopens, and marks it **Edited** once a control moves. Drawer pages are keyboard-navigable tabs with labelled groups; a stage's knobs dim while its switch is off. A cabinet IR can be loaded while the clean channel is active. The footer shows DSP load whenever overruns are recent or load reaches 80%.

With the drawer closed, the main view fits the editor's minimum 860 × 620 size without scrolling. The two stylesheets were merged into one; a computed-style and screenshot comparison across 72 window-size/state combinations confirmed the merge alone changed nothing visible. `parameters.test.js` checks the UI parameter table against `Source/params/ParameterIDs.h`. The AUTO TRIM label is legible on the dark source strip. Twenty-six UI tests pass; the native build was not rerun for this interface-only change.

### Distortion channel and static fixes

Measured with a headless build of the engine on Linux, rendering a synthetic DI take (palm-muted chugs, a sustained chord, a lead line and silence over a −85 dBFS noise floor) through the full processor at 48 kHz / 128 samples.

- **Crackle/static:** the tuner's pitch analysis ran continuously and took up to 2.1 ms of each 2.67 ms callback several times a second, so the slowest 1% of blocks overran the real-time budget (90–140%, peaks of 2–5×). It now searches a decimated signal and refines at full rate (same sub-cent accuracy, about 12× cheaper) and runs only while the tuner is open or Thicken needs the pitch. The slowest 1% of blocks now use 2–15% of the budget without a capture and about 3% with a WaveNet capture.
- **Quiet captures:** captures play at the loudness they were trained at; among NAM's own examples that spans about 18 dB (−20 to −38 dB), and a quiet capture came out 24 dB below a hot one. Amp captures are now levelled to NAM's −18 dB convention (limited to −12…+24 dB), like the official NAM plugin; the Rig page shows the adjustment. Pedal captures keep their natural level into the amp.
- **No distortion without a capture:** the Metal fallback reached only 1.5% THD at Drive 0 (4.7% at full Drive). It is now a three-stage preamp with asymmetric tube-style clipping, interstage filtering, a mid push, post-clip depth and a soft power stage, at 8× oversampling: a 30 dB rise in input moves the output about 1 dB, and aliasing on the highest fretted note at full Drive is −65 dB. Its level sits near the clean channel and normalized captures. With no IR, a built-in 4×12 voicing rolls off the fizz above 5 kHz.
- **Signal order:** a pedal capture now sits before the built-in amp as it does before a capture (it used to follow the fallback clipper).
- **Resampling:** captures at another rate were converted with a per-block rounded length, drifting and reading past each block. A streaming resampler now carries its position across blocks (exact sample count, 15-sample latency, 0.3% error on a 44.1↔48 kHz round trip in irregular blocks) with anti-alias filtering when the rate drops.
- **Loading:** capture preparation and warm-up used to hold the DSP lock, so the callback passed the raw DI through for the duration. Reloading the A2 capture eight times during real-time rendering lost 111 callbacks before (about 14 per load, each passing the raw DI) and now loses none in repeated runs.
- **Idle stages:** the channel that is fully faded out is not computed (captures and cabinet rest on Clean), and filter coefficients are recalculated only when their cutoff moves.

The built-in amp and speaker were voiced by measurement, not by ear; listening feedback is still required. Native tests cover the resampler round trip, loudness matching and gear-type detection, the fallback's saturation, and the tuner staying idle while closed. Checks for Tight and the dynamic resonance filter now measure at the amp input, since the built-in amp re-saturates its input.

### Controls and downloads

- **Releases:** CI replaces a `latest` pre-release with the zipped standalone and VST3 on each push to `main`, and fails if the build produced no files. Actions moved off the deprecated Node 20 versions.
- **Remove:** the Rig page can unload the amp capture, pedal capture or cabinet IR. Removing the capture returns Metal to the built-in amp; removing the IR returns amp-only rigs to the built-in speaker.
- **Knob travel:** High cut, Tight, delay Time and gate Release put a musical value at mid-travel (8 kHz, 70 Hz, 300 ms, 150 ms) instead of the arithmetic middle, which gave High cut half its travel above 11.5 kHz. Arrow keys move 1% of travel. Saved sessions keep their values; DAW automation lanes recorded for these four controls will follow the new curve.
- **Switches:** the six on/off controls (Clean channel, Gate, Pedal, Resonance cut, Thicken, Piezo) are exposed to hosts as on/off parameters instead of 0–1 knobs. Saved sessions restore unchanged.
- **A/B** compares tone only: input calibration and master level stay put when switching sides, as with presets.
- **Size:** the UI bundle embedded in the plugin shrank from 930 KB to 329 KB by using a 192 px copy of the logo (drawn at 38–56 px); the original stays in `ui/src/assets`.

### Simpler panel and less fuzz under notes

- **One preset display** replaces the seven tone-family buttons and the "More tones" menu: ‹ › step through every preset in family order, clicking the name opens the full list grouped by family, and Revert appears once a preset is edited. A/B sits beside it.
- **Fewer controls on screen:** the main view keeps the six amp knobs, the source, IN/OUT meters and the gate light. The drawer has three pages (Amp, Effects, Rig) instead of five. Stage meters and Auto trim moved to Rig.
- **No effect switches:** the gate, Chug cut, Sub and Piezo switch off by turning their knob fully down (it reads Off) and back on by turning it up, like Tight and High cut. The pedal keeps its switch next to its file.
- **Fuzz while playing:** the DI noise floor rides on every note into the distortion and becomes fuzz the gate cannot remove. The metal channel now band-limits the DI above 7 kHz (pure hiss in a guitar signal) and closes a low-pass ahead of the amp as notes decay toward the gate threshold (7.5 kHz down to 1.8 kHz), when the string's own treble has already faded. It follows the gate switch. The built-in amp's first stage has less gain and bias: 30 dB more input now moves the output about 3 dB instead of 1, for more note definition, still at high gain. With a −70 dBFS noise floor, noise-induced fuzz fell from −40 to −47 dB relative to the note with the A2 capture and from −46 to −49 dB with the built-in amp; the played tone changed by at most about 1 dB per octave band.

Remaining fuzz mostly comes from the noise entering the interface: keep the guitar volume up and the interface gain just below clipping (Auto trim on Rig matches Input to it), and raise the gate threshold or lower Drive for very high-gain captures.

### Crackle, hum and the signal-chain layout

- **Crackle at small buffers:** the NAM engine was built with Eigen's alignment switched off, which also switched off most of its vectorised math, and without the rational tanh the official NAM plugin uses. With both fixed, a standard WaveNet capture at 48 kHz and 128 samples uses about 19% of each block on the test machine instead of 67%. Its slowest 1% of blocks went from 144% of the budget (heard as crackle) to under 40%. Output is unchanged to within 0.01%.
- **Dropout warning:** if blocks overrun, or the audio device reports dropouts (standalone, ASIO), a warning explains the crackle and, in the standalone app, offers the next larger buffer in one click. The footer shows the buffer size as a menu. In a DAW, raise the buffer in its audio settings.
- **Hum filter:** a bridge single coil near a transformer or a computer adds 50/60 Hz hum and buzz harmonics that a high-gain amp turns into fuzz under every note, and the Bass knob after the amp boosts it. The DI now passes through adaptive notches on every mains harmonic up to 2.4 kHz, about a hertz wide at the fundamental and widening with harmonic number to absorb grid drift. They engage only after hum is heard while the strings are quiet, so a quiet humbucker passes through bit for bit. Mains frequency (50 or 60 Hz) is found automatically, and the Input stage shows when hum is being removed. In the test rig, hum at −40 dBFS went from 19 dB below the note to the noise floor (about 47 dB below).
- **Pick attack:** the attack shaper stepped the gain by up to +4 dB in a single sample, also after the amp, and noise could trigger it. It is now a transient shaper ahead of the amp only, rising over about 1.5 ms. The Thall presets that use it measured 14 dB less noise-induced fuzz afterwards.
- **Metronome:** in the header. It has tempo (knob, −/+ or tap), beats per bar (accent on beat one) and click level, with a beat light. Standalone it keeps its own time; in a DAW that is playing it follows the song's tempo and bars. The click is mixed after the rig and is left alone by presets and A/B.
- **Amp head and flow:** the head is redrawn as a boutique head: tolex cab with metal corners, a cloth grille with tube glow behind it, a lit badge naming the channel, and a black-and-gold faceplate with skirted gold-cap knobs and tick rings. It has a Clean/Lead channel switch and a jewel lamp. Below it, the signal chain reads in order (Guitar → Input → Pedal → Amp → Cab → Effects → Out), each stage with its live level and status. The selected stage's controls show in the panel underneath, always open, so nothing hides in a drawer. The editor fits its window without scrolling from 860×620 up.
