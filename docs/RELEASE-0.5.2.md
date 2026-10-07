# Cassian 0.5.2 preview

Takes now shows the selected version's waveform after Listen finishes loading it. Click the waveform to seek, use arrow keys for one-second steps (Shift for ten seconds), or Home/End to reach its boundaries. The cursor and A/B region follow the existing review controls. Original processed, Dry DI and reamps use their own decoded audio.

Waveforms reuse the existing worker-prepared stereo min/max envelope, bounded to 512 pairs. Status polls do not scan audio or resend the envelope. Fetching validates the loaded take/version identity; selecting another version, pending loading and Stop hide stale waveforms and disable seeking. Failed fetches report an error while the ordinary position slider remains available. Practice keeps its existing waveform controls.

This does not add destructive editing or change recordings, export settings, host parameters or sound files. Review decoding retains its existing 256 MiB bound.

## Validation

Validated on Windows on 2026-10-07: 156 UI tests and all four native suites passed with the optional 103-file bank; the native Release test build also rebuilt the production editor. Coverage checks waveform content, identity/revision, stopped and mismatched requests, pointer/keyboard seeking, no repeated fetches on position polls, hidden stale data and usable slider fallback. Final standalone/VST3 packaging is handled by the subsequent 0.5.4 patch.

Live interface audition, real DAW hosts, Linux and fresh-PC setup remain manual checks. Third-party sounds remain private until redistribution grants are recorded.
