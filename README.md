# Cassian

Cassian is a guitar processor with a tube-head interface, built-in clean and high-gain amps, Neural Amp Modeler (NAM) support, cabinet convolution, and stereo effects. It runs as a Windows standalone app or VST3 plugin, with a JUCE/C++20 audio engine and an embedded React editor.

The current source adds listening-only Play Along controls and clear Tone, Board, Practice, and Takes views, with a persistent saved rig name and direct Save/Save As. Standalone practice includes backing-track playback, paired guitar recording, a searchable take library, and offline reamping. The sound engine includes dual-IR cabinets, built-in overdrive, compression, modulation and stereo effects, plus prepared complete rig recall, a shared managed library and portable rig packs. It is a development build; the remaining work toward a commercial release is tracked in the [expansion roadmap](docs/EXPANSION-ROADMAP.md) and [next steps](docs/NEXT-STEPS.md).

**Serial pedalboards are now available in Board.** Enable serial editing, then add, duplicate, replace, bypass and reorder pedals in before-amp and after-cabinet lanes. Each instance has independent controls and persistent automation slots, with Undo/Redo. The Ambience block plays recorded reverb/delay responses up to 30 seconds. Existing rigs retain their original audio path until explicitly converted. New exports use schema 3 and scenes use version 3; older rigs, sessions, packs and takes migrate when read. See [pedalboard state](docs/PEDALBOARD-STATE.md).

Use **Library → Import sound ZIPs** to add supported NAM/WAV collections without extracting folders. Amps, captured drives, cabinets and recorded ambience have separate library categories. In a serial board, assign a captured pedal or ambience response in its selected-pedal inspector, then switch it on. The [October sound intake](docs/SOUND-INTAKE-OCTOBER.md) records the supplied packs and their limits.

**0.3.0 improves audio for video editing.** Reamps can retain delay/reverb tails, and soundtrack exports support start/end trimming with adjustable edge fades. Recordings stay untouched, including when mixing original backing with an extended reamp. See [0.3.0 release notes](docs/RELEASE-0.3.0.md).

**0.4.0 adds wah and envelope filtering.** Add two independent wah pedals before the amp or after the cabinet, sweep manually or follow your picking, and assign each Position to a MIDI expression CC. Envelope Clean and Vowel Lead are complete built-in tones. See [wah setup](docs/WAH-PEDAL.md) and [0.4.0 notes](docs/RELEASE-0.4.0.md).

**0.4.1 recovers tones from Takes.** Load the original recorded rig or a selected reamp's rig without changing input calibration or listening levels. Existing audio and snapshots stay untouched. See [take recovery](docs/TAKE-LIBRARY.md) and [0.4.1 notes](docs/RELEASE-0.4.1.md).

**0.4.2 organizes saved rigs.** In Library → Presets, select your saved rig to rename it, add styles/gain/tags and notes, or duplicate its saved settings. Metadata edits preserve unsaved playing edits. See [saved-rig organization](docs/SAVED-RIG-ORGANIZATION.md).

**0.5.0 improves the studio workflow.** Saved rigs load from the header; Library shows their referenced sounds and relink actions; reamp versions can be named; Takes offers sorting and quick guitar-only WAV export. See [the five-part update](docs/RELEASE-0.5.0.md).

**0.5.1 adds take audition loops.** Pause/resume and A/B looping help audition riffs and reamp versions. Review identifies the loaded take/version and protects it from controls intended for another selection. Listening loops leave recordings and export ranges unchanged. See [take auditioning](docs/RELEASE-0.5.1.md).

**0.5.2 adds review waveforms.** Find riffs in the loaded take or reamp by clicking its waveform or using keyboard seeking. Waveforms follow the loaded version and its A/B loop. See [review waveforms](docs/RELEASE-0.5.2.md).

**0.5.3 improves sound discovery.** Filter missing/available sound files and distinguish amp-only, preamp-only and full-rig captures from built-in amps and direct input. See [library discovery](docs/RELEASE-0.5.3.md).

**0.7.2 adds pedal Reset controls.** Restore one instance while keeping its bypass/file assignment; Undo restores your settings. See [pedal reset](docs/RELEASE-0.7.2.md).

