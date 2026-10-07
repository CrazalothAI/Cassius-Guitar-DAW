# Complete presets and shared sounds

Implemented 2026-10-06. The header now offers complete capture recipes and Cassian built-in rigs. Legacy control recipes remain separately labeled for compatibility; they no longer switch every distorted sound to Red-I or select Blue-I for rock. Complete recall replaces the amp, files, independent serial board and scenes while preserving Input, Master, metronome and Play Along controls. Save creates your own editable rig; the original starter remains available.

## Sound choices

As of 0.8.0 there are 22 file-free built-in rigs and 20 exact-capture recipes. The latter reference content hashes, so renamed files work and distinct capture variants cannot be silently substituted.

| Family | Capture recipes |
| --- | --- |
| Hard distortion | 5153 Red-II Tight / Metalcore / Neoclassical; Red-III Rhythm; Rectifier Classic Heavy / 808 Rhythm |
| Rock and lead | Red-I Singing Lead; British 800 Crunch / 80s Lead; Brit 50 Plexi Crunch; Guvnor Rock Drive |
| American edge and blues | American Super Edge / Klon Blues / Timmy Drive |
| Clean and ambient | 5153 Green-I Studio / Spring; Green-II Chorus; Green Ambient Plate |
| Alternative and fuzz | Rat Alternative; Russian Fuzz Wall |

Red-II is the harder EVH choice requested by the owner. Blue captures stay available as individual amp files in Library; the rock/crunch recipes use contrasting heads. Green presets play their actual NAM captures through the universal slot. The Super Reverb source is an edge-of-breakup capture, not a claimed pristine Twin/JC-120. Brit 50's filename specifies CAB despite amp-only metadata; this recipe explicitly uses Full rig and still needs a listening check. JCM800 and Rectifier full rigs bypass a second cabinet. The Rectifier 808 recipe does not add a second drive to the already captured boost.

Captured pedal variants have fixed settings. Changing their Input/Output controls adjusts their operating levels; it does not recreate every knob of the original hardware. Recorded spring/plate/Flint responses retain their captured timing and decay. The regular Delay/Reverb blocks remain adjustable.

The local managed library now contains all 103 unique supplied sounds, including 28 EVH variants, the prior Mesa/Fortin files and October intake. It contains originals by content identity, not copies of recordings or user settings. Library offers combined style, gain, speaker, source-pack, favorites and rig-type filters. Literal filename hints are identified as hints; editable metadata can correct them. A missing exact file disables its recipe and names the missing sounds. Built-in rigs always remain playable without those files.

Output trims were adjusted using repeatable synthetic signals. Input calibration and model input gains were not changed to make quiet heads louder. These checks establish finite output, useful reference levels and headroom; they do not establish a finished musical audition or perceptual loudness matching on a real guitar.

## Original distortion and plate starters (0.7.0)

These four complete rigs contain only original built-in DSP and are included with the app and source. They require no sound files. Existing starter IDs and the 20 exact-capture recipes are unchanged.

| Rig | Complete signal path | Intended use |
| --- | --- | --- |
| Iron Rhythm | Hard distortion → Natural DI → built-in 4x12 → EQ | Tight, dry metal/metalcore rhythm |
| Velvet Lead | Asymmetric distortion → Lumen → Delay → Plate | Mid-forward lead and fast melodic runs |
| Prism Clean | Lumen → Compressor → Plate | Clear chords, fingerstyle and clean melodies |
| Midnight Space | Compressor → Lumen → Chorus → Delay → Plate | Spacious clean swells and layers |

Prism Clean and Midnight Space have no distortion pedal and leave the input gate off. Plate pre-delay separates the wet response from the attack; lowering Delay/Plate Blend makes the dry instrument more prominent. Iron Rhythm and Velvet Lead use modest gate settings which still need adjustment for the instrument. None of these rigs changes Input calibration, Master, metronome or Play Along controls.

The Distortion block has Hard, Asymmetric and Fuzz voices, bass control, tone and blend; Plate has nominal decay, damping tone, pre-delay, wet width and blend. Both support two independent instances, host automation, complete saving, scenes, sessions and portable packs. Plate is a separate algorithm from Room/Chamber/Hall and recorded ambience; see [distortion](RELEASE-0.6.1.md) and [plate](RELEASE-0.6.2.md).

## Spring, blues and fuzz starters (0.8.0)

Four more original rigs are included with every app package without capture downloads:

| Rig | Complete signal path | Intended use |
| --- | --- | --- |
| Copper Blues | Low-drive Overdrive → Lumen → Spring | Warm edge-of-breakup blues |
| Country Spring | Lumen → Compressor → Spring | Articulate clean picking |
| Surf Clean | Lumen → Compressor → Spring | Clean melody with a brighter, drippier tail |
| Fuzz Orbit | Fuzz distortion → Natural DI → built-in 4x12 → EQ → Plate | Saturated rock/doom texture |

Country Spring and Surf Clean leave the gate off and have no drive pedal. Spring is an original dispersive algorithm with adjustable nominal decay, damping tone, Drip, wet pre-delay and Blend. Copper Blues keeps pedal input/output calibration modest; Fuzz Orbit explicitly uses the separate built-in Fuzz mode. Output trims are measured on the same synthetic reference as the previous starters; real guitar/perceptual audition remains necessary. See [spring](RELEASE-0.7.1.md) and [0.8.0](RELEASE-0.8.0.md).

## Distribution and installation

0.5.3 adds File status filters in sound tabs and Capture type in Amps. Types use the existing stored classification, including an explicit Unknown choice; selecting a full rig explains Auto cabinet behavior. File presence is not a hash/format or sound-quality check. Filters do not modify metadata or routing. Saved-rig dependencies remain individually inspectable in Presets. See [library discovery](RELEASE-0.5.3.md).

For your own presets, 0.4.2 adds renaming, searchable style/gain/tags/notes and exact saved-tone duplication. Select a saved rig in Library → Presets to organize it without resaving its tone. See [saved-rig organization](SAVED-RIG-ORGANIZATION.md).

`scripts/prepare-sound-bank.ps1` creates a portable `Sounds` directory from a managed library. The bank contains a relative, content-addressed manifest, the selected original files and notices. It excludes user paths, rig/session settings, favorites and recordings. The app validates every hash before importing the bank into the new user's own managed library; repeat launches do not duplicate entries or replace edited metadata. Installed standalone and VST3 share that user library. Portable users extract the whole ZIP, keeping Sounds beside Cassian.exe.

Public bank preparation requires a per-asset source, redistribution permission record and original license/permission file. An example rights record has this shape (use actual evidence, never this placeholder):

```json
{"assets":[{"id":"amp:<64-character content hash>","source":"Creator/source URL","permission":"Applicable redistribution grant","licenseFile":"Absolute path to original license or written permission"}]}
```

Run preparation with `-LibraryRoot`, a new `-OutputDirectory` and `-RightsManifest`. `-Development` creates a private bank marked unapproved. `package-windows.ps1 -SoundBank <folder>` verifies all hashes, notices and every recipe dependency, then includes Sounds in the installer and portable ZIP. A bank at `assets/sound-bank` is detected by the normal GitHub build. Private packages require the explicit `-AllowDevelopmentSounds` switch; they must not be uploaded as a public release. Installer tests can exercise that same bank with an isolated install/upgrade/uninstall directory.

**Public distribution remains pending.** The supplied archives contain no license files. Several NAM metadata records name TONE3000 as trainer, which does not establish where the owner obtained them or grant redistribution. The platform's [terms](https://www.tone3000.com/terms) restrict bundling catalog tones and require written creator/platform permission for commercial redistribution. Obtain the actual download sources and applicable grants before publishing those files. The code, built-in rigs and capture recipe definitions contain no third-party sound data and can be tested independently. No third-party capture has been pushed to GitHub during this update.

## Verification

Native checks exercise all built-in recalls on an empty library, preservation of global controls, exact board replacement, scene reset, edited identity, saving, native session restoration and portable export. An optional `CASSIAN_TEST_SOUND_BANK` path tests a complete supplied bank in a different empty library: all 103 files import, every capture recipe loads its exact identities, all paths relocate and all 20 recipes render. Both banks have bounded reference level/headroom assertions. Frontend tests cover correct Red-II/Red-I separation, contrasting crunch heads, missing-file availability, header recall, native rejection and metadata filtering.

The Windows installer regression passed with the private 103-file bank: installation verifies every packaged sound hash and the executable, shortcut and optional VST3; upgrade and uninstall preserve user data. The public packaging path rejects this bank without the explicit private-development flag.

Live interface audition, real DAW recall and fresh-PC validation remain release checks. Existing user rigs and recordings are preserved.
