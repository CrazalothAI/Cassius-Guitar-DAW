# Cabinet comparison and alignment

Scope for 1.8: make the existing parallel cabinet path easier to compare, document its timing contract, and strengthen deterministic regression coverage. Keep its response, parameter identities and saved state unchanged. Real clean/high-gain listening remains an owner acceptance task; synthetic impulse checks do not establish a better tone.

## Using the cabinet page

Choose **Tone → Cab**, load A and B, enable B, and use External IR mode or a compatible Auto route. **A only**, **50/50 blend** and **B only** set the existing B blend parameter to 0, 50 and 100 percent. They do not load/enable a missing cabinet or turn up Master. The blend remains an ordinary tone setting; save your preferred setting in the rig. These are not hidden listening-only solos: changed cabinets affect processed recordings and reamps. DI and Play Along remain independent.

The route summary distinguishes retained files from active responses. Built-in speaker, Off, Natural DI Auto and full-rig Auto do not use the external blend. An explicit External IR override can still stack a cabinet on a full-rig capture, so compare it carefully. With one active IR, blend does not change its level. Shortcuts are disabled during complete rig loading, recording and take playback.

If a blend is hollow, compare each cabinet alone at a comfortable fixed Master setting, then the blend. Similar responses with opposite polarity can cancel. Different relative delays can create frequency-dependent cancellation; neither inversion nor a delay is automatically the right sound. The page reports relative delay and polarity without claiming that different files are phase matched. **Reset alignment & polarity** clears only the two manual delays and polarity switches. It preserves the blend, A/B levels, pans, cuts, amp and pedal settings.

The existing IR loader uses the pinned JUCE convolution with stereo processing, normalization and leading/trailing silence trimming enabled. Leading silence is trimmed independently for A and B; file timestamps are not microphone distance measurements. Manual alignment adds a causal 0–10 ms delay after each convolver, using linear fractional interpolation. Equal delays preserve the blend but add delay to its output; differential delays can change its sound. These user-selected tone delays are not a change to host latency compensation. There is no automatic correlation, peak alignment or polarity selection.

## Regression contract

The dedicated `cabinet_alignment` CTest measures synthetic two-tap stereo responses recorded at 48 kHz, rendered at 44.1, 48 and 96 kHz, in mono/stereo with 64, 128 and 257 sample callback partitions. It checks audible finite reference output, partition invariance, equal-blend level preservation, the established trimmed-onset behavior, opposite-polarity cancellation, and fractional common/relative delay against the mathematical delayed single-cab response. Existing sound/state tests cover B-only fallback, pan, complete recall, older defaults and portable cabinet assets. A focused reproduction is `CassianTests.exe <NAM-fixture-path> --cabinet`.

No new automation ID, rig/scene schema, capture import format or migration is introduced. Existing rigs retain their sound. No third-party IR is added to public downloads; owner bank fixtures remain private. Before claiming tonal improvement, compare real clean/high-gain phrases, level-match deliberately, and log interface, rate, buffer, cabinets and routing. Timing benchmark results are diagnostic; they are not a hardware-independent CPU guarantee.
