Original Cassian code is AGPL-3.0-or-later; see [LICENSE.txt](LICENSE.txt) and [COPYRIGHT.md](COPYRIGHT.md). Paid builds include matching source. This candidate uses the pinned JUCE AGPLv3 and ASIO/VST3 GPLv3 options; no proprietary SDK agreement is claimed.

# Third-party components

- JUCE 8.0.6, revision `51a8a6d7aeae7326956d747737ccf1575e61e209`: https://github.com/juce-framework/JUCE/tree/8.0.6 — AGPLv3 or commercial licence. Two JavaScript bridge files are vendored without modifications under `ui/src/juce/` with original notices.
- NeuralAmpModelerCore 0.5.4, revision `1f42f88535884450104b8711d7595019afa0495b`: https://github.com/sdatkinson/NeuralAmpModelerCore/tree/v0.5.4 — MIT; fetched with its pinned Eigen submodule and nlohmann headers. Upstream contains their licensing files.
- WebView2 SDK 1.0.2903.40: Microsoft WebView2 SDK licence; downloaded from NuGet by the setup script. Runtime installation is separate.
- Steinberg ASIO SDK 2.3.4: https://www.steinberg.net/asiosdk — GPLv3 or the proprietary Steinberg ASIO licence. The setup script verifies the official archive's SHA-256 and extracts it into `.deps/asio`; no SDK files are committed. This local development build uses the open-source option. The matching source package includes the verified SDK source and GPL text; complete the recorded distribution review before stable sale publication.
- React, Vite, Tailwind CSS, and frontend development tools: versions in `ui/package-lock.json`; original licences included in installed packages.
- Signalsmith Stretch 1.3.2, revision `a670068d9aeb64913331d5cc29337b19a457a7df`: https://github.com/Signalsmith-Audio/signalsmith-stretch — MIT; pitch-preserving backing preparation. Notice: [Signalsmith Stretch](licenses/Signalsmith-Stretch.txt).
- Signalsmith Linear 0.6.4, revision `de55e6a50ffcf6f8f43f649692d94691c7025151`: https://github.com/Signalsmith-Audio/linear — MIT; Stretch's FFT/STFT dependency. Notice: [Signalsmith Linear](licenses/Signalsmith-Linear.txt).

No third-party amp captures or cabinet IRs are distributed with this project.

Windows installers use Inno Setup 6.7.3 (https://jrsoftware.org/), copyright Jordan Russell and Martijn Laan, under the [Inno Setup license](https://jrsoftware.org/files/is/license.txt). Its notice accompanies the installer. The compiler download is pinned by SHA-256 and its publisher signature is checked. The setup embeds Microsoft's signed WebView2 Evergreen bootstrapper, which installs the shared runtime only when missing; its distribution is covered by Microsoft's WebView2 terms. It requires internet access when installation is needed. Inno Setup asks commercial users to purchase a compiler license; dependency and asset distribution review remains part of the commercial release work.
