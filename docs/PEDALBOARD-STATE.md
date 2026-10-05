# Pedalboard state foundation

Implementation checkpoint: 2026-10-05. This milestone establishes a versioned description of the existing effects, their stable identities, and their automation bindings. It does not change their audio order or enable additional effect instances. The full serial editor and audio runtime remain a subsequent milestone.

## Supported document

New external rig documents use integer `schema: 2` with an `AmpSuiteState` XML string. All 87 current parameter rows and one supported pedalboard are required. Schema 1 remains accepted and receives the established defaults for missing controls after the first 41. Missing board metadata migrates only for legacy documents; a present invalid board never falls back to the old chain. Fractional/wrapping schema numbers and unknown schema-2 parameters reject.

New saved library entries also retain `schema=2`; entries without a schema property are legacy. Native sessions retain their binary XML wrapper and sparse legacy parameter handling. Scene banks write JSON version 2 with each occupied slot's validated board XML; version-1 banks inherit the shared rig identities during migration.

The `AmpSuiteState` XML document can contain one `PEDALBOARD` child. Its only properties are `version=1` and `runtime="legacy-fixed-v1"`. It contains exactly the following eight ordered `BLOCK` children:

| Type | Initial identity | Fixed anchor | Existing parameter binding |
| --- | --- | --- | --- |
| `compressor` | `legacy.compressor` | `compressor-mode` | `COMP_MODE`, `COMP_*`, `CLEAN_COMP` |
| `overdrive` | `legacy.overdrive` | `pre-amp` | `OD_*` |
| `neural-pedal` | `legacy.neural-pedal` | `amp-pedal` | `PEDAL_ON`, `PEDAL_INPUT`, `PEDAL_OUTPUT` |
| `eq` | `legacy.eq` | `post-eq` | `EQ_*` |
| `modulation` | `legacy.modulation` | `post-modulation` | `MOD_*` |
| `chorus` | `legacy.chorus` | `post-chorus` | `CHORUS_*` |
| `delay` | `legacy.delay` | `post-delay` | `DELAY_*` |
| `reverb` | `legacy.reverb` | `post-reverb` | `REVERB_*` |

Every block has exactly `id`, `type`, `automationSlot`, `anchor`, and `trimDb`. The neural pedal additionally requires `assetKey="pedal"`, referring to the existing top-level `pedalId`/`pedalPath` reference. `automationSlot` is integer zero; it is qualified by block type and binds that block to its existing parameters. `trimDb` is finite zero; a separate output trim is not implemented. IDs contain 1–64 ASCII letters, digits, periods, underscores, or hyphens and must be unique within the board.

The APVTS parameter rows remain the sole source of effect settings and bypass. Blocks do not duplicate parameter values or bypass properties. For chorus, delay and reverb, the existing mix controls determine their contribution; the board does not invent new switches. Existing parameter IDs, ranges and host indices remain unchanged.

Missing board state means this fixed legacy board. Migration adds it once with deterministic identities, leaving parameters and asset paths intact. Already valid identities are preserved, including identities different from the deterministic defaults. Equality compares the effective board, so an old document without a board equals its default migration. XML reads turn numeric attributes into strings; validation accepts the exact serialized integer spellings `1` and `0`, while rejecting booleans, fractional values and integer overflow.

## Audio compatibility

The anchors describe compatibility bindings, not a freely reorderable signal chain. The compressor's mode remains significant: mode 0 is compression inside the Lumen/legacy clean algorithm, mode 1 processes mono input before the amp, mode 2 processes the output after the cabinet, and mode 3 turns the compressor off. Migration does not convert the embedded clean compressor into a pre or post pedal.

The built-in overdrive precedes the selected amp algorithm. The neural pedal remains inside the amp processing path after its existing drive/tight processing. Legacy clean routing retains its original neural-pedal bypass behavior; explicit amp selection retains its current pedal behavior. EQ remains after amp output, tone controls, high cut and the second gate-envelope pass. Modulation, chorus, delay and reverb retain their original order and effect histories.

