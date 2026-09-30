# Cassian

The Project A guitar workstation and VST3 plugin: JUCE 8 / C++20 audio engine, Neural Amp Modeler captures, cabinet convolution, and an embedded React editor.

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

The Clean channel works without a `.nam` or cabinet file. Metal uses the loaded capture when available and a built-in two-stage high-gain fallback when it is not, so the Drive control remains audible while you are assembling a rig. Loading captures is a separate step from verifying the interface. If no AudioBox opens and the app falls back to another device, automatic monitoring stays muted; select the intended device in audio settings before enabling input.

To check the interface independently, close Cassian/other apps holding the driver and run `build/CassianAudioCheck_artefacts/Release/CassianAudioCheck.exe`. It opens ASIO for three seconds, reports device timing, callback count, peak input amplitude, and xruns, and sends silence to the outputs. It does not record audio.

Mono and stereo buses and mono/stereo outputs are supported; stereo host input buses use input 1 as the guitar source. Missing amp/cab stages pass through. For a loaded NAM capture, Drive pushes the capture input directly instead of adding a second unrelated waveshaper. Tight at 20 Hz and High cut at 20 kHz are off. Clean Compression blends a 2.5:1 compressor (15 ms attack, 140 ms release, 3 dB makeup) with the dry signal. Delay feedback is fixed at 35%; Width offsets right-channel repeats up to 25% and leaves dry guitar centered. Reverb has adjustable mix and room size. EQ is post-amp/cab shaping, not controls inside the neural capture. Starting points preserve input calibration, master volume, and loaded files. Channel buttons change the amp path and adapt the gate threshold/release for metal or clean; use a starting point to change EQ and effects together. Cabinet files must be mono/stereo and no longer than 10 seconds. PCM 24-bit WAVs are supported by JUCE's WAV reader.

## State and threading

DAW/standalone state includes every parameter and absolute model/IR paths. Assets are referenced, not embedded; keep them at those locations for recall. Missing/invalid assets report errors and preserve the current stage. Native file selection is asynchronous; parsing and model warm-up run on a loader thread. A DSP lock serializes swaps, preparation, and convolution load calls. The audio callback only tries this lock; during a swap window it passes the current input through and counts the event instead of blocking or clearing a block. Models are destroyed on the loader thread.

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

The optional pedal NAM stage runs before the amp NAM and uses a smoothed bypass. When no Fortin/TS capture is loaded, the same switch uses a built-in Tube-Screamer-style tightening stage, so metal tones do not silently lose their front-end push. Clean bypasses both captures and the external cab. Pedal file paths and bypass state are recalled with the rest of the rig. Load/change the pedal under Shape & Effects, then enable its switch on Metal. User-supplied packs are extracted under `.local/rigs/` and excluded from Git.

The gate now detects the dry input and controls both pre-amp and post-amp gain, with 6 dB hysteresis, a short hold, fast opening and adjustable release. Post-amp gating suppresses capture self-noise before delay/reverb, preserving their tails. Noise gate is labeled on the main panel and has an on/off switch and live open/closed indicator. Metal channel switching no longer inherits a clean preset's very low threshold. Rhythm and lead starting points push the capture into its calibrated gain range, while the clean presets stay transparent. A DC/subsonic input blocker and softened post-gate curve prevent interface offset and gate modulation from becoming crackle in the high-gain path.

The footer reports audio block size, DSP load and processing overruns. An input-clipping banner helps identify excessive interface gain. The user's AudioBox buffer was changed from 128 to 256 and then 512 samples at 48 kHz after observing repeated DSP overruns with two NAM captures. This trades additional latency for scheduling headroom; listening feedback is still required. The previous settings are backed up in `build/Cassian.settings.before-noise-fix`.

All four native checks and seven UI tests pass. The supplied EVH Red I + Fortin TS9 1 + Mesa Oversized SM57/VR2 chain also passed the complete processor test: finite bounded output, played notes, exact idle suppression with the gate closed, and clean-channel isolation from the loaded rig. Synthetic full-rig output peak was 0.158192 at the test settings. This does not prove the hardware crackling is resolved.

Standalone launch supports explicit `--amp "absolute path.nam" --cab "absolute path.wav" --pedal "absolute path.nam"` for loading a complete rig. Ordinary launches recall saved state. The amp flag must refer to an existing file to apply the import and conservative starting settings. Input gain and master remain unchanged.

UI status polling now reads atomic audio snapshots instead of holding the DSP lock. Previously, an unfortunate polling/callback overlap could silence an entire audio block. A concurrent polling regression renders 3,000 blocks and verifies no silent interruptions. Asset replacement uses a short dry passthrough if the swap lock is busy and reports the count in status.


### Streamlined playing view

The main amp now has six controls: Drive, Bass, Middle, Treble, Space (reverb mix), and Master. Five tone-family buttons offer Clean, Ambient, Rock, Lead, and Metal. More tones retains warm cleans and the previous starting points. Rig & Tone opens one of three pages: Shape (input, gate, filters, presence and amp output), Space (delay and room size), or Rig (amp/pedal/cab files).

With the supplied EVH Ivory pack active, Rock selects Blue I; Lead and Metal select Red I from the same folder. Other captures remain in place when the expected EVH sibling is unavailable. Clean and Ambient use Lumen independently of the loaded captures. Modern metalcore enables the loaded pedal; 80s rock and Singing lead bypass it. These are starting points, not emulations of a particular artist's commercial plugin. Choosing tones preserves input calibration and master level.

Seven UI tests and all four native checks pass. Native Rock selection was verified to load APP-5153-Ivory-Blue-I.nam and update its tone controls with Master unchanged.

### Interface polish

Dials turn by relative drag instead of jumping to the pointer: previously a single click near a dial's edge could move Master from −12 dB to +6 dB. A drag is one host automation gesture, and a click without movement sends none. Readouts follow the DSP: Tight at 20 Hz and High cut at 20 kHz read **Off**, high cut reads in kHz, micro-delay shows hundredths of a millisecond, and EQ/trim arcs fill outward from 0 dB.

The tuner opens over the amp grille, so the controls stay in place, and it reports in tune only for a detected note. Load failures can be dismissed, and the Rig page shows when a capture is resampled to the host rate. The tone-family row names the active preset, including those chosen from More tones, recognises it from the current settings after the editor reopens, and marks it **Edited** once a control moves. Drawer pages are keyboard-navigable tabs with labelled groups; a stage's knobs dim while its switch is off. A cabinet IR can be loaded while the clean channel is active. The footer shows DSP load whenever overruns are recent or load reaches 80%.

With the drawer closed, the main view fits the editor's minimum 860 × 620 size without scrolling. The two stylesheets were merged into one; a computed-style and screenshot comparison across 72 window-size/state combinations confirmed the merge alone changed nothing visible. `parameters.test.js` checks the UI parameter table against `Source/params/ParameterIDs.h`. The AUTO TRIM label is legible on the dark source strip. Twenty-six UI tests pass; the native build was not rerun for this interface-only change.
