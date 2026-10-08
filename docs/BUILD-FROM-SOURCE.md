# Build the distributed source

Download the matching `Cassian-<version>-Source.zip` beside the binary artifacts, extract `Cassian-source` into a short path (for example `C:/src/Cassian-source`), and check `SOURCE-MANIFEST.json` for its version and commit. It contains original tracked source, build scripts, pinned JUCE/NAM/Signalsmith source, NAM Eigen/AudioDSPTools submodules, ASIO source under its GPLv3 option and the frontend runtime packages/notices. Owner captures, recordings, local notes, credentials, build outputs and Git internals are excluded.

Install PowerShell 7, Node.js/npm and Visual Studio 2022 C++ Build Tools with CMake and a Windows SDK. From the extracted root, run `./scripts/build-windows.ps1`. The helper uses the included `.deps` dependency source. Microsoft WebView2 SDK and npm build tools still need internet during setup; a complete offline toolchain is not supplied. The output is `build/AmpSuite_artefacts/Release/Standalone/Cassian.exe` and `build/AmpSuite_artefacts/Release/VST3/Cassian.vst3`.

Modify/rebuild under AGPL-3.0-or-later while preserving original and third-party notices. An extracted source archive has no Git metadata; creating redistributed packages requires a checkout/commit so package manifests identify your actual revision. Compiler/SDK, signing service and imported sound licenses remain separate from the source archive. Never include signing private keys in source or binary packages.

Windows MSBuild file tracking can fail in long nested extraction paths even when the source files exist. If JUCE configuration reports `FTK1011`, move the extracted source to a shorter path and configure a fresh build folder. This was observed during source-bundle validation; it is not a missing dependency.

Validated on Windows on 2026-10-07: the RC1 source archive independently configured, rebuilt standalone/VST3 and passed all four native tests. The Microsoft WebView2 SDK prerequisite was reused locally, npm installed the locked build dependencies, and all C++ library sources came from the archive. This was a source-build check, not a fresh-PC installer acceptance test.