The amp, cabinet, input gain, hum removal, gate, resonance, parallel sub layer, amp tone controls and stereo micro-delay retain their existing processing. Cabinet A/B can generate stereo after the mono amp path. Current NAM processing accepts mono input and mono output, so this foundation does not permit placing a neural pedal after stereo cabinets or effects. A future implementation must explicitly support or reject channel conversions.

Recording and offline reamping use the existing guitar path. External backing tracks, metronome, listening-only Guitar balance/Mix focus, Master and final output protection remain outside this board description.

## Validation and recall

Board validation rejects unsupported versions/runtimes, duplicate board children, missing or extra blocks, reordered types, duplicate or invalid identities, unknown properties, nested children, changed anchors, nonzero slots, nonzero or invalid trim, and additional asset references. The serialized board is limited to 16 KiB. Unsupported state is rejected before recall changes the active tone, assets, scene bank or saved-rig identity; it is never silently interpreted as a different audio graph.

The shared state helper is used at rig and session boundaries. Stored scenes, complete rigs, A/B, take snapshots and schema-1 packs retain this description through their surrounding state machinery. A missing description migrates on read; existing take files do not need rewriting. Schema-1 packs still reference only the existing amp, pedal and cabinet A/B assets. This milestone does not expand their asset limits or authorize redistribution of captures.

Pack import and export use the same isolated validated/migrated tree as rig recall. New packs contain a schema-2 document and retain the four-asset bound; an unsupported board rejects before extraction or destination replacement. Original take documents remain byte-identical when used for a reamp. A reamp keeps its supplied reference snapshot alongside the newly rendered audio rather than rewriting the original.

## Future serial runtime and automation

The intended serial board budget is at most 16 effect blocks across pre/post lanes, with one amp and the existing cabinet stage. The current runtime still contains eight singleton effect bindings and at most one active neural pedal plus the amp capture; no claim is made about additional NAM instances or CPU headroom.

Persistent block identity must be separate from order and from automation slot. A later serial runtime will assign stable, kind-qualified slots with an explicit namespace, such as `BOARD_EQ_01_FOCUS`; this is a design example, not an exposed parameter today. Reordering a block must preserve its automation binding. Duplicating it must allocate a new identity and slot. Deleting, replacing or reusing slots must have an explicit policy so old host automation cannot silently begin controlling another effect. The existing legacy parameter IDs remain supported through the slot-zero bindings.

Before enabling the editor, implement independent DSP instances, validated asset traversal and pack deduplication, bounded buffers and channel rules, worker-thread graph preparation, safe transitions and undo/redo. Then verify audible order changes, stereo preservation, automation recall, old state migration and measured performance with real captures.

`tests/PedalboardStateTests.cpp` verifies deterministic/idempotent migration, preservation of existing tone data and block identities, XML round trips, effective legacy equality, and rejection without document mutation. Integration tests cover the surrounding rig/session/scene/pack/take paths.

## Verification

The Windows Release build and all four native CTest entries pass. The 113 existing UI tests pass; this milestone adds no UI or audio-routing control. Focused board checks cover malformed/future/duplicate state, fractional/wrapping schemas, incomplete schema-2 documents, sparse legacy host states, first-41-control rigs, saved identities/baselines, scene v1/v2, old/new packs and rejected destination replacement. Old take snapshots reamp without rewriting `Original rig.json` or original WAVs.

Audio comparisons are sample-identical between legacy and migrated documents: all five amp source choices with all four compressor modes at 48 kHz, plus Lumen/post compression at 44.1 and 96 kHz. Built-in overdrive, a fixture neural pedal, EQ, modulation, chorus, delay, reverb and micro-delay run during the comparisons. Existing tests continue to cover dual cabinets, uninterrupted scene delay history, Play Along recording isolation, MIDI and protected output. The fixtures are synthetic inputs/example NAMs; this does not establish live guitar listening quality or performance of a future multi-instance board.

Release standalone/VST3 and the local Windows app/setup/portable packages are rebuilt for this source milestone. Packaging does not publish or merge it. Fresh-PC installation, Linux, real DAW hosts, sustained interface use, additional NAM block budgets and a curated redistributable sound library remain unverified.
