# Versioned preview builds

Cassian 1.3.2 is a feature preview following 1.0.0 RC2. The numeric binary version is 1.3.2; `ui/src/release.json` records the preview channel separately. Native status supplies both to the UI, so a browser build cannot incorrectly relabel an older connected engine. See [original starter-tone guide](RELEASE-1.3.2.md), [guided input setup](RELEASE-1.3.1.md), [recording recovery and download cleanup](RELEASE-1.3.0.md), [validated section recovery](RELEASE-1.2.1.md) and [large personal archives](RELEASE-1.2.0.md). Signing, sound redistribution clearance, sustained interface testing and real DAW validation remain release work.

## One version source

`ui/package.json` is authoritative. CMake reads that version for the standalone and VST3, the standalone reports JUCE's generated version, the native status reports it to the editor, and packaging uses it for the installer. The footer shows the connected engine's version, or the UI version in a browser preview. Both root version records in `ui/package-lock.json` must agree. Numeric major/minor/patch components are limited to 0–255 to fit JUCE's version encoding.

For the next version, run `npm version <major.minor.patch> --no-git-tag-version` in `ui`, commit the resulting source changes, and rebuild both formats. This command does not publish a Git tag or release. CMake rejects mismatched version metadata; Windows packaging rejects stale app/plugin version resources. The isolated upgrade smoke installer can deliberately use an older installer version while keeping the test binary unchanged.

## Identify each download

Normal packaging writes one `Cassian-Setup.exe`, `Cassian-1.3.2-Windows.zip`, matching `Cassian-1.3.2-Source.zip`, `SHA256SUMS.txt` and `Cassian-Build.json` under `build/releases/1.3.2` (using the current version). It keeps the current installer and runnable `Cassian.exe` at the project root for convenient local access. Successful default packaging removes only recognized obsolete top-level package files. Custom `-OutputDirectory` retains the specified destination without root cleanup. The portable ZIP opens directly to Cassian.exe.

The checksum file covers `Cassian-Setup.exe`, the versioned portable ZIP and matching source. The installer retains its internal Windows version even though its download filename stays simple. Metadata records channel/candidate and whether signing was configured; actual signatures are independently verified by the readiness check. Build metadata records the version, checkout at packaging, tracked source modifications, private-bank status, sound count and those hashes. Checksums establish file integrity, not publisher authenticity; they are not a substitute for code signing. A checkout reference identifies packaging context and is not proof that an arbitrary supplied executable was compiled from that commit.

In PowerShell 7, `./scripts/package-windows.ps1 -Release` additionally rejects modified tracked source, untracked build inputs, smoke installers and private-sound flags. Build and test the committed revision before using it. `-Standalone` and `-OutputDirectory` remain available. Source archives without Git metadata must be built from a checkout before packaging.

The pinned JUCE build originally omitted the version-information input from its Windows resource dependencies. Cassian adds that dependency so an incremental version bump updates FileVersion/ProductVersion in both binaries rather than retaining the older resource.

## GitHub previews

The workflow builds/tests before publishing. Main replaces the `latest` development prerelease with exactly one installer and the current version's portable/source files, metadata and checksums. Both artifact upload and release creation list exact current-version paths; there are no wildcards selecting older builds. The remote-main guard prevents a superseded run replacing the newest download. A separately authorized push of a tag such as `v1.3.2` must match the source version and produces a versioned prerelease containing one installer; it does not overwrite an existing versioned release. No release tag, signature or private sound publication is created by local packaging.

Regression checks cover package/lock mismatch, invalid versions, stale binaries, version resources, checksum/alias consistency, installed version registration during upgrade and removal of the isolated registration on uninstall. These tests do not establish fresh-PC prerequisite readiness or audible performance.

Previous 0.3.0 validation on Windows on 2026-10-06: 134 UI tests, all four native suites, Release standalone/VST3 builds, version input checks and installation/upgrade/uninstall with the private 103-file bank passed. Both binary resource versions are 0.3.0; the deliberately older smoke installer registers 0.0.1 before upgrading to 0.3.0. No live interface, Linux or real DAW check was performed in this patch.

