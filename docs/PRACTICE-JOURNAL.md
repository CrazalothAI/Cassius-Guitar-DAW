# Practice sets and session history

Available in the **1.5.0 Preview standalone app**, under **Practice → Practice sets & history**. DAW instances use their host's session tools and do not open a journal.

Save a named set containing up to eight independent exercises. Each exercise has a name, a time target of 1–120 whole minutes and a BPM target of 40–240. Clean touch/dynamics, rhythm precision and lead articulation can live in the same set. Select an exercise and choose **Start practice timer**. Unsaved exercise edits can also start a session; the session keeps its own snapshot, so changing or deleting a set never rewrites past work.

Starting a timer leaves the playing rig, backing transport, loop, recording and metronome unchanged. **Use exercise tempo** explicitly changes the metronome BPM using its normal automation gesture, without turning the click on. That button and transfers are disabled during recording. Time/BPM targets are practice intentions, not detected performance or accuracy scores.

The native monotonic timer measures elapsed time until **Pause timer**; pause breaks and resume when ready. **Finish session** persists the time and your notes, up to 1,000 characters. Notes are saved on finish, not as you type. Search history by exercise, set name or notes. Finished-session totals do not include unfinished/interrupted entries. Deleting a set or completed session requires a second confirmation click. Export before removing history you want to retain.

## Storage and interruptions

The journal is `practice-journal.json` inside the normal Cassian library directory. It contains names, targets, dates, elapsed seconds, completion states and notes; it does not embed audio, rig documents, asset paths or account information. Nothing is uploaded, and Help's support-report whitelist excludes journal content.

A dedicated disk worker reads/writes the journal. The audio callback, guitar recording/reamp paths and DAW state never use it. Status polls return a small summary; the UI fetches full lists when their revision changes. Writes use validated temporary files and atomic replacement; failures keep the previous document and surface an error. A cross-process lease permits one writable journal per library. A second standalone window reports why it cannot write; close the other window and reopen to acquire it. An externally edited file is preserved instead of being overwritten by stale in-memory state.

An active timer checkpoints every 30 seconds. Normal shutdown preserves elapsed time but marks the session **interrupted**, rather than claiming it was finished. After an unexpected interruption, reopening keeps the last persisted checkpoint and marks any running/paused entries interrupted. Closed-app time is never added, and a timer does not resume automatically. Unsaved notes and time after the last successful checkpoint may be lost in an unexpected interruption. A forgotten timer pauses at six hours.

Capacity is 32 sets, eight exercises per set, 256 sessions and a 512 KiB document. Full history is not silently evicted; export and explicitly remove older entries when needed. Unsupported, malformed, oversized or deeply nested documents fail closed without resetting existing work.

## Moving and backing up history

**Export sets & history** writes a portable JSON copy chosen by the user. An active session is exported as interrupted, while the local timer continues. **Import sets & history** adds missing identities without replacing existing entries. Repeating the same import does not duplicate entries; a conflicting identity rejects the entire import. Finish the current timer before importing. Imported running/paused sessions become interrupted and never start playback or a timer.

All three personal backup scopes include an existing journal, independently of which recordings were selected. Pause the timer and let a pending save finish before creating/restoring a backup. Restore keeps the verified journal in the recovered folder and preserves the current journal. Choose **Show saved files**, then import that `practice-journal.json` from Practice to add its entries. A damaged optional journal is retained with a warning, without preventing recovery of rigs/audio.

Use 1.5.0 or newer to restore archives containing this new journal payload. Older personal archives remain readable by the current app.

This milestone does not link sessions to individual recordings, change backing tracks automatically, judge playing accuracy, draw progress charts or add cloud synchronization. Sustained playing, first-user setup and real second-PC trials remain acceptance work.
