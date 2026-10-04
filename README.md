# Cassian

Cassian is a guitar processor with a tube-head interface, built-in clean and high-gain amps, Neural Amp Modeler (NAM) support, cabinet convolution, and stereo effects. It runs as a Windows standalone app or VST3 plugin, with a JUCE/C++20 audio engine and an embedded React editor.

The current source adds dual-IR cabinets, an original built-in overdrive, compression for every amp source, and clean/lead starting points. Standalone practice includes backing-track playback, paired guitar recording, a searchable take library, and offline reamping. It also includes safer audio loading, prepared complete rig recall, a shared managed library, portable rig packs, independent pedal trims, and expanded clean/stereo effects. It is a development build; the remaining work toward a commercial release is tracked in the [expansion roadmap](docs/EXPANSION-ROADMAP.md).

## Download and launch

Download **Cassian-Windows.zip** from [Latest build](https://github.com/CrazalothAI/Cassius/releases/tag/latest). The archive contains `Cassian.exe` and the `Cassian.vst3` bundle. Successful builds from pushes to `main` update this prerelease; check [GitHub Actions](https://github.com/CrazalothAI/Cassius/actions) for build status.

Run `Cassian.exe` for standalone use. Windows needs the Microsoft Edge WebView2 Evergreen Runtime to display the editor. For a DAW, copy the entire `Cassian.vst3` bundle into its VST3 plugin location and rescan. The build does not replace installed plugins automatically.

The native window, executable, and supported system tray use the Cassian wolf logo. The tray menu offers **Show Cassian** and **Quit Cassian**; closing the main window quits the app normally. Linux/X11 tray support exists in source, but this release does not provide a validated Linux distribution.

## Start playing

1. Connect your guitar and headphones/speakers to your audio interface. Input 1 and outputs 1–2 are the initial defaults. In standalone, choose any physical guitar input with the footer's **Guitar input** menu; **Audio settings** opens driver/device/output selection. In a DAW, route the intended guitar input to Cassian's first input channel.
2. In standalone, open **Audio settings** in the footer (or **Options → Audio/MIDI Settings**) to select your driver and outputs. A DAW controls its own device, sample rate, and buffer size.
3. Choose a starting preset from the header. Clean, ambient, piezo, rock, lead, metal, and extended-range voices are available. **Natural Nylon** provides a neutral DI starting point for a real nylon or piezo input.
4. Select **Amp** below the head to choose Lumen, Ferrum, a NAM capture, or Natural DI. The built-in amps work without downloading captures.
5. Raise Master gradually from its default −12 dB. Use the IN/OUT and stage meters to check levels, then save the result in **Library → Presets → Save current rig**.

For difficult passages, open **Practice & record**, load a backing track and choose **Speed** from 50–150%. Pitch stays unchanged. Preparation pauses the track and preserves its cursor and loop points; press Play when ready. The timeline uses the original track's seconds. **Loop edge fade** applies short fades around the seam (5 ms by default) to reduce clicks without shortening the loop. Metronome/count-in BPM remains independently set. See [practice and recording](docs/PRACTICE-RECORDING.md) for preparation, memory and sound-quality limits.

### Audio interfaces

Cassian has no manufacturer whitelist. Standalone supports interfaces exposed through its JUCE audio backends, including ASIO and Windows Audio on Windows. A VST3 host manages the interface itself. Compatibility depends on the installed driver and its supported channels, rates, and buffers; each hardware model has not been independently tested.

On a first launch with exactly one ASIO input/output device, Cassian selects it and requests 48 kHz / 128 samples, input 1, and outputs 1–2. With multiple ASIO choices or no ASIO device, it opens **Audio/MIDI Settings** so you can choose the driver, input, output, and channels. For interfaces without ASIO, choose the available Windows Audio backend. Turn off **Mute audio input** in that dialog when ready to monitor your selected input.

Subsequent launches preserve the saved driver, physical channels, timing, and monitoring setting for any manufacturer. If the requested device is unavailable and JUCE falls back to another device, monitoring starts muted and audio settings opens; choose the intended interface and unmute it. Device selection does not force a saved input back to input 1. The footer's **Guitar input** menu selects one physical channel, including the second input of a two-channel interface; the JUCE settings dialog can display paired channel groups.

Use the interface's direct-monitor/mix control to hear processed playback without doubling the dry guitar. On AudioBox USB 96, turn **Mixer** toward **Playback**. Set the chosen input gain below clipping. If crackling coincides with the dropout warning, use the footer's buffer menu or audio settings to try 256 or 512 samples. Larger buffers give processing more time at the cost of additional latency.

## Amp and cabinet routing

The universal amp slot separates the sound source from the older Clean/Lead channel flag.

| Amp source | Behavior |
|---|---|
| **Current rig** | Compatibility mode for older sessions. Clean uses the built-in clean path and bypasses neural captures and the external cabinet. Lead uses the loaded amp capture, or Ferrum when none is loaded. |
| **Lumen** | Built-in clean amp with gentle saturation, compression, and clean voicing. Pedal and cabinet slots remain available. |
| **Ferrum** | Built-in oversampled high-gain amp. It stays selected even if a NAM file is retained in the rig. |
| **NAM capture** | Plays the selected clean or distorted NAM through the same universal slot. The channel switch cannot bypass it. A missing capture leaves this source silent and reports the problem. |
| **Natural DI** | Bypasses the electric amp path, tight/drive shaping, and adaptive hum removal. Optional pedal and external body IR processing remain available. |

On the Cab page, load responses into A and B, then enable **Second cabinet**. B blend is a linear balance: 0% A, 100% B; identical responses retain their level at 50%. If only one enabled response exists, it plays at full blend weight. Level controls still apply. Pan balances the stereo channels without turning a stereo IR into mono. Alignment adds a manual delay to either response; polarity inversion is available for each. Responses keep the existing automatic silence trimming and energy normalization, so alignment is relative to the trimmed responses. This is not automatic phase alignment or a virtual microphone-position model.

Library cabinet details offer **Use as cabinet B**. Full-rig captures in Auto bypass both responses. Cabinet Off bypasses the cabinet cuts as well. Common cuts also work with the built-in speaker; individual A/B controls apply only to external responses.

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

Factory entries currently consist of Cassian's own built-in amps and control starting points. No third-party NAM captures or WAV responses are bundled. **Articulate lead** uses Cassian overdrive into Ferrum; **Studio clean** uses Lumen with post-cab compression. Both work without external files.

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

Native asset imports and successful stage loads create content-verified copies in the shared library. Its default location is `%APPDATA%\Cassian\Library` on Windows and `~/.config/Cassian/Library` on Linux. Standalone and plugin instances share this catalog; session state also retains its references. Edited metadata and deleted rigs survive writes from older instances. Factory favorites remain editor-profile preferences.

**Export rig JSON** writes a small reference-only document. **Export pack** writes a `.cassian.zip` containing the current rig and its selected amp, pedal, and both cabinet files, up to 64 MB per file. Identical A/B cabinet assets are packaged once. **Import pack** validates checksums and supported files, stores managed copies, and adds a saved rig without changing the active sound. Importing ordinary JSON still requires its referenced files or content-verified relinking. Packs contain your selected third-party assets; share them only within the applicable asset permissions.

Saved complete rigs and native A/B prepare all selected models and the cabinet off the audio thread before committing. Guitar fades briefly out and back in around activation; delay and reverb tails keep running. This is not gapless dual-engine scene switching. A failed preparation leaves the previous rig active. DAW session restoration retains its existing asynchronous stage-loading behavior for compatibility.

The optional **Match A/B loudness** checkbox uses recent guitar/input RMS measurements to adjust the recalled amp output, preserving Master. Play comparable phrases before capturing and switching. It is an approximate comparison aid, capped at ±12 dB correction, and skips matching when usable measurements are absent.

Header presets are **control starting points**, distinct from saved complete rigs. They preserve input calibration and Master, but choose their own amp source, EQ, and effects. Clean presets select Lumen; dirty presets retain compatibility routing. With a supported user-loaded EVH pack, some dirty presets can select a sibling capture variant. Save a complete rig when exact asset recall matters.

## Controls and effects

The head keeps six primary controls: **Drive, Bass, Middle, Treble, Space, and Master**. Space adjusts reverb mix. The stage row opens detailed controls in signal order without hiding the amp.

| Stage | Controls |
|---|---|
| **Input** | Input gain, Auto trim, gate threshold/release, pick-attack shaping, and hum-removal status. |
| **Pedal** | Built-in Cassian overdrive (Drive, Tone, Level, Low cut) followed by one optional NAM pedal capture. Independent smoothed bypasses and NAM input/output trims. |
| **Amp** | Source, NAM capture type and metadata level matching, output level, Tight, compressor routing/mix/threshold/ratio/attack/release/makeup, Presence, and High cut. |
| **Cab** | Parallel WAV responses A/B, B enable/blend, independent levels, stereo balance, polarity and 0–10 ms alignment, common low/high cuts, cabinet mode, selection/removal. |
| **EQ** | Four post-cabinet bands: Body (120 Hz shelf), Mud (350 Hz bell), Focus (1.2 kHz bell), and Fizz (4.8 kHz shelf), each ±12 dB. Flat EQ and Smooth distortion starting settings. |
| **Effects** | Stereo chorus mix/rate/depth; delay time/mix/width/feedback and tempo divisions; Room/Chamber/Hall reverb voicing, size, damping, and pre-delay; micro-delay, resonance reduction, sub-octave blend, and electric piezo simulation. |

Delay feedback ranges from 0–85%, defaulting to the previous 35%. Width offsets right-channel repeats while keeping dry guitar centered. Tempo sync uses the standalone/metronome tempo or a playing DAW's BPM, with quarter, eighth, dotted eighth, sixteenth, half, and whole-note divisions. Turning sync off restores the retained manual time.

Chorus uses independent left/right modulation to widen mono cleans and starts bypassed. Reverb offers three voicings of the same stereo feedback network, rather than separate modeled plate/spring algorithms. Pre-delay moves only the wet sound. Compressor routing defaults to **Lumen only · legacy**, preserving the old clean path. **Pre-amp** adds compression before overdrive/pedal/amp; **Post-cab** uses stereo-linked compression after the cabinet and before effects. Both work with Lumen, Ferrum, NAM, and Natural DI. **Off** disables compression. Compression at 0% is dry, including makeup gain. NAM captures retain their own modeled dynamics in addition to any selected external compressor. Pedal Input drives the capture harder or softer; Output changes its level afterward. Metadata matching changes NAM output gain without changing its internal drive.

Turn gate, attack, sub, and piezo controls fully down to switch them off. Tight at 20 Hz and High cut at 20 kHz are bypassed. The input gate detects the dry signal and also gates the amp output before delay/reverb, preserving effect tails. Adaptive hum removal learns mains noise during quiet passages; Ferrum also uses input noise shaping when the gate is enabled.

For input calibration, play your hardest notes and press **Auto trim** on the Input stage. It adjusts software input gain toward a −12 dBFS peak. Set the interface's physical gain below clipping first; software trim cannot undo clipping at the interface.

Drag a dial vertically or horizontally; hold Shift for fine adjustment. Arrow keys step through its range, Page Up/Down make larger steps, Home/End select the limits, and double-click resets it. A click without dragging leaves its value unchanged. Native gestures and parameter changes participate in DAW automation.

The header provides preset selection, previous/next, Revert for edited presets, A/B, tuner, and metronome. The tuner analyzes pitch while open; the sub-octave effect can also request pitch analysis. The editor supports a minimum 860 × 620 size, keyboard-accessible tabs, and a library dialog with focus handling.

## Backing tracks and metronome

In standalone, open **Practice & record** above the amp. Load a local mono/stereo backing track, adjust its separate volume, seek, and use **Set A**, **Set B**, and **Loop A–B** to repeat a section. Play resumes from the cursor; Pause keeps the cursor and Stop rewinds. Supported file types come from the native file chooser (WAV, AIFF, FLAC and Ogg on the current build). Tracks are decoded and resampled off the audio thread into bounded memory, up to 256 MiB of stereo float audio at the interface rate (about 11.7 minutes at 48 kHz). Failed imports preserve the previous track.

Choose an Off/1-bar/2-bar count-in using the metronome tempo and beats per bar, then **Play** or **Record guitar**. Recording asks for a destination folder and creates a unique take directory with **Guitar dry.wav** (raw mono input before Input gain) and **Guitar processed.wav** (stereo guitar after effects, before Master/limiter). Both are 32-bit float at the current interface rate, with matching frame counts. Backing tracks and clicks are excluded. **Finish take** pauses playback and finalizes both WAV headers; **Open take folder** reveals the files. The processed file preserves headroom above 0 dBFS, so set an appropriate playback level when importing it elsewhere. Monitoring uses the existing protected output mix.

The practice transport is standalone-only; a DAW owns playback and recording in the VST3. In a supporting DAW, enable the optional stereo backing bus to route accompaniment through the plugin. Changing rigs and presets does not change practice controls. Tracks, cursor and active takes are not saved in rigs or restored into an autoplaying session. Rate/buffer changes stop practice transport and finalize an active take; loaded tracks are rebuilt for the new rate. See [practice and recording](docs/PRACTICE-RECORDING.md) for limits and validation.

Open **Take library** within Practice & record to search, name and favorite recordings. New takes enter automatically with an original rig snapshot; **Import take folder** adds older Cassian dry/processed pairs. **Listen** reviews the selected original, dry DI or reamp version with a separate volume, pausing backing playback and muting live guitar and the click. **Reamp with current rig** renders the dry file through an isolated copy of your current guitar chain and saves a new stereo float WAV plus its rig snapshot. Originals stay unchanged. Exports retain the original rate and frame count, omit Master/backing/clicks, and can be cancelled. See [take library and reamping](docs/TAKE-LIBRARY.md) for storage and limits.

The backing bus, standalone player and metronome are mixed **after guitar distortion and effects**, then share Master and the output limiter. The metronome offers tempo, tap tempo, beats per bar, and click level. Standalone uses its own clock; while a DAW is playing, it follows host tempo and bar position. Presets and A/B preserve click settings. Delay can follow that tempo or use its manual time.

Simplified guitar path:

```text
Input gain → hum/piezo/input shaping → gate → optional pre-amp compressor
→ Cassian overdrive → NAM pedal → selected amp → cabinet A/B blend
→ optional post-cab compressor → amp output/tone shaping → high cut/post-amp gate → EQ
→ stereo chorus → stereo delay → voiced reverb → micro-delay
→ backing/metronome mix → Master → output limiter
```

Optional resonance reduction precedes the pedal; the parallel sub-octave layer joins after the cabinet. Stages are bypassed according to the selected source and controls.

## Troubleshooting

| Symptom | Check |
|---|---|
| No guitar | Selected physical input, intended driver/output pair, Mute audio input in standalone audio settings, Master, load-error banner, and selected Amp source. Explicit NAM requires a loaded capture. |
| Dry/doubled guitar | Turn the interface Mixer toward Playback and check for another monitored DI path in the DAW. |
| Hollow or fuzzy capture | Check capture type and cabinet mode; avoid an unintended second cabinet. Reduce Drive or pedal gain, and try Smooth distortion EQ. |
| Noise while idle | Raise the gate threshold enough to close between notes, with a release that preserves sustain. |
| Crackling over notes | Check input clipping and the dropout warning. Reduce interface gain if clipped; increase buffer size when callbacks overrun. A gate does not repair clipped audio or dropouts. |
| Feedback with a backing track | Confirm the guitar input is not receiving a loopback mix and that accompaniment reaches the backing bus or a separate playback path. Lower speaker/listening level if sound is feeding back acoustically. |
| Missing rig files | Managed copies survive moved downloads. Relink if the managed file is also missing. Rig JSON references files; portable packs include them. |
| Wrong sound after selecting a preset | Presets choose control/source settings. Recall a saved complete rig for exact files and routing. |

For an independent Windows interface check, close apps holding the driver and run `build/CassianAudioCheck_artefacts/Release/CassianAudioCheck.exe --list`. Then choose a listed driver/device, for example `CassianAudioCheck.exe --driver "ASIO" --device "Your interface driver name" --input 1 --output 1`. For separate endpoints use `--input-device` and `--output-device`. Optional `--rate`, `--buffer`, and `--outputs` flags select timing and mono/stereo playback. `--help` describes the options. With no arguments, the diagnostic uses the sole ASIO device. It opens the chosen interface for three seconds, reports callbacks, input peak, timing, and xruns, and sends silence to the outputs without recording audio.

## Build from source

### Windows requirements

- Visual Studio 2022 Build Tools with Desktop development with C++, MSVC v143, and a Windows 10/11 SDK.
- CMake 3.24+, Git, and Node.js/npm. CI uses Node 22.
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

CMake fetches pinned JUCE 8.0.6, NAM Core 0.5.4, Signalsmith Stretch 1.3.2 and Signalsmith Linear 0.6.4 revisions, installs locked frontend dependencies, builds the UI, and embeds its single HTML resource. Stretch prepares accompaniment on the worker; it adds no stretching work or latency to live guitar processing. The native app does not need a development server. The build helper also supports existing `.deps/JUCE`, `.deps/nam`, `.deps/signalsmith-stretch` and `.deps/signalsmith-linear` source checkouts.

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
| `Source/LibraryStore.h` | Shared manifest merging, atomic persistence, managed asset copies, and deleted-rig tracking. |
| `Source/CompleteRig.cpp` | Off-thread complete rig preparation and short fade around activation. |
| `Source/RigPack.cpp` | Portable ZIP export/import, entry limits, content verification, and managed storage. |
| `Source/PracticeEngine.*` | Worker-prepared backing/review playback and paired guitar recording through a bounded audio FIFO. |
| `Source/TakeLibrary.*` | Take catalog, review selection, rig snapshots, and isolated offline reamp exports. |
| `Source/PluginEditor.*` | Embedded WebView editor, native bridge, and asynchronous file pickers. |
| `Source/Standalone.cpp` | Standalone entry point, device setup, and tray integration. |
| `Source/dsp/` | NAM/IR processing, resampling, amp/speaker voicing, filters, gates, and effects. |
| `Source/params/ParameterIDs.h` | Native parameter layout; new routing parameters follow existing automation indices. |
| `ui/src/` | React editor, library, presets, and parameter/native bridge tests. |
| `tests/` | Native processor, catalog/rig, model, and interface diagnostic checks. |
| `scripts/` | Windows build and pinned SDK setup. |
| `.github/workflows/build.yml` | Windows build, UI/native tests, artifacts, and main-branch prerelease publishing. |

File reading, hashing, parsing, capture preparation, warm-up, and managed storage run outside the audio callback. The callback processes bounded internal chunks with preallocated buffers and only tries the model-swap lock; contention mutes the guitar while preserving backing, click, Master, and limiting. Cabinet publication serializes JUCE's wait-free convolution handoff with processing; retired handoff objects are reclaimed by the loader. Tone controls and capture gain changes are smoothed; resonance coefficients update at a reduced control rate. Hum and resonance telemetry use atomic snapshots.

Sample-rate conversion retains streaming state between callbacks. New controls append without moving the previous 58 parameter indices; old complete rigs receive compatible defaults, including bypassed chorus and manual delay. See [audio quality update](docs/AUDIO-QUALITY-UPDATE.md) for engineering details and remaining limits.

Verified on Windows on **2026-10-03**: **77 UI tests**, **four native CTest suites**, and standalone/VST3 Release builds pass. Regressions include parameter parity and legacy recall, routing, gates/EQ, resampling, backing/click isolation, concurrent IR publication, complete rig preparation during callbacks and failed-preparation rollback, chorus stereo width, measured delay timing, reverb pre-delay/decay, pedal levels, compressor dynamics, A/B matching, shared/stale library writers, managed copies, and portable packs with tamper/path rejection. Additional checks cover overdrive harmonics and exact bypass, stereo-linked compression on every source, dual-IR level/polarity/pan/alignment, cabinet B recall and pack deduplication, and older rig defaults. Practice and take checks cover paired float recordings, looping/count-in, catalog persistence, review cancellation, offline reamp consistency with byte-identical originals, pitch preservation at 50/75/150%, cancelled preparation, real-time recording isolation and loop-seam fade reduction. See [sound foundation](docs/SOUND-FOUNDATION.md) for local callback timings and limits. Additional checks use user-supplied amp, pedal, and cabinet files, which remain excluded from Git.

Interface checks cover brand-independent device matching, arbitrary physical channel masks, restored non-ASIO and older combined-name settings, missing/ambiguous startup choices, fallback microphone rejection, and the standalone input/settings controls. Diagnostic `--help` and `--list` were verified on the development machine. Other manufacturers' hardware has not been physically tested.

Automated renders do not establish live sound quality, long-run AudioBox reliability, or compatibility across DAW hosts. The latest foundation pass did not perform Linux host validation or live AudioBox listening. The production UI build reports an existing `eval` warning from the official JUCE native interop shim.

## Roadmap, attribution, and dependencies

See the [expansion roadmap](docs/EXPANSION-ROADMAP.md) for ordered pedalboards, more effects, power-amp processing, MIDI/scenes, parallel amp paths, and asset sourcing. The current chain has a built-in overdrive, one neural pedal, one amp, two parallel cabinet responses, and global effects.

Project ownership markers and their preservation instruction are recorded in [provenance](docs/PROVENANCE.md). Third-party attribution and licenses are listed in [THIRD_PARTY.md](THIRD_PARTY.md). JUCE uses AGPLv3 or a commercial license; NAM Core is MIT licensed. Review the applicable dependency and asset distribution terms before shipping a commercial build. User-imported captures are marked unverified for factory redistribution and are excluded from this repository. Rig JSON contains references; user-exported portable packs contain selected files.
