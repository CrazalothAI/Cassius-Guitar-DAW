# Cassian user guide

In standalone **Takes → Recover interrupted recording**, choose the original take folder to recover a separate readable audio copy. Listen to Original processed, acknowledge your review, then Confirm recovered take to enable export/reamping. For a long take, Open take folder and explicitly acknowledge review in another player. Original files stay intact. Recovery validates all copied stems and keeps a local report; unreadable headers/uncheckpointed tails are not repaired. New recordings checkpoint their WAV headers on the disk worker about every two seconds. See [take recovery and limits](TAKE-LIBRARY.md).

In standalone, **Library → Automatic tone recovery** keeps up to 64 changed reference snapshots, checking once a minute when recording/loading/export/backup operations are idle. You can disable automatic snapshots, capture immediately, or add a recovered saved preset without changing your playing tone. Sound files and recordings are not copied into these snapshots; keep personal archives on another drive. See [tone recovery](RELEASE-1.1.1.md).

For personal library/take protection, open **Library → Backup & recovery**. Create a verified archive on another drive, or restore recovered copies without replacing existing work. Complete backups are the default. Clear **Include recorded takes and reamps** for tones only, or enable **Choose specific takes** and select recordings for a smaller archive. All library sounds/tones stay included. Archives support up to 32 GiB; ZIP64 is selected automatically when needed. Allow disk space for the archive and an expanded verification copy. See [backup and recovery](BACKUP-AND-RECOVERY.md) for included files and DAW/session exclusions.

Recovery validates new practice/review sections and preserves existing ones. If optional section files are skipped, your audio and rigs still recover. Choose **Show saved files** and inspect **Recovery report.json** for individual results; verified originals remain alongside recovered media.

Cassian is a guitar workstation for live amp/pedal tones, backing-track practice, recording and reamping. The 1.0 release candidate targets Windows x64 standalone and VST3. Linux source exists but has no validated release package. All 22 built-in rigs include their sounds; the 20 capture recipes require the listed user-imported files.

## Install and connect

Download **Cassian-Setup.exe**, close any previous Cassian/DAW instance, and run Setup. Standalone is selected by default; VST3 is optional. Setup installs to your user profile and offers shortcuts. WebView2 is installed when missing and needs internet for that step. Portable users must extract the complete ZIP and already have WebView2. GitHub's source archive is for building, not running the app.

Connect guitar and headphones/speakers to the interface. Open **Help & setup → Open audio settings**, choose the driver and interface, select playback outputs, then select the guitar's physical input using the footer. Enable **Monitor guitar input** only after selecting the intended input/output. Start with Master low and keep interface input gain below clipping. Use the interface's playback monitoring to avoid hearing dry and processed guitar together.

Try **Prism Clean**, **Velvet Lead** or **Iron Rhythm** in the preset selector. Every built-in rig works without capture downloads. **Natural DI** is a neutral source for appropriate acoustic/piezo inputs; body IRs are optional. Capture recipes name missing dependencies and do not silently substitute a different head.

In a DAW, select the interface, guitar track input and monitoring in the host. Scan `%LOCALAPPDATA%\Programs\Common\VST3` if necessary. The host controls audio hardware and MIDI routing; standalone monitoring controls are unavailable inside the plugin.

## Choose and save tones

**Tone** shows the amp head and amp/cab controls. **Board** edits independent serial pedals. Older rigs keep their compatibility path until you enable serial editing. **Practice** and **Takes** use compact amp strips so transport and recordings have room.

The rig bar shows the current name, amp and Edited state. **Save as** creates a named complete rig; **Save** updates it. Complete rigs retain amp/cab files, board, tone controls and scenes. Input calibration, Master, metronome and listening mix remain global during recall. Native sessions also retain those global controls.

Add, duplicate, bypass, replace and reorder pedals in Board. Distortion/NAM/overdrive remain before the amp; stereo-capable effects can use supported lanes. Two automation slots per family and 16 reserved blocks bound each board. Removing a pedal reserves its slot to preserve host automation; Undo can restore it. Reset controls affects only the selected instance. Structural edits prepare a graph and briefly fade the guitar; they are not gapless and can restart tails.

## Scenes and MIDI

Scenes store four variations sharing the current loaded files. Outside Edit scenes, click a populated slot to recall it. While editing, selecting a slot does not recall it. Rename changes its saved title; Copy saved scene uses its saved settings, not later live edits. Occupied destinations explicitly show replacement. Save the complete rig after organizing scenes.

