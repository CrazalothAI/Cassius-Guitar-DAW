# Organize your saved rigs

Open **Library → Presets**, optionally filter Rig type to **Saved rigs**, and select a rig's name. Its detail panel offers:

- **Saved rig name**: rename the entry without resaving its audio settings.
- **Rig styles**, **Rig gain** and **Rig tags**: categorize tones for the existing style/gain/search filters. Styles accept comma-, semicolon- or space-separated words and normalize to lowercase without duplicates.
- **Rig notes**: keep pickup, playing or song reminders. Notes also participate in search.
- **Save rig metadata**: persist these fields. Renaming the active rig updates its displayed name while keeping its comparison baseline and unsaved tone edits.
- **Duplicate saved rig**: create a separately named entry with a new identity, copied metadata and the exact saved snapshot. Its favorite starts off. The copy does not activate automatically and does not include unsaved playing edits. Select the copy, press Use, edit it, then Save to create a variation.

Names permit 1–80 characters. Tags, styles and notes each permit up to 1,000. Editing and duplication wait for rig loading to finish. Invalid metadata or persistence failures leave the original entry and active identity intact; a failed copy is removed. Metadata cannot overwrite the stored state, ID or schema.

Native metadata and copies live in the shared user library and survive reopening Cassian. The rig's existing asset references remain unchanged; duplication does not copy sound files or clear missing assets. Saved-rig metadata also survives native session storage. Current-rig JSON/pack exports do not include these library-entry annotations. Factory starters remain immutable; save your own version first.

Browser preview implements these controls using local control snapshots. It does not provide native audio or a complete serial board. Saving subsequent preview tone edits retains your metadata.

The 0.4.2 update also fixes a Takes initialization race that could reset a quickly selected reamp version before export/recovery. Selection-dependent form resets now finish before display.
