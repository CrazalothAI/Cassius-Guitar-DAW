# Path to Cassian 2.0.0

Checkpoint: 2026-10-08. The owner requested continued work toward 2.0.0. Current implementation milestone: **1.6.0 Preview**. These are delivery goals, not promises of dates, sound superiority or completed acceptance. Advance versions when the behavior, compatibility and appropriate checks are ready.

The release should make a complete guitar workflow easier: choose an interface, find a convincing clean or driven rig, play with backing, record protected DI, compare reamps, and export useful audio. Maintain local ownership, open-source code and optional paid installers/support. Keep general multitrack editing and the promotional/community website outside this app milestone.

| Milestone | Intended result | Evidence required |
| --- | --- | --- |
| **1.6 — practice progress** | Finished-session progress, exact take/version links, safe history migration, backup link remapping | Native persistence/import/recovery and UI navigation tests; real planner/second-PC feedback remains separate |
| **1.7 — first-session polish** | A concise setup-to-playing flow, visible selected input/monitor state, starter choices for clean/crunch/rhythm/lead, actionable empty/error states | Beginner workflow tests and an actual installer-to-recording trial; do not guess driver capability or claim calibration that was not measured |
| **1.8 — cabinet and tone refinement** | Audition-driven cabinet improvements and, if measurement supports it, explicit alignment/polarity tools for blended cabinets | Deterministic impulse/latency tests, rig/scene/automation migration, level-matched clean/high-gain listening and demanding-board timing |
| **1.9 — finish and preserve work** | Refine take/version comparison, recording-to-export discoverability and interruption/transfer handling using the preceding trials | Original-audio preservation tests, long-take stress, actual camera/export sync and recovery/upgrade trials |
| **2.0 — release hardening** | Freeze documented state/automation contracts, resolve regressions, ship one current installer/portable/source set with verified provenance/checksums and clear support terms | Actual host/interface acceptance, fresh-PC prerequisites and upgrades, dependency/asset distribution review and honest release-readiness records |

1.6 implements the first row. Later rows need scoped designs and measurements before code is committed; optional cabinet alignment should not replace or destabilize current rigs. Signing is deferred at the owner's request. A 2.0 preview can remain unsigned, but it must state that clearly and must not mark the existing trusted-signature stable gate passed.

## Compatibility and release rules

- Keep existing rigs, scenes, pedal identities and automation recoverable. Introduce migrations before exposing a new serialized contract; reject unsupported/corrupt inputs without overwriting user work.
- Keep listening balance/Focus, click/count-in and practice journal work outside dry/processed recording and reamp signal paths. File/catalog work belongs on workers with bounded inputs.
- Backups remain additive, and originals remain intact. Transfers must explain missing media and preserve identities or explicitly remap them.
- Retain the 22 original public starter rigs and all provenance/license notices. Broader captured sounds require per-asset redistribution rights before public packaging; owner-private files are not cleared by a software version bump.
- Test the installer, VST3 and source/metadata from the same committed revision. Passing automated checks do not substitute for real-device/host evidence or override failed checks.
- Keep one newest public installer and matching packages. Do not add obsolete executables at the repository root.

## After the core 2.0 release

Parallel/dual amps, live harmonizers/transpose, advanced streamed speed and gapless switching require separate latency/performance/recall designs. Rank them using real playing feedback after the core workflow is stable. The deferred website should provide demos, current downloads, docs, transparent roadmap and suggestions without requiring an account to use local guitar tools.

Track acceptance separately in [TODO](TODO.md), [commercial release](COMMERCIAL-RELEASE.md) and `release/acceptance.json`. A source version is evidence of shipped code, not proof of market parity or permission to redistribute captures.
