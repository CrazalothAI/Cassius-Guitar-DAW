# Cassian 0.4.2 preview

Saved rigs can now be renamed, categorized and duplicated from Library → Presets.

- Name, styles, gain, tags and notes feed the existing discovery filters without changing the saved tone.
- Active-rig renaming preserves the Edited indicator and current unsaved controls.
- Duplication copies the saved snapshot and metadata under an independent identity, preserving the original and current playing rig.
- Strict field validation and rollback protect metadata and copies when persistence fails.
- Browser preview retains metadata when saving later tone edits.
- Takes now resets selection-dependent forms before display, preventing initialization from overwriting a quickly chosen reamp version.

No host parameters or rig schema changed. See [organization workflow and limits](SAVED-RIG-ORGANIZATION.md).

## Validation

Validated on Windows on 2026-10-06: 143 UI tests and all four native CTest suites passed, including the optional 103-file bank. Release standalone/VST3 builds, binary version checks and isolated installation/upgrade/uninstall passed. Installer checks include user-data preservation, version registration, shortcuts, optional VST3 placement and every private sound hash.

Native organization checks cover normalized metadata, renamed active identity with unsaved edits, exact saved-snapshot duplication, independent identities/favorites, recall, app restart, native session restore, preservation of another instance's rig, invalid fields/names and persistence-failure rollback. UI checks cover native requests/error guards, searchable categories, browser duplication of saved settings, preserved unsaved playing controls and metadata retention on later Save. The existing selected-version soundtrack regression passes with the Takes initialization fix.

Live interface audition, Linux, real DAW hosts and fresh-PC installation remain manual checks. Binaries remain unsigned development previews; supplied private sounds are excluded from public downloads until redistribution grants are recorded.
