# Windows downloads and installation

The primary download is **Cassian-Setup.exe**. The installer installs the standalone app into `%LOCALAPPDATA%\Programs\Cassian`, adds a Start menu shortcut, offers a desktop shortcut, and registers an uninstaller in Windows Settings. This is a per-user x64 installation for Windows 10 version 1809 or newer. No administrator prompt is requested by Cassian Setup.

The optional VST3 component installs to `%LOCALAPPDATA%\Programs\Common\VST3\Cassian.vst3`, following the current-user VST3 layout. A DAW may need that folder added to its scan paths. The standalone app is the default component. An approved sound bank can be packaged and imported automatically; the owner's supplied files remain private pending redistribution clearance.

Setup detects WebView2 through Microsoft's documented per-user/machine registry entries. If missing, it runs Microsoft's signed Evergreen bootstrapper, requiring internet access, and checks again before installing Cassian. Runtime download failures produce an error rather than completing with an unusable editor. Setup does not remove the shared WebView2 Runtime on uninstall. Cassian uses the static MSVC C/C++ runtime, avoiding a separate Visual C++ Redistributable installation.

Run a newer installer to upgrade using the same application identity and installation location. Close Cassian and any DAW using its plugin beforehand; setup can request closure when files are in use. Uninstall removes installed app/plugin files and shortcuts, leaving saved app settings, managed library data, practice sections and recordings alone.

**Cassian-1.4.0-Windows.zip** (using the release version) is the portable alternative: its root contains `Cassian.exe`, the VST3 bundle, quick-start instructions, user guide, project license, source reference and third-party notices. Extract it before running. The portable app uses the same normal user settings/library locations and needs WebView2 already installed. GitHub's source ZIP is for source builds and contains no executable.

## Build and verify

After building Release standalone and VST3, run:

```powershell
./scripts/package-windows.ps1 -Release
./scripts/test-windows-installer.ps1
./scripts/test-package-integrity.ps1
./scripts/test-release-readiness.ps1
```

Packaging writes the installer, versioned portable/source ZIPs and reports under `build/releases/<current-version>`. It keeps only the current `Cassian-Setup.exe` and standalone `Cassian.exe` in the project root. After successful default packaging, recognized old top-level generated installers/ZIPs and reports are removed without recursion; recordings, installed apps and unrelated files are not scanned. Custom `-OutputDirectory` keeps its explicit destination without root cleanup. `-Standalone` can select a separately built executable. No generated binaries are committed to Git. The compiler is pinned to Inno Setup 6.7.3 with a verified SHA-256 and publisher signature. The WebView2 bootstrapper signature is verified before embedding it. Its runtime payload comes from Microsoft at install time.

Version 0.2.0 adds versioned installer/ZIP copies, `SHA256SUMS.txt` and `Cassian-Build.json`. Packaging verifies the app/plugin Windows version resources against `ui/package.json`; `-Release` requires committed source and rejects private development sounds. See [versioned previews](RELEASE-VERSIONING.md) for version bumps, integrity checks and tagged prerelease behavior.

The smoke test compiles with an isolated application identity. It installs the app, shortcut and optional plugin into a unique workspace folder, verifies the executable hash and shortcut target, upgrades from installer version 0.0.1 to the project version, and uninstalls. It checks that an unregistered user file survives upgrading/uninstalling. The real Cassian installation, Start menu and VST3 folder are untouched. Failures preserve logs; successful tests remove their temporary files and uninstall registration. CI runs this test before uploading or publishing downloads.

Validated locally: Release standalone/VST3 build, system-only DLL imports, four native CTest suites, portable archive layout, and installation/upgrade/uninstall with an existing WebView2 Runtime. Missing-runtime installation and Windows download reputation have not been tested on a fresh PC. The generated Cassian binaries/installer are currently unsigned; publisher code signing remains release work.

The main-branch Windows workflow replaces the `latest` development prerelease after tests pass. It publishes one `Cassian-Setup.exe`, the current version's portable/source ZIPs, checksums and reports. Exact paths exclude previous build versions and duplicate installer aliases. Packaging a local checkout does not publish it. The README's direct installer link points to this one current installer.

Third-party notices accompany both formats. Inno Setup supports a signing step, but no signing certificate or secrets are stored in this repository. Commercial distribution still needs the dependency/asset review described in THIRD_PARTY.md and the expansion roadmap.

## 1.0.0 candidate source and signing

Packaging now also emits `Cassian-1.0.0-Source.zip` containing the committed project and pinned runtime dependency source, plus all three artifact hashes. Deliver this alongside binaries. The current RC1 is unsigned. [Signing setup](WINDOWS-SIGNING.md) covers a conventional certificate provider and Microsoft Artifact Signing; a trusted verified account is required before either can sign real downloads. Packaging supports app/plugin/Setup/uninstaller signing and rejects failed signatures. [Commercial acceptance](COMMERCIAL-RELEASE.md) and the generated `RELEASE-READINESS.json` distinguish tested packaging from pending real-user acceptance.
