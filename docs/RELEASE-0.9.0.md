# Cassian 0.9.0 preview

Performance → MIDI now offers separate **Toggle distortion/plate/spring 1 and 2**, **Distortion 1/2 drive**, and **Plate/Spring 1/2 blend** assignments. Switches accept CC or PC; expression uses CC, spans 0–100% and supports reversed direction. Add the instance in Board, apply the assignment, optionally Learn, then enable mapping and your controller input (or route MIDI from the DAW).

Targets use the existing numbered automation slots, preserving identity during reorder. Missing/deleted slots report an error instead of changing hidden controls. Loading or unresolved rig assets block requests. Expression leaves bypassed pedals off. Mappings persist with native app/DAW sessions and survive rig changes; rigs and portable packs continue to exclude controller bindings. Host IDs, rig/scene schemas and MIDI configuration version are unchanged. Older apps do not understand the new action names and disable configurations containing them on downgrade.

This milestone includes [0.8.1 scene renaming](RELEASE-0.8.1.md) and [0.8.2 saved-scene copying](RELEASE-0.8.2.md), both without recalling or changing the live tone. The starter catalog remains 22 complete file-free rigs plus 20 exact-capture recipes. Private captures remain local pending redistribution grants.

## Validation

Coverage includes all twelve new actions, CC press/release and held-switch behavior, repeated PC switches, independent expression endpoints/midpoint/inversion, bypass preservation, unrelated controls, reordered targets, deleted/absent targets, scene topology recall, alternate-rate native session restore, complete-rig mapping persistence and unresolved assets. UI checks cover target choices, CC-only expressions, reverse direction and PC switches. All 179 UI tests and all four native suites passed on Windows on 2026-10-07. Release standalone/VST3 and the embedded frontend built; the standalone binary and VST3 module identify 0.9.0. Release-version checks passed. The optional relocated bank validated all 103 assets and 20 exact recipes.

The isolated installer passed install, upgrade/current-version registration, app/shortcut/optional-VST3 placement, private bank hashes, uninstall and user-data preservation. Packaging emits root Cassian.exe, setup/portable aliases, versioned 0.9.0 artifacts, checksums and clean-checkout metadata. Package verification checks executable identity, alias/metadata hashes, root ZIP layout, notices and separation of public packages from all 103 private assets.

Physical controllers, live guitar/interface use, DAW host routing, Linux MIDI and fresh-PC installation remain separate validation work. Synthetic MIDI injection does not establish hardware latency. Signing and public third-party sound permissions remain release gates.