0.4.0 validation on Windows on 2026-10-06: 136 UI tests, all four native suites with the optional 103-file bank, Release standalone/VST3 builds, binary version checks and isolated installer install/upgrade/uninstall passed. Both binaries report 0.4.0. See [0.4.0 evidence and limits](RELEASE-0.4.0.md).

0.4.1 validation on Windows on 2026-10-06: 139 UI tests, all four native suites with the optional 103-file bank, Release standalone/VST3 builds, binary version checks and isolated installer install/upgrade/uninstall passed. Both binaries report 0.4.1. See [0.4.1 evidence and limits](RELEASE-0.4.1.md).

0.4.2 validation on Windows on 2026-10-06: 143 UI tests, all four native suites with the optional 103-file bank, Release standalone/VST3 builds, binary version checks and isolated installer install/upgrade/uninstall passed. Both binaries report 0.4.2. See [0.4.2 evidence and limits](RELEASE-0.4.2.md).

0.5.0 validation on Windows on 2026-10-06: 150 UI tests, all four native suites with the optional 103-file bank, Release standalone/VST3 builds, binary version checks and isolated installer install/upgrade/uninstall passed. Both binaries report 0.5.0. See [0.5.0 evidence and limits](RELEASE-0.5.0.md).

0.5.1 validation on Windows on 2026-10-07: 154 UI tests, all four native suites with the optional 103-file bank, Release standalone/VST3 builds, binary version checks and isolated installer install/upgrade/uninstall passed. Both binaries report 0.5.1. See [0.5.1 evidence and limits](RELEASE-0.5.1.md).

0.5.2 and 0.5.3 each passed their full UI/native checks on Windows on 2026-10-07 (156 and 159 UI tests respectively). The combined 0.5.4 build passed 161 UI tests, four native suites with the optional 103-file bank, Release standalone/VST3 builds, version checks and isolated installer install/upgrade/uninstall. Both final binaries report 0.5.4. See [combined evidence and limits](RELEASE-0.5.4.md).

0.5.5 and 0.5.6 each passed full UI/native checks on Windows on 2026-10-07 (163 and 165 UI tests respectively). The combined 0.6.0 milestone passed 168 UI tests, four native suites with the optional 103-file bank, Release standalone/VST3 builds, binary version checks and isolated installer install/upgrade/uninstall. Both final binaries report 0.6.0. See [evidence and limits](RELEASE-0.6.0.md).

0.6.1 and 0.6.2 each passed full UI/native checks on Windows on 2026-10-07 (169 and 170 UI tests). The combined 0.7.0 milestone passed 171 UI tests, four native suites with the optional 103-file bank, Release standalone/VST3 builds, binary version checks and isolated installer install/upgrade/uninstall. Both final binaries report 0.7.0. See [evidence and limits](RELEASE-0.7.0.md).

0.7.1 and 0.7.2 each passed full UI/native checks on Windows on 2026-10-07 (172 and 173 UI tests). The combined 0.8.0 milestone passed 174 UI tests, four native suites with the optional 103-file bank, Release standalone/VST3 builds, binary version checks and isolated installer install/upgrade/uninstall. Both final binaries report 0.8.0. See [evidence and limits](RELEASE-0.8.0.md).

For 1.0, stable publication requires a stable channel, committed manual acceptance records, clean public packages, matching source and trusted timestamped signatures. The workflow fails the stable gate when these are missing. Its automated release publications remain prereleases; final stable publication is separately authorized after all acceptance passes. See [release preparation](COMMERCIAL-RELEASE.md).

RC2 release packaging requires three level-10 independent VST3 validation runs for the compiled plugin hash. Metadata retains validator/seed/hash details; unsigned package integrity verifies that the shipped plugin is the tested binary. This is additional automated evidence and does not mark real DAW acceptance passed.
