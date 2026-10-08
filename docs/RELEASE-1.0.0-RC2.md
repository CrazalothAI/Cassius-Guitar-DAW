# Cassian 1.0.0 RC2

The next 1.0 milestone strengthens release reliability. Signing remains deferred; RC2 is an unsigned release candidate. The guitar processing algorithms are unchanged.

- Add pinned Tracktion pluginval 1.0.4 and three reproducible level-10 runs of the actual VST3 at 44.1/48/96 kHz and 64–1024-sample buffers. Test loading, processing, state restore, automation, buses, parameter thread safety and fuzzing.
- Isolate each host-test process in scratch library/practice/take storage and skip installed sound-bank import. Normal user storage remains unchanged. Invalid relative scratch paths reject.
- Preserve reports/logs in CI even on failure and reject changed tools, changed plugins, missing tests, nonzero exits and stale success reports.
- Require current independent validation before Release packaging; record tested compiled-plugin hash and compare it with unsigned delivered plugin content.
- Record the owner's signing deferral and the pending actual-use acceptance matrix. [Validation](PLUGIN-VALIDATION.md), [manual matrix](RELEASE-ACCEPTANCE-MATRIX.md).

## Verification on Windows, 2026-10-08

187 UI tests and all four native suites passed, including the local 103-file bank. The rebuilt standalone/VST3 and binary-version checks passed. The rebuilt VST3 passed all three level-10 seeds (10002/10003/10004). Native tests exercised default-constructor scratch rig saving, installed-bank exclusion, ordinary storage resolution and invalid-path rejection. Negative checks rejected a modified validator and replaced a stale success report after a missing-plugin failure. Isolated installer install/upgrade/uninstall passed and preserved test user data.

Final package integrity is checked after packaging the committed revision. The numeric Windows app version remains 1.0.0; the editor reports RC2 separately. Private captures are excluded from public downloads.

GUI/editor interaction and the separate Steinberg SDK validator were skipped; these are not passes. Actual DAW sessions, physical MIDI, real camera synchronization, fresh-PC prerequisites, real upgrades and signing remain pending. This update does not certify all hosts or live hardware configurations.
