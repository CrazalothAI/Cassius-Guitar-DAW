# Play Along and workspace navigation

Development milestone: 2026-10-05. Implements the first two items in NEXT-STEPS.md. Serial multi-instance pedalboards, capture sourcing and commercial qualification remain future work.

## Listening mix

Guitar balance and Mix focus append after the original 85 native automation indices. Both default to zero. Focus uses four broad bands: 120 Hz shelf −2 dB, 350 Hz bell −2 dB, 1.2 kHz bell +4 dB and 4.8 kHz shelf −1 dB at 100%; the band gains scale with Focus. Balance spans −12 to +12 dB. Existing tone controls retain their IDs and behavior.

`GuitarMix` uses bounded, prepared scratch storage, 30 ms gain/band ramps and a 20 ms EQ bypass blend. Neutral EQ is bypassed exactly, and gain preparation starts at the configured value. During playback it computes a difference from the original processed guitar. The practice recorder captures the original guitar first, then the difference is added before backing/clicks. Offline reamping processes only the original guitar chain. Review clears the live guitar before playing stored audio. No callback allocation, I/O or mutex acquisition is added by this filter.

Input calibration, Master, click and Play Along controls are global listening/session settings. Starting points, complete-rig recall, A/B and scenes preserve them; native session restoration retains saved values. They are excluded from active tone edit detection. The original rig snapshot can retain global settings for inspection; the exported audio excludes Play Along and Master.

`outputPeakWarning` describes a held pre-limiter mix peak threshold, after Master: at least −0.5 dBFS within the last second of processed audio. It is not a measurement of limiter gain reduction, nor a measurement of external browser audio. Bring guitar forward sets +3 dB/45% consistently; repeated presses do not accumulate gain. Neutral mix resets only those controls.

## Navigation and rig identity

Tone, Board, Practice and Takes share one selected destination. Tone retains the full head with Amp/Cab tabs. Other views use a compact Drive/Master strip. Board exposes the six existing stages and scene controls; it remains a fixed chain. Mix and Performance open focus-trapped dialogs, and standalone Audio settings remains in the footer.

The persistent rig bar distinguishes saved complete rigs from header control starting points. Save As adds an entry, Save updates its ID, and tone/asset/scene edits mark it Edited. Save failures keep the previous identity; Save As errors remain visible inside the dialog for retry. Successful rig activation restores identity only after preparation commits. Native sessions and A/B include a bounded comparison baseline. An A/B snapshot older than an in-place Save compares against the currently saved library state and correctly shows Edited. Deleted rig entries require Save As.

Take review can be stopped from any destination, and an active recording links back to Practice. Utility dialogs preserve the destination and return keyboard focus to their trigger. Primary and stage tabs support arrows, Home and End. The minimum-size editor confines overflowing detailed controls to their panels.

## Verification

- 113 UI tests pass, including native command/error mocks, navigation exclusivity and keyboard behavior, compact/full amp switching, Save/Save As, edited identity, comparison/global preservation, and dialog focus/retry behavior.
- All four native CTest entries pass. Focused tests exercise mono/stereo at 44.1, 48 and 96 kHz, sample-exact neutral delta, gain/polarity, measured note-band lift, and control smoothing.
- Actual recorded dry WAVs retain the original input. Processed WAVs are sample-identical between neutral and changing listening controls. Actual offline reamps are identical with maximum listening changes; take-export tests also use maximum settings and preserve original files.
- Auxiliary backing/click comparisons are sample-identical; the existing integrated backing-player test now runs with maximum listening controls. Peak-warning tests cover silence, a threshold crossing and expiry.
- Native tests verify Save As/in-place Save, recall, failed recall/persistence, deleted rigs, scene edits, session identity, and A/B snapshots preceding a later Save.
- Production browser inspection at 860 × 620 checks every destination, utilities and preview saving. No document overflow; Practice and Takes use substantially more panel space than the previous full-head layout.
- Release standalone/VST3 builds succeed. The isolated Windows installer checks pass for app/shortcut/optional VST3 installation, upgrade, uninstall and preservation of test user data. This uses an existing WebView2 runtime, not a fresh-PC missing-runtime scenario.

Automated renders do not establish the owner's live guitar/YouTube balance, extended interface stability, DAW-host automation/recall, Linux compatibility, or fresh-PC WebView installation. Those remain the real-use/release checks in NEXT-STEPS.md. Root development packages are rebuilt locally; this milestone does not publish or merge them.