**0.7.1 adds an original spring-style reverb.** Two independent pedals offer decay, damping tone, drip, pre-delay and blend without sound downloads. See [spring reverb](docs/RELEASE-0.7.1.md).

**0.7.0 expands the ready-to-play tones.** Iron Rhythm and Velvet Lead use the new distortion pedal; Prism Clean and Midnight Space use the new plate while keeping clean decays open. All four work without sound downloads. See [0.7.0 notes](docs/RELEASE-0.7.0.md).

**0.6.2 adds an original stereo plate.** A separate reverb pedal brings decay, damping tone, pre-delay, width and blend to cleans and leads. See [plate reverb](docs/RELEASE-0.6.2.md).

**0.6.1 adds a dedicated distortion pedal.** Add original Hard, Asymmetric or Fuzz clipping to the serial board, with independent duplicate controls and no capture-file dependency. See [distortion](docs/RELEASE-0.6.1.md).

**0.6.0 connects review to export.** Copy a review or recalled section into the export range, save guitar-only excerpts or a mixed video soundtrack, and check actual duration/peak attenuation after saving. This milestone includes searchable take notes and persistent sections. See [0.6.0 notes](docs/RELEASE-0.6.0.md).

**0.5.6 adds saved take sections.** Name and revisit A-B review ranges across app restarts, including reamped versions. See [saved take sections](docs/RELEASE-0.5.6.md).

**0.5.5 adds searchable take notes.** Keep tuning, tempo and performance reminders alongside recordings without changing their audio. See [take notes](docs/RELEASE-0.5.5.md).

**0.5.4 exports saved tones directly.** Export a selected saved rig as references or a portable sound pack while keeping your current playing edits. Exports retain the snapshot selected before the save dialog opens. See [saved-rig exports](docs/RELEASE-0.5.4.md).

## Download and launch

This README describes the development source; public downloads can lag the current feature branch. Local milestone builds are not automatically published or merged.

