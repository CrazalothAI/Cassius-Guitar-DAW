# Cassian 0.7.1 preview

Board adds an original spring-style reverb: four feedback rings, each with six short delayed allpasses, produce a dispersive return texture. It is distinct from Plate and Room/Chamber/Hall and requires no capture or impulse response. This is an original digital effect, not a measured physical tank or branded emulation.

Decay controls nominal feedback time (0.3–6 seconds); Tone damps the return; Drip changes allpass dispersion; Pre-delay (0–100 ms) separates the wet onset; Blend combines dry and wet. Two instances own separate storage and host controls. Addition defaults to after-cabinet placement; pre-amp placement is also supported.

Storage is allocated in prepare. Processing uses bounded feedback, fixed arrays, denormal protection and smoothed controls, with no callback allocation or file I/O. Zero blend preserves exact dry stereo timing while the tank drains. Bypass uses the existing board fade/tail-draining path.

Spring host controls append after Plate. Older rigs/scenes default them only if no spring block, including deleted reservations, exists; spring-containing state requires complete controls. Existing schemas and host IDs remain unchanged. Older apps cannot play an unknown spring block.

## Validation

Native checks cover 44.1/48/96 kHz impulses, callback partitioning, stereo output, pre-delay timing, decay extension, tone/dispersion changes, exact dry blend, appended host order, old rig/scene migration, independent instances, rejected incomplete state, alternate-rate session restore and portable packs. UI checks cover independent Drip controls and after-cabinet addition. All 172 UI tests and all four native suites passed on Windows on 2026-10-07 with the optional 103-file bank; Release tests/VST3 and the embedded frontend built. Final standalone/installer packaging will follow in 0.8.0; real guitar/interface/DAW audition remains required.
