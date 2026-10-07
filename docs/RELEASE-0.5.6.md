# Cassian 0.5.6 preview

Takes now offers saved sections for processed recordings, dry DI and reamps. Set A-B, save a named phrase and recall it later. Recall pauses at A and enables looping. Sections persist across app restarts in managed take-section storage, separate from backing-track sections and rigs. Exact-content copies share sections; different files have separate identities.

Up to 32 sections and 48 characters per name use the existing validated atomic section store. Saves capture the selected range before queueing file work. Worker execution checks the loaded take/version and generation again, so obsolete commands cannot alter a newer review. No file writes occur in the audio callback. Existing take catalogs and host automation are unchanged.

## Validation

New native checks cover restart persistence, processed/dry separation, captured queue-time ranges, recall transport, stale queued save cancellation, deletion and unchanged recording hashes. UI checks cover exact take/version routing, selection reset and unavailable controls. All 165 UI tests and four native suites passed on Windows on 2026-10-07 with the optional 103-file bank. Release tests/VST3 and the embedded frontend built successfully. A startup guard waits for the first review callback before section edits. Final standalone/installer checks are part of 0.6.0. Real interface/DAW, Linux and fresh-PC checks remain manual.
