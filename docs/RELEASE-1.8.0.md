# Cassian 1.8.0 Preview — cabinet comparison

The Cab page now explains which responses are active and provides **A only**, **50/50 blend**, **B only**, and **Reset alignment & polarity**. Compare loaded parallel IRs without changing Input, Master, Play Along, cabinet levels/pans or cuts. Shortcuts are unavailable during rig loading, recording and take playback. Their ordinary tone edits affect processed recordings and reamps; they are not listening-only controls.

The summary identifies built-in, bypassed, full-rig Auto, Natural DI Auto, missing-IR and single/dual external routes. Relative manual delay and polarity are visible together; opposite polarity warns about potential cancellation without assuming different IRs are phase matched. Reset clears only manual delay and polarity. It does not silently choose a new cabinet or EQ curve.

The established cabinet DSP, normalization, trim, host automation IDs and rig/scene/session formats are unchanged. A dedicated native impulse contract covers mono/stereo, resampling, callback partition invariance, trimmed onsets, equal blend, cancellation and fractional relative/common delay. See [cabinet refinement](CABINET-REFINEMENT.md) for the method and a useful listening sequence. The four custom amplifier heads from [1.7](RELEASE-1.7.0.md) remain available.

Verification: 260 UI tests and six native CTest entries pass, including the dedicated cabinet impulse contract. Three pluginval strictness-10 runs pass; isolated app/shortcut/VST3 installation, upgrade, uninstall and user-data preservation checks pass. The bundled offline cabinet page is checked at 860×620, 1100×760 and 1440×1000. These checks do not establish actual host GUI, interface timing or fresh-PC acceptance.

This is an unsigned Preview. Real playing/host/installation acceptance and distribution review remain separate from automated evidence. [Path to 2.0](ROAD-TO-2.0.md).
