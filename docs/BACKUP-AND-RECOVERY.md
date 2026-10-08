# Personal backup and recovery

Implemented in the 1.1.0 preview. Open **Library → Backup & recovery**.

## Create a backup

Finish recording, reamping, exporting and library edits, then choose **Create backup**. Save the `.cassian-backup.zip` on another drive if you want protection against failure of the computer's main drive. Keep multiple dated copies.

Since 1.1.2, **Include recorded takes and reamps in new backups** is on by default. Clear it for a **tone-library backup** if recordings are too large or already backed up elsewhere. That archive retains rigs, sounds, scenes and practice sections; it excludes recordings, reamps, take metadata and take review sections. It does not require excluded recording files. The archive limit applies to the remaining content too. The filename, manifest scope and completion message identify the choice. Restore adds the archive's actual rig/take copies; a tone-library archive contains zero takes.

Since 1.1.3, leave that option enabled and check **Choose specific takes** for a smaller archive. Select at least one recording, then choose **Create backup**. Each selected take includes its complete take folder: dry/processed audio, any backing stems, reamps, rig snapshots and metadata. Review sections are included only when their content key matches a WAV in those folders. All library rigs, sounds and practice sections stay included. This selects whole takes, not individual reamp versions or audio ranges. It does not remember a selection after reopening the library panel.

Use **Refresh take list** after importing or editing recordings. Missing or ambiguous selected identities are rejected instead of silently omitted; an empty selection cannot fall back to a complete backup. Missing files in unselected take folders do not block the selective archive. Missing library sounds still do. The filename, `selected-takes` manifest scope, included take count and completion message distinguish these archives. Restore uses the same additive workflow for all three scopes and preserves older schema-1 archives.

The archive includes saved complete rigs and their metadata, ordered boards and stable automation slots, performance scenes, a copy of the current tone, managed and externally referenced NAM/WAV sounds, cataloged take folders (original dry/processed guitar and backing stems), reamp WAVs and rig snapshots, take names/notes/favorites, and saved practice/review section documents. Sounds are deduplicated by content. Local references are rewritten into archive-relative paths, including embedded rig documents.

The worker writes audio in bounded chunks, then reads the resulting archive back and verifies every SHA-256 checksum. It checks source files/catalogs again before atomically replacing the destination backup. Missing sounds/audio, corrupt metadata, disk errors, changes during copying and cancellation fail the operation; an existing destination backup is retained. Editing another instance's library during a backup may make that backup fail, rather than produce a misleading partial copy.

Since **1.2.0**, all three scopes support archives up to **32 GiB**, with at most 8,192 payload files and 2,048 included takes. The payload budget reserves 32 MiB for metadata/ZIP overhead. Larger archives automatically use stored ZIP64 with 64-bit sizes and offsets; small archives retain classic ZIP below its original 2 GiB threshold. Older schema-1 archives remain readable. Use 1.2.0 or newer to restore large ZIP64 backups. Cassian accepts the stored, single-disk ZIP64 profile it creates; recompressed, encrypted or split ZIP64 archives are rejected. Keep the original archive intact.

Verification temporarily needs room for another expanded copy on the destination drive. Restore needs room for expanded media on the library drive. Hashing and copying use bounded buffers and support cancellation; very large libraries still require time and disk space. Divide recordings across selective archives if needed; library tones/sounds appear in each. Scheduled full archives are later work. The separate [1.1.1 automatic tone snapshots](RELEASE-1.1.1.md) reference existing files; recover important snapshot tones into saved presets before making a personal archive. Snapshot history/preferences are not archived. See [large archive validation](RELEASE-1.2.0.md).

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