Download **[Cassian-Setup.exe](https://github.com/CrazalothAI/Cassius-Guitar-DAW/releases/download/latest/Cassian-Setup.exe)** from [Latest Windows download](https://github.com/CrazalothAI/Cassius-Guitar-DAW/releases/tag/latest), run it, and follow the installer. Then open **Cassian** from the Windows Start menu. Setup offers an optional desktop shortcut and VST3 plugin; administrator access is not required. It checks for Microsoft WebView2 and installs the runtime when missing (internet access is needed for that step).

The current source version is **0.7.2 preview**. Builds also include versioned installer/portable copies, `SHA256SUMS.txt` and `Cassian-Build.json`. The editor footer identifies the running version. These files become available when this source reaches the release workflow; pushing a feature branch alone does not replace the main-branch download. See [versioned previews](docs/RELEASE-VERSIONING.md).

The app installs to `%LOCALAPPDATA%\Programs\Cassian`. Run a newer installer to update it in place; close Cassian and any DAW using its plugin first. Uninstall through **Windows Settings → Apps → Cassian**. Saved settings, managed captures/rigs, practice sections and recordings are retained. Optional VST3 installation uses `%LOCALAPPDATA%\Programs\Common\VST3\Cassian.vst3`; rescan your DAW and add that location to its plugin paths if needed.

For portable use, download **Cassian-Windows.zip**, extract it, and open **Cassian.exe directly inside the extracted folder**. No build/Release/Standalone folder search is needed. Portable use requires an installed WebView2 Runtime. **GitHub's “Source code (zip)” contains source files, not a runnable app.** Successful `main` builds publish the installer and portable ZIP; check [GitHub Actions](https://github.com/CrazalothAI/Cassius-Guitar-DAW/actions) for status. Releases are currently development prereleases.

The native window, executable, and supported system tray use the Cassian wolf logo. The tray menu offers **Show Cassian** and **Quit Cassian**; closing the main window quits the app normally. Linux/X11 tray support exists in source, but this release does not provide a validated Linux distribution.

## Start playing

1. Connect your guitar and headphones/speakers to your audio interface. Input 1 and outputs 1–2 are the initial defaults. In standalone, choose any physical guitar input with the footer's **Guitar input** menu; **Audio settings** opens driver/device/output selection. In a DAW, route the intended guitar input to Cassian's first input channel.
2. In standalone, open **Audio settings** in the footer (or **Options → Audio/MIDI Settings**) to select your driver and outputs. A DAW controls its own device, sample rate, and buffer size.
3. Choose a starting preset from the header. Clean, ambient, piezo, rock, lead, metal, and extended-range voices are available. **Natural Nylon** provides a neutral DI starting point for a real nylon or piezo input.
4. In **Tone**, select **Amp** below the head to choose Lumen, Ferrum, a NAM capture, or Natural DI. The built-in amps work without downloading captures.
5. Raise Master gradually from its default −12 dB. Use the stage meters to check levels, then choose **Save as** in the rig bar and name your complete rig. **Save** updates that entry after edits.

For difficult passages, open **Practice**, load a backing track and choose **Speed** from 50–150%. Pitch stays unchanged. Preparation pauses the track and preserves its cursor and loop points; press Play when ready. The timeline uses the original track's seconds. **Loop edge fade** applies short fades around the seam (5 ms by default) to reduce clicks without shortening the loop. Metronome/count-in BPM remains independently set. See [practice and recording](docs/PRACTICE-RECORDING.md) for preparation, memory and sound-quality limits.

### Find your workspace

| View | Purpose |
|---|---|
| **Tone** | Full amp head, main knobs, amp source and cabinet details. |
| **Board** | Compact amp strip and all six fixed stages in signal order. Multiple independent pedal instances/reordering are planned. |
| **Practice** | Compact amp strip, backing waveform, transport, sections and recording. |
| **Takes** | Compact amp strip, take search, review and reamping. |

The rig bar stays visible in every view: name, **Edited**, selected amp identity, **Save**, **Save as**, and complete-rig **A/B**. Header starting points are labeled separately. **Library**, **Mix**, and **Performance** are available beside the view tabs; **Audio settings** remains in the standalone footer. Arrow keys, Home and End move between tabs. Review can be stopped from the footer after leaving Takes; an active recording provides a link back to Practice.

### Play along with YouTube or other backing

Open **Mix** and choose **Bring guitar forward** for +3 dB Guitar balance and 45% Mix focus. Start browser/video volume around 25%, then adjust to taste. Cassian cannot control browser audio; the two applications mix at your interface. For Cassian's loaded backing track, adjust its separate backing volume in Mix or Practice; in a DAW, use the host's backing fader.

**Guitar balance** (−12 to +12 dB) follows the amp/effects. **Mix focus** (0–100%) adds broad 1.2 kHz definition, reduces 120/350 Hz overlap and gently softens the high shelf. Both start neutral, work with clean and distorted tones, and ramp smoothly. **Neutral mix** resets only these two controls. Rig, scene, starting-point and A/B changes preserve them; native sessions save them. They neither mark a saved tone Edited nor alter dry/processed recordings or offline reamps. Internal backing and metronome bypass them.

The Mix warning reports peaks reaching the −0.5 dBFS pre-limiter ceiling within the last second. It does not measure limiter gain reduction. Lower Master if warned; an external browser track is outside this measurement. See [Play Along validation](docs/PLAY-ALONG-NAVIGATION.md) for test coverage and limits.

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

The preset menu now offers **18 complete built-in rigs** and **20 exact-capture recipes** covering clean, ambient, blues, rock, lead, metalcore and fuzz sounds. Red-II is selected explicitly for harder EVH distortion; British rock/crunch uses the JCM800 or Brit 50 captures. Complete recall replaces files, the serial board and scenes, preserving global listening/performance controls. Legacy control starting points remain separately labeled and no longer switch all distorted presets to Red-I. Missing exact sounds disable a capture recipe instead of substituting another head. See [presets and shared sounds](docs/PRESETS-AND-SHARED-SOUNDS.md).

The owner's 103 supplied sounds are imported locally. A verified bank can accompany the installer/portable ZIP and import automatically on a new PC. Public third-party sound distribution is still pending source/license records; built-in rigs need no files. The bank pipeline preserves creator metadata and includes per-asset permissions/notices. No third-party NAM/WAV files have been added to Git during this update.

### Import and use assets

- Import multiple `.nam` files into Amps or Pedals, or `.wav` files into Cabinets. Validation runs on the loader thread; importing does not change the playing rig.
- Select **Use** to activate an asset. Using an amp selects the NAM source; using a cabinet selects External IR mode.
- Identical file contents within a category share a SHA-256 ID. Reimporting a duplicate preserves edited metadata and favorites, and records renamed file paths.
- Missing assets appear in the catalog. **Relink** accepts a file with the original content hash, including a renamed copy; a different capture must be imported as a different asset.

NAM support includes mono-input/mono-output WaveNet, LSTM, and A2/SlimmableContainer models in supported file versions 0.5.x through 0.7.0. Captures and pedal models are resampled when their embedded sample rate differs from the host. Models without a declared rate run at the host rate. Load errors appear in the editor.

Cabinet files must be mono or stereo WAVs no longer than ten seconds. Stereo IRs retain their left/right response. A fixed ambience WAV can be imported, but this does not turn it into an adjustable reverb pedal.

### Export audio for video

Record a take in Practice, then open **Takes → Video soundtrack → Export for video**. Export the selected processed, dry or reamped version as a **48 kHz / 24-bit stereo WAV**, with optional recorded backing and separate balance controls. New takes store a synchronized backing stem alongside the original guitar stems. The worker preserves originals, supports cancellation and reduces mixed peaks only when needed for −1 dBFS sample-peak headroom. Browser audio, metronome/count-in and listening-only Play Along controls remain excluded. Import the WAV into Clipchamp with your video; see [recording/export steps](docs/VIDEO-AUDIO-EXPORT.md).

### Save, recall, and share

**Save as** in the rig bar (or **Save current rig** in Library) stores the selected amp source, capture classification, cabinet mode, stage files, tone controls, bypass states and scenes as a new complete rig. **Save** updates the active saved entry; an unsaved or deleted entry prompts for a new name. Tone/asset/scene edits show **Edited**, while listening controls do not. The name and comparison baseline survive native session restoration and complete-rig A/B. Browser preview saves control settings only.

Native **A/B** compares complete rig state. Rig recall preserves input calibration, Master, metronome and Play Along controls by default; reopening a standalone or DAW session restores its full saved state. Failed recall or saving keeps the previous identity and reports an error.

Use **Import rig** and **Export current rig** for `.cassian.json` documents. Imported rigs enter the library without changing the sound; select Use to recall them. Recall validates the document and its referenced assets before changing parameters. Missing or changed assets, invalid values, and incomplete documents are rejected with an error.

New schema-3 documents require all 159 parameters and a supported fixed or serial board. The first 87 host parameters retain their IDs and positions; 72 independent controls append afterward. Schema 1/2 retain their established defaults and fill the new controls on read. Invalid routing, unsupported versions and incomplete modern documents reject before replacing the active sound. Keep originals for older Cassian builds; current exports require a compatible schema-3 build.

Native asset imports and successful stage loads create content-verified copies in the shared library. Its default location is `%APPDATA%\Cassian\Library` on Windows and `~/.config/Cassian/Library` on Linux. Standalone and plugin instances share this catalog; session state also retains its references. Edited metadata and deleted rigs survive writes from older instances. Factory favorites remain editor-profile preferences.

**Export rig JSON** writes a small reference-only document. **Export pack** writes a `.cassian.zip` containing the current rig and its selected amp, both captured pedals, both cabinet files, and both ambience responses, up to 64 MB per file. Identical content within an asset kind is packaged once. **Import pack** validates checksums and supported files, stores managed copies, and adds a saved rig without changing the active sound. Importing ordinary JSON still requires its referenced files or content-verified relinking. Packs contain your selected third-party assets; share them only within the applicable asset permissions.

Saved complete rigs and native A/B prepare all selected models and the cabinet off the audio thread before committing. Guitar fades briefly out and back in around activation; delay and reverb tails keep running. This is not gapless dual-engine scene switching. A failed preparation leaves the previous rig active. DAW session restoration retains its existing asynchronous stage-loading behavior for compatibility.

The optional **Match A/B loudness** checkbox uses recent guitar/input RMS measurements to adjust the recalled amp output, preserving Master. Play comparable phrases before capturing and switching. It is an approximate comparison aid, capped at ±12 dB correction, and skips matching when usable measurements are absent.

Header presets are **control starting points**, distinct from saved complete rigs. They preserve input calibration and Master, but choose their own amp source, EQ, and effects. Clean presets select Lumen; dirty presets retain compatibility routing. With a supported user-loaded EVH pack, some dirty presets can select a sibling capture variant. Save a complete rig when exact asset recall matters.

## Controls and effects

### MIDI foot control

Open **Performance** for eight CC/program-change assignments and **Learn controller**. Recall saved complete rigs, toggle overdrive/NAM pedal/EQ/gate/metronome, or use CC expression for Master, Drive, reverb and delay mix. In standalone, enable the controller's input checkbox; in VST3, route MIDI through your DAW. Mapping starts disabled and stays with the native app/DAW session when rigs change. CC switches need a release below 64 before their next press; PC numbers use 0–127. See [MIDI foot control](docs/MIDI-FOOT-CONTROL.md) for setup, ranges, persistence and timing limits.

### Tone controls

The Effects page includes an original **Modulation** pedal with phaser, flanger and tremolo, smoothed switching, stereo Spread and tempo sync. Try **Velvet tremolo**, **Phase lead** or **Jet rock**. It runs after EQ and before chorus/delay/reverb, with a MIDI bypass assignment. Older rigs and ordinary starting presets keep it bypassed. See [modulation pedal](docs/MODULATION-PEDAL.md) for controls, routing and validation.

The Tone head keeps six primary controls: **Drive, Bass, Middle, Treble, Space, and Master**. Space adjusts reverb mix. Board exposes all detailed stages below a compact Drive/Master strip.

| Stage | Controls |
|---|---|
| **Input** | Input gain, Auto trim, gate threshold/release, pick-attack shaping, and hum-removal status. |
| **Pedal** | Built-in Cassian overdrive (Drive, Tone, Level, Low cut) followed by one optional NAM pedal capture. Independent smoothed bypasses and NAM input/output trims. |
| **Amp** | Source, NAM capture type and metadata level matching, output level, Tight, compressor routing/mix/threshold/ratio/attack/release/makeup, Presence, and High cut. |
| **Cab** | Parallel WAV responses A/B, B enable/blend, independent levels, stereo balance, polarity and 0–10 ms alignment, common low/high cuts, cabinet mode, selection/removal. |
| **EQ** | Four post-cabinet bands: Body (120 Hz shelf), Mud (350 Hz bell), Focus (1.2 kHz bell), and Fizz (4.8 kHz shelf), each ±12 dB. Flat EQ and Smooth distortion starting settings. |
| **Effects** | Stereo chorus mix/rate/depth; delay time/mix/width/feedback and tempo divisions; Room/Chamber/Hall reverb voicing, size, damping, and pre-delay; micro-delay, resonance reduction, sub-octave blend, and electric piezo simulation. |

Delay feedback ranges from 0–85%, defaulting to the previous 35%. Width offsets right-channel repeats while keeping dry guitar centered. Tempo sync uses the standalone/metronome tempo or a playing DAW's BPM, with quarter, eighth, dotted eighth, sixteenth, half, and whole-note divisions. Turning sync off restores the retained manual time.

In the original fixed chain, chorus uses independent left/right modulation to widen mono cleans and starts bypassed. Reverb offers three voicings of the same stereo feedback network, rather than separate modeled plate/spring algorithms. Pre-delay moves only the wet sound. Compressor routing defaults to **Lumen only · legacy**, preserving the old clean path. **Pre-amp** adds compression before overdrive/pedal/amp; **Post-cab** uses stereo-linked compression after the cabinet and before effects. Both work with Lumen, Ferrum, NAM, and Natural DI. **Off** disables compression. Compression at 0% is dry, including makeup gain. NAM captures retain their own modeled dynamics in addition to any selected external compressor. Pedal Input drives the capture harder or softer; Output changes its level afterward. Metadata matching changes NAM output gain without changing its internal drive.

Turn gate, attack, sub, and piezo controls fully down to switch them off. Tight at 20 Hz and High cut at 20 kHz are bypassed. The input gate detects the dry signal and also gates the amp output before delay/reverb, preserving effect tails. Adaptive hum removal learns mains noise during quiet passages; Ferrum also uses input noise shaping when the gate is enabled.

For input calibration, play your hardest notes and press **Auto trim** on the Input stage. It adjusts software input gain toward a −12 dBFS peak. Set the interface's physical gain below clipping first; software trim cannot undo clipping at the interface.

Drag a dial vertically or horizontally; hold Shift for fine adjustment. Arrow keys step through its range, Page Up/Down make larger steps, Home/End select the limits, and double-click resets it. A click without dragging leaves its value unchanged. Native gestures and parameter changes participate in DAW automation.

The header provides starting-point selection, previous/next, Revert for edited starting points, tuner, and metronome. Complete-rig A/B is in the rig bar. The tuner analyzes pitch while open; the sub-octave effect can also request pitch analysis. The editor supports a minimum 860 × 620 size, keyboard-accessible tabs, and library/utility dialogs with focus handling.

Four **Scenes** in the Board stage panel store rhythm, lead, clean or ambient variations using the current amp/pedal/cabinet files. Open **Edit scenes**, select a slot, name it and store the current tone; click a populated slot or assign **Recall scene** in Performance to switch. Scenes retain guitar routing/effects while preserving Input, Master, click settings and practice controls. Banks save with complete rigs, native sessions and portable packs. Existing effects keep their history, but changes to their controls can alter audible tails. See [performance scenes](docs/PERFORMANCE-SCENES.md) for behavior and limits.

## Backing tracks and metronome

In standalone, open **Practice**. Load a local mono/stereo backing track, adjust its separate volume, seek, and use **Set A**, **Set B**, and **Loop A–B** to repeat a section. Play resumes from the cursor; Pause keeps the cursor and Stop rewinds. Supported file types come from the native file chooser (WAV, AIFF, FLAC and Ogg on the current build). Tracks are decoded and resampled off the audio thread into bounded memory, up to 256 MiB of stereo float audio at the interface rate (about 11.7 minutes at 48 kHz). Failed imports preserve the previous track.

The waveform shows the cursor and loop region; click or use its arrow keys to seek. Open **Sections** to name and save the current A/B loop, replace an existing section, recall it, or delete it. Recall pauses at A and enables looping; press Play when ready. Sections persist per track content, including renamed copies, and keep their original-track timing when Speed changes. They are saved separately from tone rigs.

Choose an Off/1-bar/2-bar count-in using the metronome tempo and beats per bar, then **Play** or **Record guitar**. Recording asks for a destination folder and creates a unique take directory with **Guitar dry.wav** (raw mono input before Input gain) and **Guitar processed.wav** (stereo guitar after effects, before Master/limiter). Both are 32-bit float at the current interface rate, with matching frame counts. Backing tracks and clicks are excluded. **Finish take** pauses playback and finalizes both WAV headers; **Open take folder** reveals the files. The processed file preserves headroom above 0 dBFS, so set an appropriate playback level when importing it elsewhere. Monitoring uses the existing protected output mix.

The practice transport is standalone-only; a DAW owns playback and recording in the VST3. In a supporting DAW, enable the optional stereo backing bus to route accompaniment through the plugin. Changing rigs and presets does not change practice controls. Tracks, cursor and active takes are not saved in rigs or restored into an autoplaying session. Rate/buffer changes stop practice transport and finalize an active take; loaded tracks are rebuilt for the new rate. See [practice and recording](docs/PRACTICE-RECORDING.md) for limits and validation.

Open **Takes** to search, name and favorite recordings. New takes enter automatically with an original rig snapshot; **Import take folder** adds older Cassian dry/processed pairs. **Listen** reviews the selected original, dry DI or reamp version with a separate volume, pausing backing playback and muting live guitar and the click. **Reamp with current rig** renders the dry file through an isolated copy of your current guitar chain and saves a new stereo float WAV plus its rig snapshot. Originals stay unchanged. Exports retain the original rate and frame count, omit Master/backing/clicks, and can be cancelled. See [take library and reamping](docs/TAKE-LIBRARY.md) for storage and limits.

The backing bus, standalone player and metronome are mixed **after guitar distortion and effects**, then share Master and the output limiter. The metronome offers tempo, tap tempo, beats per bar, and click level. Standalone uses its own clock; while a DAW is playing, it follows host tempo and bar position. Presets and A/B preserve click settings. Delay can follow that tempo or use its manual time.

Simplified guitar path:

```text
Input gain → hum/piezo/input shaping → gate → optional pre-amp compressor
→ Cassian overdrive → NAM pedal → selected amp → cabinet A/B blend
→ optional post-cab compressor → amp output/tone shaping → high cut/post-amp gate → EQ
→ phaser/flanger/tremolo → stereo chorus → stereo delay → voiced reverb → micro-delay
→ guitar recording tap → listening-only Guitar balance / Mix focus
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

To produce the same Windows downloads locally:

```powershell
./scripts/package-windows.ps1
./scripts/test-windows-installer.ps1
```

Packaging puts **Cassian-Setup.exe**, **Cassian-Windows.zip**, and **Cassian.exe** in the repository's top-level folder. The setup compiler is downloaded from its pinned, hash-checked official release; its publisher signature and Microsoft's runtime bootstrapper signature are checked. Installer tests use a separate application identity and a temporary workspace location, checking install, shortcuts, optional VST3, upgrade and uninstall. Existing Cassian installations are untouched. The binaries statically link the MSVC runtime. See [Windows distribution](docs/WINDOWS-DISTRIBUTION.md) for packaging, prerequisite and signing details.

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
| `Source/PracticeEngine.*` | Worker-prepared backing/review playback and aligned dry/processed/backing recording through a bounded audio FIFO. |
| `Source/PracticeSections.*` | Per-track named loops, validation and atomic shared persistence. |
| `Source/TakeLibrary.*`, `Source/VideoAudioExport.cpp` | Take catalog, review selection, rig snapshots, isolated reamps and video soundtrack WAV export. |
| `Source/MidiControl.*` | Bounded MIDI queue, control worker, Learn, assignments and session configuration. |
| `Source/PerformanceScenes.h` | Four guitar-parameter snapshots, bank validation, recall and rig/session persistence. |
| `Source/PedalboardState.h` | Bounded fixed-board identities, legacy migration and stable automation bindings. |
| `Source/dsp/ModulationPedal.h` | Stereo phaser/flanger/tremolo, smoothed bypass/voice transitions and tempo subdivisions. |
| `Source/PluginEditor.*` | Embedded WebView editor, native bridge, and asynchronous file pickers. |
| `Source/Standalone.cpp` | Standalone entry point, device setup, and tray integration. |
| `Source/dsp/` | NAM/IR processing, resampling, amp/speaker voicing, filters, gates, and effects. |
| `Source/params/ParameterIDs.h` | Native parameter layout; new routing parameters follow existing automation indices. |
| `ui/src/` | React editor, library, presets, and parameter/native bridge tests. |
| `tests/` | Native processor, catalog/rig, model, and interface diagnostic checks. |
| `scripts/`, `installer/` | Windows build, pinned SDK/tool setup, installer and portable packaging, installation smoke tests. |
| `.github/workflows/build.yml` | Windows build, UI/native tests, artifacts, and main-branch prerelease publishing. |

File reading, hashing, parsing, capture preparation, warm-up, and managed storage run outside the audio callback. The callback processes bounded internal chunks with preallocated buffers and only tries the model-swap lock; contention mutes the guitar while preserving backing, click, Master, and limiting. Cabinet publication serializes JUCE's wait-free convolution handoff with processing; retired handoff objects are reclaimed by the loader. Tone controls and capture gain changes are smoothed; resonance coefficients update at a reduced control rate. Hum and resonance telemetry use atomic snapshots.

Sample-rate conversion retains streaming state between callbacks. New controls append without moving the previous 58 parameter indices; old complete rigs receive compatible defaults, including bypassed chorus and manual delay. See [audio quality update](docs/AUDIO-QUALITY-UPDATE.md) for engineering details and remaining limits.

Verified on Windows on **2026-10-06**: **132 UI tests**, **four native CTest suites**, and standalone/VST3 Release builds pass. Regressions include parameter parity and legacy recall, routing, gates/EQ, resampling, backing/click isolation, concurrent IR publication, complete rig preparation during callbacks and failed-preparation rollback, chorus stereo width, measured delay timing, reverb pre-delay/decay, pedal levels, compressor dynamics, A/B matching, shared/stale library writers, managed copies, and portable packs with tamper/path rejection. Additional checks cover overdrive harmonics and exact bypass, stereo-linked compression on every source, dual-IR level/polarity/pan/alignment, cabinet B recall and pack deduplication, and older rig defaults. Practice and take checks cover aligned guitar/backing float recordings and 48 kHz / 24-bit video WAV export, looping/count-in, catalog persistence, review cancellation, offline reamp consistency with byte-identical originals, pitch preservation at 50/75/150%, cancelled preparation, real-time recording isolation and loop-seam fade reduction. See [sound foundation](docs/SOUND-FOUNDATION.md) for local callback timings and limits. Additional checks install the complete 103-file user bank into relocated empty storage and recall/render all 20 exact capture recipes. Supplied sound data remains excluded from Git pending redistribution records. Reference signal checks do not replace live playing, DAW host or fresh-PC validation.

Interface checks cover brand-independent device matching, arbitrary physical channel masks, restored non-ASIO and older combined-name settings, missing/ambiguous startup choices, fallback microphone rejection, and the standalone input/settings controls. Diagnostic `--help` and `--list` were verified on the development machine. Other manufacturers' hardware has not been physically tested.

MIDI checks cover Learn/cancel, switch edges, channels, expression ranges/inversion, invalid state and overlapping bindings, bounded queue overflow, stale-command rejection, worker recovery, rig recall and preserved globals. See [MIDI foot control](docs/MIDI-FOOT-CONTROL.md). Modulation checks measure phaser/flanger cancellation, tremolo rate/depth, stereo movement, click-free transitions, bounded feedback, legacy defaults, MIDI bypass and saved-tempo offline renders; see [modulation pedal](docs/MODULATION-PEDAL.md). Scene checks cover every saved parameter, globals, legacy/invalid banks, failed-load rollback, UI/MIDI recall, portable packs and uninterrupted delay history; see [performance scenes](docs/PERFORMANCE-SCENES.md).

Automated renders do not establish live sound quality, long-run AudioBox reliability, or compatibility across DAW hosts. This update did not perform physical MIDI controller, DAW MIDI routing, Linux host or live AudioBox listening validation. The production UI build reports an existing `eval` warning from the official JUCE native interop shim.

## Roadmap, attribution, and dependencies

See the [expansion roadmap](docs/EXPANSION-ROADMAP.md) for more effects, power-amp processing, MIDI/scenes, parallel amp paths, and asset sourcing. Serial boards support up to 16 independently controlled blocks, with two slots per effect type, including two neural pedals and two recorded ambience responses. They surround one amp and two parallel cabinet responses; legacy rigs retain their fixed routing until converted.

[Sound intake](docs/SOUND-INTAKE.md) records the supplied capture families, calibration and cabinet-routing evidence, clean/jazz/nylon gaps, and missing redistribution documentation. It does not add third-party factory assets or claim a completed guitar audition.

Project ownership markers and their preservation instruction are recorded in [provenance](docs/PROVENANCE.md). Third-party attribution and licenses are listed in [THIRD_PARTY.md](THIRD_PARTY.md). JUCE uses AGPLv3 or a commercial license; NAM Core is MIT licensed. Review the applicable dependency and asset distribution terms before shipping a commercial build. User-imported captures are marked unverified for factory redistribution and are excluded from this repository. Rig JSON contains references; user-exported portable packs contain selected files.
