# Cassian 1.6.0 Preview

Practice history now connects elapsed work with recordings and a clear local progress view. This is the first implementation milestone on the [path to 2.0.0](ROAD-TO-2.0.md), building on 1.5.0's planner and fixed-cache streamed loops.

- Last 28 days of finished-session time, recent/previous seven-day totals, active practice days and time targets reached.
- Per-exercise/set totals and planned BPM ranges. Targets remain intentions, not measured playing accuracy or attained speed.
- Up to eight exact take/version links per session, including dry DI and independent reamps. Opening a link selects the version in Takes without playing audio or recalling a rig.
- Duplicate-resistant linking, visible unavailable identities and non-destructive unlinking. Linking/opening stays disabled during recording.
- Compatible schema-1 journal migration, bounded schema-2 validation and additive identity-conflict checks. Older 1.5.0 builds cannot read schema 2.
- Personal backup recovery creates a validated `practice-journal-linked.json` for newly recovered take IDs, retaining the verified original journal and links to excluded takes. Import remains explicit; conflicting existing history is preserved.
- Fix a sanitizer-proven use-after-free when scene status compared a live pedalboard during a worker rig replacement. Status and host setup now share the existing metadata commit lock; the audio callback takes no new lock. A regression runs concurrent polling through 64 complete recalls.

History and progress stay local, outside the audio callback, recordings, reamps and DAW state. No accounts, telemetry or subscription dependency are introduced. Public builds retain the original starter collection; private third-party sound files remain excluded.

Automated validation and actual release evidence are recorded separately. Fresh-PC installation, actual DAW/MIDI acceptance, sustained playing, export/video sync and sound redistribution rights remain pending where no evidence has been supplied. Signing remains deferred; this is an unsigned preview.

The first 1.6.0 full memory run caught the race above in the complete-starter-recall phase after ordinary tests had passed. Its stack identified a `ValueTree` child freed by `replaceState` while scene edited-state traversal was reading it. This is consistent with the earlier intermittent failure phase documented for 1.5.0, but does not prove the earlier crash had exactly that cause. Release validation must pass again after the fix; the earlier ordinary passes do not override the memory finding.

Local post-fix validation: **243 UI tests in 26 files**, all **five native CTest entries** (55.41 seconds, including optional owner-private capture recipes), the **full standard-Windows AddressSanitizer suite**, three independent level-10 VST3 runs (seeds 10002–10004), version-contract checks, and isolated installer/app/shortcut/VST3/upgrade/uninstall/user-data checks passed. The native suite covers journal migration, duplicate/cap/conflict handling, linked selective-backup recovery into a separate library, original-history/audio preservation and concurrent recall polling. Private fixtures are test inputs only; they are not public release assets. Automated isolated transfer/installation checks do not count as a real second-PC or beginner trial.
