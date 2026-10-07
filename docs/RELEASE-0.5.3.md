# Cassian 0.5.3 preview

Sound-library browsing now includes File status: All sounds, Available or Missing files. Combine it with style, gain, source pack and favorites to find missing managed files or narrow the collection. Relinking refreshes the filtered results. Presets continue to use their existing dependency inspection and exact-recipe availability checks; this file filter applies to sound tabs only.

Amps also includes Capture type: Built-in amp, Direct input, Unknown capture, Amp-only, Preamp-only or Full rig. Recorded types use the existing catalog classification; unknown captures remain unknown rather than guessing from a friendly name. The selected amp explains full-rig cabinet handling and the preamp-only processing limit. Switching to another sound category ignores amp-type filters; Clear filters resets both new filters.

File status checks presence, not content validity or sound quality. Recorded types originate in capture metadata and import-time filename hints; check source notes before choosing cabinet routing. This patch changes discovery only, without altering owner metadata, sounds, routing or host parameters.

## Validation

Validated on Windows on 2026-10-07: 159 UI tests and all four native suites passed with the optional 103-file bank; the native Release test build rebuilt the production editor. UI checks combine file/type filters, relink refresh, built-in versus captured amp types, unknown/malformed metadata, Clear filters and tab isolation. Final standalone/VST3 packaging is handled by the subsequent 0.5.4 patch.

Live audition, real DAW hosts, Linux, fresh-PC setup and third-party distribution grants remain outstanding.
