# Cassian 0.6.1 preview

Board now includes an original distortion pedal with Hard, Asymmetric and Fuzz modes. Drive sets clipping depth; Tone controls the wet high-frequency rolloff; Low cut removes bass before clipping; Blend combines the drive and dry signal; output trim balances the pedal. It processes mono guitar before the amp with 4x oversampling, DC removal and smoothed controls. It does not require third-party captures. This is an original effect, not an emulation of a named commercial pedal.

Two independent instances support duplicate, replacement, reorder, bypass and existing Undo/Redo. New kind-qualified parameters append after all existing host controls. Old rigs/scenes default this family only when no distortion block, including reserved/deleted blocks, exists. Documents containing distortion must provide all its controls. Existing room/chamber/hall, overdrive and amp algorithms are unchanged.

Oversampling adds less than eight base-rate samples in current DSP checks. This update does not add host latency compensation or claim gapless graph switching. Zero blend and settled block bypass preserve dry audio. Partial blend uses the oversampled path; its small filter latency remains audible-path latency.

## Validation

All 169 UI tests passed on Windows on 2026-10-07. Native checks cover 44.1/48/96 kHz, partition independence, finite clipping, DC removal, distinct modes/drive/bass control, appended host order, old scene/rig migration, duplicates, rejected post-cab placement, complete state, session recall and portable packs. All four native suites passed with the optional 103-file bank; Release tests/VST3 and the embedded frontend built successfully. Live guitar and actual DAW automation remain manual checks. The 0.7.0 milestone rebuilds standalone and installer packages.
