# Organize your saved rigs

Open **Library → Presets**, optionally filter Rig type to **Saved rigs**, and select a rig's name. Its detail panel offers:

In 0.5.0, saved rigs also appear in the header's **Your saved rigs** group for direct complete recall and Revert. When a complete rig is selected, header arrows step through saved and available starter rigs.

- **Saved rig name**: rename the entry without resaving its audio settings.
- **Rig styles**, **Rig gain** and **Rig tags**: categorize tones for the existing style/gain/search filters. Styles accept comma-, semicolon- or space-separated words and normalize to lowercase without duplicates.
- **Rig notes**: keep pickup, playing or song reminders. Notes also participate in search.
- **Save rig metadata**: persist these fields. Renaming the active rig updates its displayed name while keeping its comparison baseline and unsaved tone edits.
- **Duplicate saved rig**: create a separately named entry with a new identity, copied metadata and the exact saved snapshot. Its favorite starts off. The copy does not activate automatically and does not include unsaved playing edits. Select the copy, press Use, edit it, then Save to create a variation.

Names permit 1–80 characters. Tags, styles and notes each permit up to 1,000. Editing and duplication wait for rig loading to finish. Invalid metadata or persistence failures leave the original entry and active identity intact; a failed copy is removed. Metadata cannot overwrite the stored state, ID or schema.

Native metadata and copies live in the shared user library and survive reopening Cassian. The rig's existing asset references remain unchanged; duplication does not copy sound files or clear missing assets. Saved-rig metadata also survives native session storage. Current-rig JSON/pack exports do not include these library-entry annotations. Factory starters remain immutable; save your own version first.

The native detail panel now lists referenced sounds and their availability. A missing file disables Use for the selected rig; Relink is available for managed references and verifies original content through the existing picker. Import the original pack when no managed relink target exists. Inspection does not load the tone or validate a file's contents; loading still performs full asset checks. Missing sound references are retained rather than replaced with another amp/cabinet.

Browser preview implements these controls using local control snapshots. It does not provide native audio or a complete serial board. Saving subsequent preview tone edits retains your metadata.

The 0.4.2 update also fixes a Takes initialization race that could reset a quickly selected reamp version before export/recovery. Selection-dependent form resets now finish before display.
