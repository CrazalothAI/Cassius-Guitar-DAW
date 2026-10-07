# Versioned preview builds

Cassian 0.7.1 is a development preview. Spring reverb is documented in [0.7.1 notes](RELEASE-0.7.1.md). Signing, sound redistribution clearance, sustained interface testing and real DAW validation remain release work.

## One version source

`ui/package.json` is authoritative. CMake reads that version for the standalone and VST3, the standalone reports JUCE's generated version, the native status reports it to the editor, and packaging uses it for the installer. The footer shows the connected engine's version, or the UI version in a browser preview. Both root version records in `ui/package-lock.json` must agree. Numeric major/minor/patch components are limited to 0–255 to fit JUCE's version encoding.

For the next version, run `npm version <major.minor.patch> --no-git-tag-version` in `ui`, commit the resulting source changes, and rebuild both formats. This command does not publish a Git tag or release. CMake rejects mismatched version metadata; Windows packaging rejects stale app/plugin version resources. The isolated upgrade smoke installer can deliberately use an older installer version while keeping the test binary unchanged.

## Identify each download

Normal packaging retains `Cassian-Setup.exe`, `Cassian-Windows.zip` and the root `Cassian.exe`. It also emits `Cassian-0.7.0-Setup.exe`, `Cassian-0.7.0-Windows.zip`, `SHA256SUMS.txt` and `Cassian-Build.json` (using the current version). Versioned copies are byte-identical to their convenient aliases. The ZIP still opens directly to Cassian.exe.

The checksum file covers the versioned installer and ZIP. Build metadata records the version, checkout at packaging, tracked source modifications, private-bank status, sound count and those hashes. Checksums establish file integrity, not publisher authenticity; they are not a substitute for code signing. A checkout reference identifies packaging context and is not proof that an arbitrary supplied executable was compiled from that commit.

In PowerShell 7, `./scripts/package-windows.ps1 -Release` additionally rejects modified tracked source, untracked build inputs, smoke installers and private-sound flags. Build and test the committed revision before using it. `-Standalone` and `-OutputDirectory` remain available. Source archives without Git metadata must be built from a checkout before packaging.

The pinned JUCE build originally omitted the version-information input from its Windows resource dependencies. Cassian adds that dependency so an incremental version bump updates FileVersion/ProductVersion in both binaries rather than retaining the older resource.

## GitHub previews

The workflow builds/tests before publishing. Main continues to replace the `latest` development prerelease with easy download aliases plus versioned files, metadata and checksums. A separately authorized push of a tag such as `v0.7.0` must match the source version and produces a versioned prerelease; it does not overwrite an existing versioned release. This update does not create or push a release tag, merge main, sign binaries or publish third-party sounds.

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
