# Cassian 1.0.0 RC1

This candidate prepares Windows distribution for the owner's open-source code / paid installers and support model. It is **unsigned** and is not a completed stable commercial release.

## Changes

- Adopt AGPL-3.0-or-later for original source, retain component licenses and Crazaloth provenance, and package project/user/dependency notices.
- Add in-app Help with first-sound setup, explicit input monitoring, recording/export guidance and a reviewable support report. Reports exclude file paths, recordings and imported sound names; copying is explicit and no report is automatically sent.
- Report the engine's release channel separately from its numeric binary version. The editor shows RC1 accurately without relabeling an older engine.
- Build a matching source archive containing committed project source and pinned runtime dependency sources, including NAM submodules and verified ASIO source.
- Add package integrity and stable release readiness checks. Pending manual acceptance remains recorded as pending.
- Add conventional certificate and Microsoft Artifact Signing packaging support, including Inno installer/uninstaller signing. Provider signing remains untested until a verified account/certificate is supplied. [Owner setup instructions](WINDOWS-SIGNING.md).
- Improve Help dialog keyboard focus and missed-audio-deadline wording.

## Verification on Windows, 2026-10-07

187 UI tests and all four native suites passed, including the private 103-file bank exercised locally. Release standalone and VST3 compiled; version-resource, invalid-version/lock mismatch checks and isolated installer install/upgrade/uninstall passed. The installer included license, source reference and user guide; test user data survived. Signing logic checks covered certificate rotation, publisher matching, missing timestamps, malformed configuration and signing failure propagation using stubs; these are not evidence of provider signing.

Public package integrity passed for aliases, checksums, notices, source manifest/dependency presence and private-sound exclusion. The exported source at `0c7333f` independently configured and rebuilt standalone/VST3 with the bundled C++ dependencies and passed all four native suites; the preinstalled Microsoft WebView2 SDK was supplied as a prerequisite and npm installed its locked build tools. Subsequent changes only document the short extraction path and this evidence. Negative checks rejected a tampered installer and rejected this unsigned candidate at the stable release gate. `RELEASE-READINESS.json` lists eight pending manual evidence records, candidate channel and missing trusted signature. This candidate uses the 22 original file-free rigs for public distribution. Private third-party captures remain excluded pending redistribution permission.

The owner reports most sounds tested and sounding good. Fresh-PC prerequisites, real-user upgrade, DAW/physical MIDI acceptance, camera/export synchronization, artwork rights, seller terms and final dependency distribution review remain release acceptance work. Linux and signing-provider integration are not claimed tested. See [acceptance and commercial release](COMMERCIAL-RELEASE.md).
