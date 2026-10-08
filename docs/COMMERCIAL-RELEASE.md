# Cassian commercial release preparation

The owner selected **open-source code with paid installers/support** on 2026-10-07. Original code is AGPL-3.0-or-later. Pinned JUCE uses its AGPLv3 option; pinned ASIO/VST3 source uses its GPLv3 option. Other components retain their own terms. The GPL/AGPL provisions permit combined works; preserve component notices and provide matching source. This is a source/distribution implementation record, not a guarantee that every business/asset obligation is satisfied.

Primary sources: [JUCE 8.0.6 license](https://github.com/juce-framework/JUCE/blob/8.0.6/LICENSE.md), [GNU selling guidance](https://www.gnu.org/philosophy/selling.en.html), [GPL/AGPL compatibility](https://www.gnu.org/licenses/license-compatibility.en.html), [Inno commercial license request](https://jrsoftware.org/isorder.php).

Signing was deferred by the owner on 2026-10-08; continue unsigned preview development. The current 1.3.1 preview adds guided input setup with explicit bounded trim, following recording checkpoints, verified recovery copies and one current installer per GitHub release. The 32 GiB personal backup/section recovery workflow remains available. The owner reports practical sound tests tried so far are passing; no complete hardware/DAW matrix was supplied. RC2 introduced [independent plugin validation](PLUGIN-VALIDATION.md), which remains required and does not substitute for real DAW/hardware acceptance. See the [product priorities](PRODUCT-ROADMAP.md).

## Product included in this candidate

Windows x64 standalone and VST3, 22 original file-free rigs, original effects, serial board editing, scenes/MIDI, library imports, backing practice, take recording/review/reamp and audio export. Linux is not advertised as validated. There is no account/activation server or DRM. Charging for installers/support does not revoke users' modification/redistribution rights. Source remains available at no additional charge.

The owner reports most sound checks good. `release/acceptance.json` records that statement and its scope. It does not claim a complete device/driver/buffer matrix, DAW/MIDI test pass or camera synchronization check.

## Complete before stable sale publication

1. Obtain a code-signing certificate/service and choose its verified publisher identity. No certificate was supplied. Unsigned candidates remain explicitly labeled; signing is this release process's quality gate, not a claim that unsigned selling is prohibited by law.
2. Run fresh Windows install with missing WebView2 and upgrade from a real previous installation, preserving existing data. Isolated smoke upgrades alone do not establish these passes.
3. Record real-host VST3 scanning, recall/automation and physical MIDI evidence for the advertised features. Test an exported take against camera footage in Clipchamp.
4. Confirm the supplied wolf artwork's origin/redistribution rights. Owner provenance markers do not establish artwork permissions.
5. Publish the seller identity, price, payment/download delivery, support scope/contact, refund terms and privacy information appropriate to the actual operation. Do not invent refund or support promises. The website remains deferred; a hosted storefront is a separate owner setup decision.
6. Finish dependency distribution review, including Inno's commercial compiler-license request. Original sounds can launch without third-party captures; any later factory captures require per-asset permission and notices.

Commit evidence in `release/acceptance.json`, change `ui/src/release.json` to stable only after acceptance, and rebuild the exact committed revision. Evidence must describe actual tests/rights/terms; changing booleans is not testing or obtaining permissions.

## Packaging and signing

`./scripts/package-windows.ps1 -Release` builds a public candidate from committed source, emits setup/portable/matching-source ZIPs, metadata and hashes. `./scripts/test-package-integrity.ps1` verifies their contents and consistency. `./scripts/test-release-readiness.ps1` writes `RELEASE-READINESS.json`; `-RequireReady` fails unless manual evidence, stable channel, clean public metadata, matching source and trusted timestamped app/plugin/installer signatures are present.

For Microsoft Artifact Signing onboarding, eligibility and the Azure signing command, see [Windows signing](WINDOWS-SIGNING.md). No account/certificate has been supplied; actual provider signing remains untested.

With a current-user certificate and signing provider's HTTPS RFC3161 timestamp URL:

```powershell
./scripts/package-windows.ps1 -Release -CertificateThumbprint '<40 hex digits>' -TimestampUrl '<provider HTTPS timestamp URL>'
./scripts/test-package-integrity.ps1
./scripts/test-release-readiness.ps1 -RequireReady
```

The Windows SDK SignTool can be supplied with `-SignTool`. Binary copies in staging are signed, leaving original compiler outputs intact; Inno signs Setup and its uninstaller. SHA256 digests and timestamp verification are used. This path is implemented but cannot be exercised end-to-end without a real certificate; do not claim signed release evidence yet.

Deliver `Cassian-<version>-Source.zip`, build instructions and notices beside paid binaries and keep them accessible. Source links identify the commit but do not replace the actual matching source bundle. Candidates remain GitHub prereleases. Stable publication must pass the readiness gate; no checkout, payment provider, certificate purchase, production website or customer messages are performed by this repository update.
