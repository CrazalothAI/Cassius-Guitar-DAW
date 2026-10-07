# Cassian 0.5.0 preview

This update completes five workflow improvements.

1. **Saved rigs in the header.** The preset selector lists Your saved rigs alongside built-in starters and exact-capture recipes. Previous/Next steps through saved and available complete rigs when a complete rig is selected. Revert reloads the saved snapshot through native complete recall; input calibration and listening controls remain global.
2. **Saved-rig sound inspection.** Select a saved rig in Library → Presets to see its referenced amp, cabinets, captured pedals and ambience files. Missing references disable the selected entry's Use button. Relink opens the existing original-content picker when the reference is in the managed library; otherwise import its original pack. Inspection checks file availability without changing the playing tone. Actual format, hash and processing validation happen during loading.
3. **Named reamp versions.** Select a reamp in Takes, edit Reamp version name and press Rename version. The catalog label changes; the version identity, WAV path, snapshot path and file bytes remain intact. Original processed and Dry DI labels stay fixed. Labels persist across restarts and roll back if the worker cannot save the catalog.
4. **Take sorting.** Newest first, Name and Favorites first work alongside search/favorites filtering. Sorting leaves the selected take and version unchanged. Missing/invalid dates fall back deterministically to the entry ID; Favorites first orders favorites before recency.
5. **Quick guitar WAV export.** Export guitar WAV writes the full selected original/dry/reamp version as 48 kHz / 24-bit stereo WAV with 10 ms edge fades and existing −1 dBFS peak protection. It excludes backing and clicks, uses neutral export gains, retains any reamp tail and ignores the separate soundtrack trim/mix controls. The existing save picker chooses the destination. The Video soundtrack panel remains available for a trimmed backing mix.

No host parameters or rig schemas changed. This adds no new sound files or redistribution grants.

## Validation

Validated on Windows on 2026-10-06: 150 UI tests and all four native CTest suites passed, including the optional 103-file bank. Release standalone/VST3 builds, binary version checks and isolated installation/upgrade/uninstall passed. Installer checks include user-data preservation, version registration, shortcuts, optional VST3 placement and all private sound hashes.

Regressions cover saved-header recall/rejection/Revert and preview arrow navigation, global listening controls, dependency inspection without live mutation, stable-ID relocation/relinking, invalid/missing inspection states, persisted version labels, byte-identical audio/snapshots, invalid label rejection and catalog-write rollback. Take sorting preserves selection. Quick-export tests decode actual dry and processed WAVs at 44.1/48/96 kHz source rates, confirming 48 kHz stereo 24-bit output, full duration, neutral guitar levels, backing exclusion and boundary fades; the UI also verifies full extended-version export independent of soundtrack trim/gain controls.

Live interface audition, Linux, real DAW hosts, fresh-PC setup and camera alignment in Clipchamp remain manual checks. Binaries remain unsigned development previews, and third-party sounds remain private until distribution grants are recorded.
