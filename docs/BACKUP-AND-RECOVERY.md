# Personal backup and recovery

Implemented in the 1.1.0 preview. Open **Library → Backup & recovery**.

## Create a backup

Finish recording, reamping, exporting and library edits, then choose **Create backup**. Save the `.cassian-backup.zip` on another drive if you want protection against failure of the computer's main drive. Keep multiple dated copies.

The archive includes saved complete rigs and their metadata, ordered boards and stable automation slots, performance scenes, a copy of the current tone, managed and externally referenced NAM/WAV sounds, cataloged take folders (original dry/processed guitar and backing stems), reamp WAVs and rig snapshots, take names/notes/favorites, and saved practice/review section documents. Sounds are deduplicated by content. Local references are rewritten into archive-relative paths, including embedded rig documents.

The worker writes audio in bounded chunks, then reads the resulting archive back and verifies every SHA-256 checksum. It checks source files/catalogs again before atomically replacing the destination backup. Missing sounds/audio, corrupt metadata, disk errors, changes during copying and cancellation fail the operation; an existing destination backup is retained. Editing another instance's library during a backup may make that backup fail, rather than produce a misleading partial copy.

This first format uses a stored streaming ZIP below **2 GiB**, with at most 8,192 payload files and 2,048 takes. It deliberately rejects larger libraries instead of truncating classic ZIP sizes. Verification temporarily needs room for another expanded copy. ZIP64, selectable take subsets, scheduled snapshots and automatic retention are later work.

DAW project/session files, host automation, standalone device preferences, MIDI port/assignment configuration, external video projects and external backing tracks that have not been recorded as take stems are outside this archive. Save those separately. Practice section identities survive; reload the same original backing audio to use them. This is an explicit saved backup, not crash autosave.

## Restore copies

Choose **Restore backup**, read the copy-restore explanation, then select the archive. All files are validated/extracted in scratch storage before catalogs are changed. Unsafe paths, duplicate/case-colliding names, symbolic links, unknown entries/formats, oversized expansion, damaged checksums and invalid rig documents are rejected.

Recovered media lives beneath `Cassian/Library/Recovered/<unique-folder>`. Rig/take paths and original/reamp snapshots point into that copy. Saved rigs and takes receive new identities and `(recovered)` names, retaining metadata. The existing library, take recordings and currently playing tone are preserved. Open Library Presets or Takes to use the new entries. Repeating a restore intentionally adds another set of copies.

Existing sound metadata wins when an asset ID matches; a missing catalog path can gain the recovered path. An existing altered managed asset is not silently overwritten—relink/repair that asset separately if loading reports changed managed content. Existing practice/review sections take priority. New section files are added under their normal interprocess locks; any addition failures are reported and the verified copies remain available in the recovered folder.

Library and take catalog commits use the same interprocess locks as normal saves and atomic replacement per file. If the second commit fails, the first is rolled back; if rollback itself fails, verified media is retained and its location is reported. The recovered folder also contains before-restore catalog journals for manual recovery if the process or power fails between commits. This is not a claim of a multi-file, power-failure-atomic transaction.

Use **Show saved files** to locate the finished archive or recovery folder. Keep the `Recovered` media while its imported entries are in use; deleting it breaks their references. No backup is uploaded. Private/licensed sounds remain personal assets; making a backup does not grant redistribution permission or add them to public releases.

## Verification

Native tests exercise external sound/take relocation, byte-identical audio, reamp snapshots, populated/null scenes, notes/favorites, colliding saved IDs, preserved existing sections, cancellation before/during copying, missing sources, damaged/traversal/case-altered archives, worker completion and unchanged active parameters/rig identity. UI tests cover preview restrictions, restore explanation, progress, cancellation, errors, duplicate requests, output reveal and library refresh.

Windows file-path failures are reported without committing partial recovery. This preview still needs user backup/restore trials on a second PC and large-library storage/performance tests; automated scratch tests do not establish those results.
