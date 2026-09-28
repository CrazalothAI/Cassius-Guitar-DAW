# Third-party components

- JUCE 8.0.6, revision `51a8a6d7aeae7326956d747737ccf1575e61e209`: https://github.com/juce-framework/JUCE/tree/8.0.6 — AGPLv3 or commercial licence. Two JavaScript bridge files are vendored without modifications under `ui/src/juce/` with original notices.
- NeuralAmpModelerCore 0.5.4, revision `1f42f88535884450104b8711d7595019afa0495b`: https://github.com/sdatkinson/NeuralAmpModelerCore/tree/v0.5.4 — MIT; fetched with its pinned Eigen submodule and nlohmann headers. Upstream contains their licensing files.
- WebView2 SDK 1.0.2903.40: Microsoft WebView2 SDK licence; downloaded from NuGet by the setup script. Runtime installation is separate.
- Steinberg ASIO SDK 2.3.4: https://www.steinberg.net/asiosdk — GPLv3 or the proprietary Steinberg ASIO licence. The setup script verifies the official archive's SHA-256 and extracts it into `.deps/asio`; no SDK files are committed. This local development build uses the open-source option. Review distribution licensing before release.
- React, Vite, Tailwind CSS, and frontend development tools: versions in `ui/package-lock.json`; original licences included in installed packages.

No third-party amp captures or cabinet IRs are distributed with this project.