Open **Performance** to select one of eight MIDI assignments, Apply, optionally Learn, then enable mapping and the physical port. In a DAW, route MIDI from the host. Switches use CC press/release or PC. Expression needs CC. Independent distortion/plate/spring numbers identify automation slots, not card order; missing/deleted targets report an error. Bindings belong to the app/DAW session rather than exported rigs.

## Play along and record

With YouTube/browser audio, adjust browser volume and use **Mix → Bring guitar forward** as a listening starting point. Cassian does not capture browser audio. To record backing with guitar, load the audio file into **Practice**. You are responsible for permission to publish any backing material.

Practice offers pitch-preserving 50–150% speed, waveform seeking, A–B looping, named sections and count-in. BPM is set independently; Cassian does not automatically detect the backing tempo. Record dry/processed guitar; an internally loaded backing track also produces a synchronized stem. Metronome/count-in are excluded from the recorded soundtrack. Play Along focus/balance affect listening only.

## Review, reamp and export

Open Takes to search, name, favorite and annotate recordings. Listen to processed audio, Dry DI or a reamp; seek its waveform and save named sections. Review controls apply only after the selected version loads. Pause resumes from the same position. Reamping produces a separate version and preserves the original recording and rig snapshot.

For a full guitar file, use the guitar WAV shortcut. For excerpts, set/recall review A–B, expand Video soundtrack and choose **Use review A–B**. Export a guitar-only range or a guitar/backing soundtrack. Exports are 48 kHz / 24-bit stereo WAVs. The completion report shows actual duration and sample-peak attenuation. Export does not include browser audio, Master, Mix focus or metronome. Import the WAV into Clipchamp and synchronize it with camera footage; interface/camera delay can require an offset.

## Library, portability and backups

Library imports NAM captures and WAV cabinets/ambience, supports tags/favorites and missing-file relinking, and distinguishes capture types. Full-rig captures can already include a cabinet; Auto avoids adding another. Preamp-only classification does not add a separate power-amp simulator.

Save/export rigs before changing installations. Reference exports contain file references; portable packs include referenced files, subject to their redistribution permissions. Paid installers/support do not grant rights to third-party captures. The public candidate includes original sounds, not the owner's private bank.

Normal library/take storage is under `%APPDATA%\Cassian`, except externally referenced imported files and chosen recording/export destinations. Uninstall preserves settings, rigs and recordings. Portable use shares normal user storage; it is not a fully self-contained profile. Use Library → Backup & recovery for complete personal rig/take archives within its limits. Save DAW sessions, MIDI/device preferences and external media projects separately before reinstalling Windows or moving PCs.

## Troubleshooting and support

Before evaluating tones, open **Help & setup → Check your guitar input**. Choose your guitar's physical input, enable Monitor guitar input, then click **Check input for 8 seconds** and play your hardest chords/notes. The meter is raw input before software trim and effects. Review the sampled peak and warnings; only **Apply suggested input trim** changes Input, aiming near −12 dBFS with a ±12 dB limit. If clipping is detected, lower the interface's physical gain and check again. The gate cannot repair clipped input. This sampled check can miss short transients and is not a capture's dBu calibration. Stop recording/take playback first, and recheck after device, channel, rate, buffer or hardware-gain changes. DAW users choose routing in the host; the installed plugin can check its live input when callbacks are running.

- **No sound:** confirm interface input/output, Monitor guitar input (or DAW monitoring), selected guitar channel, input meter and Master. Try a built-in rig to isolate missing captures.
- **Crackle/dropouts:** check clipping first, then try a larger buffer. Reports indicate missed deadlines, not proof that every crackle has the same cause. Avoid loading complex assets repeatedly while playing.
- **Missing sounds:** use Library's Missing files filter and relink. Capture recipes remain unavailable until their exact dependencies exist.
- **Guitar buried under backing:** lower the backing/browser level; adjust Mix for listening. Set the separate guitar/backing export balance for recordings.
- **Plugin absent:** enable VST3 during Setup, add the current-user VST3 directory to the host scan paths, then rescan.

Help & setup contains a support report showing version, device names and audio settings. Review before copying; it excludes recordings, paths and imported sound names. Nothing is submitted automatically. Add OS/driver versions and reproduction steps to [GitHub issues](https://github.com/CrazalothAI/Cassius-Guitar-DAW/issues). Paid support details will come from the seller's offer, not an assumed promise in the app.

Source-code rights are described in [COPYRIGHT.md](../COPYRIGHT.md), [LICENSE.txt](../LICENSE.txt) and [THIRD_PARTY.md](../THIRD_PARTY.md). The matching source ZIP accompanies distributed builds at no extra charge.
