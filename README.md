# Cassian

Cassian is a guitar processor with a tube-head interface, built-in clean and high-gain amps, Neural Amp Modeler (NAM) support, cabinet convolution, and stereo effects. It runs as a Windows standalone app or VST3 plugin, with a JUCE/C++20 audio engine and an embedded React editor.

The current build adds a searchable asset library, a universal amp slot, and complete rig saving. It is a development build; the remaining work toward a commercial release is tracked in the [expansion roadmap](docs/EXPANSION-ROADMAP.md).

## Download and launch

Download **Cassian-Windows.zip** from [Latest build](https://github.com/CrazalothAI/Cassius/releases/tag/latest). The archive contains `Cassian.exe` and the `Cassian.vst3` bundle. Successful builds from pushes to `main` update this prerelease; check [GitHub Actions](https://github.com/CrazalothAI/Cassius/actions) for build status.

Run `Cassian.exe` for standalone use. Windows needs the Microsoft Edge WebView2 Evergreen Runtime to display the editor. For a DAW, copy the entire `Cassian.vst3` bundle into its VST3 plugin location and rescan. The build does not replace installed plugins automatically.

The native window, executable, and supported system tray use the Cassian wolf logo. The tray menu offers **Show Cassian** and **Quit Cassian**; closing the main window quits the app normally. Linux/X11 tray support exists in source, but this release does not provide a validated Linux distribution.

## Start playing

1. Connect your guitar to **input 1** and headphones or speakers to your audio interface. Cassian uses input 1 for guitar even when a stereo input bus is available.
2. In standalone, open **Options → Audio/MIDI Settings** to select your driver and outputs. A DAW controls its own device, sample rate, and buffer size.
3. Choose a starting preset from the header. Clean, ambient, piezo, rock, lead, metal, and extended-range voices are available. **Natural Nylon** provides a neutral DI starting point for a real nylon or piezo input.
4. Select **Amp** below the head to choose Lumen, Ferrum, a NAM capture, or Natural DI. The built-in amps work without downloading captures.
5. Raise Master gradually from its default −12 dB. Use the IN/OUT and stage meters to check levels, then save the result in **Library → Presets → Save current rig**.

### AudioBox USB 96

On first launch, the standalone app looks for an AudioBox ASIO driver and requests 48 kHz / 128 samples, input 1, and outputs 1–2. Subsequent launches recall the saved device setup. If it falls back to a different device, automatic monitoring stays muted until the intended setup is selected.

Turn the AudioBox **Mixer** knob toward **Playback** to hear the processed guitar. Set input 1 gain below clipping. If crackling coincides with the dropout warning, use the footer's buffer menu or audio settings to try 256 or 512 samples. Larger buffers give processing more time at the cost of additional latency.

## Amp and cabinet routing

The universal amp slot separates the sound source from the older Clean/Lead channel flag.

| Amp source | Behavior |
|---|---|
| **Current rig** | Compatibility mode for older sessions. Clean uses the built-in clean path and bypasses neural captures and the external cabinet. Lead uses the loaded amp capture, or Ferrum when none is loaded. |
| **Lumen** | Built-in clean amp with gentle saturation, compression, and clean voicing. Pedal and cabinet slots remain available. |
| **Ferrum** | Built-in oversampled high-gain amp. It stays selected even if a NAM file is retained in the rig. |
| **NAM capture** | Plays the selected clean or distorted NAM through the same universal slot. The channel switch cannot bypass it. A missing capture leaves this source silent and reports the problem. |
| **Natural DI** | Bypasses the electric amp path, tight/drive shaping, and adaptive hum removal. Optional pedal and external body IR processing remain available. |

For NAM, **Capture type** offers Auto, Amp-only, Preamp-only, and Full rig. Auto reads the capture's gear metadata; override it when the metadata is absent or inaccurate. Preamp-only classification does not add a power-amp simulator: that stage remains future work.

| Cabinet mode | Behavior with an explicit amp source |
|---|---|
| **Auto** | Full-rig captures bypass a separate cabinet. Other amp sources use a loaded IR; Ferrum and classified amp-only/preamp-only NAMs use the built-in speaker when no IR is loaded. Natural DI has no automatic guitar cabinet. |
| **External IR** | Uses the loaded WAV, including an intentional override for full-rig captures or a body IR for Natural DI. Load an IR for this mode. |
| **Built-in 4×12** | Uses Cassian's built-in speaker voicing. |
| **Off** | Bypasses separate cabinet processing. |

Unlabelled NAM captures do not automatically receive a built-in speaker. **Current rig** preserves its original cabinet routing; select an explicit amp source to edit cabinet mode.

A capture containing a cabinet usually works best with cabinet Auto or Off. Stacking another guitar cabinet can make it sound hollow or overly filtered. Bass, Middle, Treble, Presence, and the EQ pedal shape the processed sound; they do not change controls inside a captured amplifier.

## Library and complete rigs

Open **Library** to browse **Amps**, **Pedals**, **Cabinets**, and **Presets**. Search names, gear, creators, tags, and notes; filter by Factory, User, or Favorites. Edit friendly names, creator attribution, source URLs, tags, and notes in the details panel.

Factory entries currently consist of Cassian's own built-in amps and control starting points. No third-party NAM captures or WAV responses are bundled.

### Import and use assets

- Import multiple `.nam` files into Amps or Pedals, or `.wav` files into Cabinets. Validation runs on the loader thread; importing does not change the playing rig.
- Select **Use** to activate an asset. Using an amp selects the NAM source; using a cabinet selects External IR mode.
- Identical file contents within a category share a SHA-256 ID. Reimporting a duplicate preserves edited metadata and favorites, and records renamed file paths.
- Missing assets appear in the catalog. **Relink** accepts a file with the original content hash, including a renamed copy; a different capture must be imported as a different asset.

NAM support includes mono-input/mono-output WaveNet, LSTM, and A2/SlimmableContainer models in supported file versions 0.5.x through 0.7.0. Captures and pedal models are resampled when their embedded sample rate differs from the host. Models without a declared rate run at the host rate. Load errors appear in the editor.

Cabinet files must be mono or stereo WAVs no longer than ten seconds. Stereo IRs retain their left/right response. A fixed ambience WAV can be imported, but this does not turn it into an adjustable reverb pedal.

### Save, recall, and share

**Save current rig** stores the selected amp source, capture classification, cabinet mode, stage files, tone controls, and bypass states. Native **A/B** compares the same complete rig state. Rig recall preserves input calibration, Master, and metronome settings by default; reopening a standalone or DAW session restores its full saved state.

Use **Import rig** and **Export current rig** for `.cassian.json` documents. Imported rigs enter the library without changing the sound; select Use to recall them. Recall validates the document and its referenced assets before changing parameters. Missing or changed assets, invalid values, and incomplete documents are rejected with an error.

Assets are referenced by path and content ID, not copied or embedded. Keep the files available, or transfer them separately and relink. The library and saved rigs currently live in each standalone/DAW session rather than a shared global catalog. Factory favorites are editor-profile preferences; user metadata and favorites live in session state. Asset swaps are asynchronous and sequential, so rig recall is not seamless scene switching.

Header presets are **control starting points**, distinct from saved complete rigs. They preserve input calibration and Master, but choose their own amp source, EQ, and effects. Clean presets select Lumen; dirty presets retain compatibility routing. With a supported user-loaded EVH pack, some dirty presets can select a sibling capture variant. Save a complete rig when exact asset recall matters.

## Controls and effects

The head keeps six primary controls: **Drive, Bass, Middle, Treble, Space, and Master**. Space adjusts reverb mix. The stage row opens detailed controls in signal order without hiding the amp.

| Stage | Controls |
|---|---|
| **Input** | Input gain, Auto trim, gate threshold/release, pick-attack shaping, and hum-removal status. |
| **Pedal** | One pre-amp NAM pedal capture with smoothed bypass, file selection, and removal. |
| **Amp** | Source, NAM capture type, output level, Tight or clean Compression, Presence, and High cut. |
| **Cab** | Cabinet mode, WAV selection, and removal. |
| **EQ** | Four post-cabinet bands: Body (120 Hz shelf), Mud (350 Hz bell), Focus (1.2 kHz bell), and Fizz (4.8 kHz shelf), each ±12 dB. Flat EQ and Smooth distortion starting settings. |
| **Effects** | Stereo delay time/mix/width, reverb room size, micro-delay, dynamic resonance reduction, sub-octave blend, and electric piezo simulation. |

Delay feedback is fixed at 35%; Width offsets right-channel repeats while keeping dry guitar centered. Reverb uses one adjustable stereo room algorithm. Delay subdivisions, adjustable feedback, additional reverb types, and modulation pedals are not implemented yet.

Turn gate, attack, sub, and piezo controls fully down to switch them off. Tight at 20 Hz and High cut at 20 kHz are bypassed. The input gate detects the dry signal and also gates the amp output before delay/reverb, preserving effect tails. Adaptive hum removal learns mains noise during quiet passages; Ferrum also uses input noise shaping when the gate is enabled.

For input calibration, play your hardest notes and press **Auto trim** on the Input stage. It adjusts software input gain toward a −12 dBFS peak. Set the interface's physical gain below clipping first; software trim cannot undo clipping at the interface.

Drag a dial vertically or horizontally; hold Shift for fine adjustment. Arrow keys step through its range, Page Up/Down make larger steps, Home/End select the limits, and double-click resets it. A click without dragging leaves its value unchanged. Native gestures and parameter changes participate in DAW automation.

The header provides preset selection, previous/next, Revert for edited presets, A/B, tuner, and metronome. The tuner analyzes pitch while open; the sub-octave effect can also request pitch analysis. The editor supports a minimum 860 × 620 size, keyboard-accessible tabs, and a library dialog with focus handling.

## Backing tracks and metronome

Play backing tracks from an existing player or DAW; Cassian does not include a standalone track player. In a supporting DAW, enable the optional stereo backing bus to route accompaniment through the plugin.

The backing bus and metronome are mixed **after guitar distortion and effects**, then share Master and the output limiter. The metronome offers tempo, tap tempo, beats per bar, and click level. Standalone uses its own clock; while a DAW is playing, it follows host tempo and bar position. Presets and A/B preserve click settings. Delay time remains manually set rather than tempo-synchronized.

Simplified guitar path:

```text
Input gain → hum/piezo/input shaping → gate → NAM pedal → selected amp
→ cabinet → amp output/tone shaping → high cut/post-amp gate → EQ
→ stereo delay → room reverb → micro-delay
→ backing/metronome mix → Master → output limiter
```

Optional resonance reduction precedes the pedal; the parallel sub-octave layer joins after the cabinet. Stages are bypassed according to the selected source and controls.

## Troubleshooting

| Symptom | Check |
|---|---|
| No guitar | Input 1, intended driver/output pair, Master, load-error banner, and selected Amp source. Explicit NAM requires a loaded capture. |
| Dry/doubled guitar | Turn the interface Mixer toward Playback and check for another monitored DI path in the DAW. |
| Hollow or fuzzy capture | Check capture type and cabinet mode; avoid an unintended second cabinet. Reduce Drive or pedal gain, and try Smooth distortion EQ. |
| Noise while idle | Raise the gate threshold enough to close between notes, with a release that preserves sustain. |
| Crackling over notes | Check input clipping and the dropout warning. Reduce interface gain if clipped; increase buffer size when callbacks overrun. A gate does not repair clipped audio or dropouts. |
| Feedback with a backing track | Confirm the guitar input is not receiving a loopback mix and that accompaniment reaches the backing bus or a separate playback path. Lower speaker/listening level if sound is feeding back acoustically. |
| Missing rig files | Open the appropriate library tab and Relink the original content. Rig JSON does not contain the audio/model files. |
| Wrong sound after selecting a preset | Presets choose control/source settings. Recall a saved complete rig for exact files and routing. |

For an independent Windows interface check, close apps holding the ASIO driver and run `build/CassianAudioCheck_artefacts/Release/CassianAudioCheck.exe`. It opens the interface for three seconds, reports callbacks, input peak, timing, and xruns, and sends silence to the outputs without recording audio.

## Build from source

### Windows requirements

- Visual Studio 2022 Build Tools with Desktop development with C++, MSVC v143, and a Windows 10/11 SDK.
- CMake 3.22+, Git, and Node.js/npm. CI uses Node 22.
- WebView2 Evergreen Runtime for running the native editor; the SDK is downloaded separately by the setup script.

From the repository root in PowerShell:

```powershell
./scripts/build-windows.ps1
```

The helper locates CMake through PATH or Visual Studio, sets up the pinned WebView2 and ASIO SDKs, builds both Release formats, and runs native tests. Close Cassian before replacing its executable. The equivalent manual commands are:

```powershell
./scripts/setup-webview2.ps1
./scripts/setup-asio.ps1
cmake --preset windows
cmake --build --preset release --parallel 4
ctest --preset release --output-on-failure
```

CMake fetches pinned JUCE 8.0.6 and NAM Core 0.5.4 revisions, installs locked frontend dependencies, builds the UI, and embeds its single HTML resource. The native app does not need a development server. The build helper also supports existing `.deps/JUCE` and `.deps/nam` source checkouts.

Outputs:

```text
build/AmpSuite_artefacts/Release/Standalone/Cassian.exe
build/AmpSuite_artefacts/Release/VST3/Cassian.vst3
```

### Frontend preview and tests

```powershell
cd ui
npm ci
npm test
npm run dev
```

Open the local URL printed by Vite. Browser preview has working controls and parameter-only local rig saves, but no native audio engine or file import. To preview the production bundle:

```powershell
npm run build
npm run preview -- --configLoader runner
```

UI tests are separate from the Windows build helper. Native tests require a configured build and use `ctest --preset release --output-on-failure` from the repository root.

## Architecture and validation

| Location | Purpose |
|---|---|
| `Source/PluginProcessor.*` | Audio processing, host state, asynchronous asset loading, and complete rig recall. |
| `Source/AssetLibrary.h` | Content IDs, catalog metadata, duplicate merging, and missing-file handling. |
| `Source/PluginEditor.*` | Embedded WebView editor, native bridge, and asynchronous file pickers. |
| `Source/Standalone.cpp` | Standalone entry point, device setup, and tray integration. |
| `Source/dsp/` | NAM/IR processing, resampling, amp/speaker voicing, filters, gates, and effects. |
| `Source/params/ParameterIDs.h` | Native parameter layout; new routing parameters follow existing automation indices. |
| `ui/src/` | React editor, library, presets, and parameter/native bridge tests. |
| `tests/` | Native processor, catalog/rig, model, and interface diagnostic checks. |
| `scripts/` | Windows build and pinned SDK setup. |
| `.github/workflows/build.yml` | Windows build, UI/native tests, artifacts, and main-branch prerelease publishing. |

File reading, hashing, parsing, capture preparation, and warm-up run outside the audio callback. The callback processes bounded internal chunks with preallocated buffers and only tries the model-swap lock; contention produces a counted silent chunk rather than blocking. Sample-rate conversion retains streaming state between callbacks. Existing sessions default to compatibility routing; new source/type/cabinet parameters are appended to preserve earlier parameter indices.

Verified on Windows on **2026-10-03**: **53 UI tests**, **four native CTest suites**, and standalone/VST3 Release builds pass. Regressions cover parameter parity and recall, mono/stereo routing, large blocks, gates/EQ, clean and high-gain paths, resampling, metronome/backing isolation, library duplicates, neutral DI, cabinet overrides, rig validation, preserved globals, missing references, and content-verified relinking. Additional foundation checks passed with user-supplied amp, pedal, and cabinet files, which remain excluded from Git.

Automated renders do not establish live sound quality, long-run AudioBox reliability, or compatibility across DAW hosts. The latest foundation pass did not perform Linux host validation or live AudioBox listening. The production UI build reports an existing `eval` warning from the official JUCE native interop shim.

## Roadmap, attribution, and dependencies

See the [expansion roadmap](docs/EXPANSION-ROADMAP.md) for managed/shared libraries, ordered pedalboards, more effects, power-amp processing, dual cabinets, MIDI/scenes, parallel paths, and asset sourcing. The current chain has one neural pedal, one amp, one cabinet, and global effects.

Project ownership markers and their preservation instruction are recorded in [provenance](docs/PROVENANCE.md). Third-party attribution and licenses are listed in [THIRD_PARTY.md](THIRD_PARTY.md). JUCE uses AGPLv3 or a commercial license; NAM Core is MIT licensed. Review the applicable dependency and asset distribution terms before shipping a commercial build. User-imported captures are marked unverified for factory redistribution and are not included in this repository or rig exports.
