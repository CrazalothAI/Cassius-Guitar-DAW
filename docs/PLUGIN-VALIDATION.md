# Independent VST3 validation

RC2 validates the actual Windows VST3 with Tracktion pluginval 1.0.4, pinned to its official release archive SHA256. The cached executable must also match that archive. This is a development tool and is excluded from installer, portable and matching application-source downloads.

## Run it

After building Release VST3, in PowerShell 7:

```powershell
./scripts/test-vst3-plugin.ps1
```

The default runs level 10 with seeds 10002, 10003 and 10004 at 44.1/48/96 kHz and buffers 64/128/256/512/1024. It covers cold/warm instantiation, processing/reinitialization, state restoration, automation, bus layouts, parameter thread safety and parameter fuzzing. Source: [pluginval documentation](https://github.com/Tracktion/pluginval/tree/v1.0.4), [upstream CI guidance](https://github.com/Tracktion/pluginval/blob/v1.0.4/docs/Adding%20pluginval%20to%20CI.md).

`-Vst3` selects a different bundle. `-OutputDirectory`, `-Strictness` (5–10) and `-Seeds` permit focused reproduction. Each run has a unique log folder. `PLUGIN-VALIDATION.json` identifies the numeric app version, tested binary hash, matrix, seeds, completed tests and explicit limits. A new invocation writes running/failed status before accepting a pass, so a stale success report cannot survive a failed check. A nonzero exit, missing success marker, missing required tests or changed plugin binary rejects the run. There is a per-test inactivity timeout and an outer run timeout.

GitHub runs these checks after native suites and before packaging. Reports and logs are saved as a separate `Cassian-Plugin-Validation` artifact, including when validation fails. Failed validation prevents package publication. `package-windows.ps1 -Release` also requires three passing level-10 runs for the exact compiled VST3 hash. Build metadata records this evidence; unsigned package integrity compares the delivered VST3 directly with the tested hash. For later signed builds, validation identifies the compiled input and artifact checksums identify the delivered signed copy. Smoke/development packaging is not a release-validation claim.

## Isolated storage

The launcher supplies an absolute `CASSIAN_VALIDATION_ROOT` only to each child process. The default VST3 constructor then uses that scratch root for library, practice and takes, and does not import the installed sound bank. Normal app launches keep their existing storage locations. Invalid relative test roots reject rather than falling back to owner storage. The native suite verifies scratch rig saving and ordinary path restoration. This is a developer host-test facility, not a standalone portable-profile feature.

## Limits

GUI/editor tests are deliberately skipped. The standalone interface, actual DAW sessions, physical MIDI, recording against a real camera and sustained device timing are not tested by pluginval. The separate Steinberg SDK validator is not supplied; its skipped line in the log is expected and must not be described as a Steinberg validation pass. Owner capture boards are exercised separately by native fixtures, not by these empty-profile host runs. Windows pluginval does not provide proof that every callback is allocation-free. Keep the real-use acceptance matrix pending until those tests are performed.
