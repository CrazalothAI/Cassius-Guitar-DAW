# Release acceptance matrix

Record actual results with OS/build, interface/driver, rate/buffer, input/output, DAW version where applicable, steps, outcome and any artifacts. The owner reported most sounds good on 2026-10-07; that report does not identify the full configuration matrix. Signing is deferred. Automated tests are separate from the pending manual records in `release/acceptance.json`.

| Area | Required exercise | Current evidence |
| --- | --- | --- |
| Fresh Windows setup | Install with missing WebView2; choose interface/input, enable monitoring, choose built-in sound, save/reopen a rig and record/export | Pending on a fresh environment |
| Real upgrade | Upgrade a real older install; preserve settings, managed sounds, saved rigs, scenes and takes; uninstall without deleting user data | Isolated smoke upgrade passes; real install pending |
| Sustained playing | Clean and high-gain tones, backing and metronome, recording and scene changes; 48 kHz / 128 and 256, then supported alternate rates | Owner sound feedback; device/configuration record pending |
| Actual DAW | VST3 scan; mono/stereo and backing bus; save/reopen project; automation and tempo sync; close/reopen editor | pluginval automation/processing checks; real sessions pending |
| Physical MIDI | Switches and expression including duplicate pedal slots, saved session, unavailable/deleted target | Native synthetic MIDI tests; physical device pending |
| Video soundtrack | Record camera and Cassian, export 48 kHz / 24-bit audio, align in Clipchamp and inspect start/end drift | Automated export checks; actual camera check pending |
| Commercial content | Artwork origin, per-asset redistribution grants, component notices/source delivery and seller/support terms | Original file-free rigs ship; manual content/business review pending |

Suggested actual-host coverage: the DAW(s) the owner intends to support, with exact versions recorded. Do not list an untested host as compatible. Pluginval is a host test harness, not a certification of every DAW. A second PC should be tested with a standard user account; local build/install checks on the developer machine cannot establish a fresh-PC pass.

For each completed row, keep evidence and update only its corresponding acceptance record. Failures should include reproduction steps and the reviewed in-app support report. Never include private captures, recordings or identity documents in public issues without explicit permission.
