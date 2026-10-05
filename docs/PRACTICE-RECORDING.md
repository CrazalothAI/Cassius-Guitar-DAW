# Practice player and paired guitar recording

The standalone app's **Practice** view provides a backing-track transport and recording below a compact amp strip. Tone retains the full head; Takes has a dedicated review/reamp view. The VST3 leaves practice playback and recording to the DAW. No tone parameter IDs, rig schema, or owner attribution markers change.

## Playback

- Import a local mono/stereo track using the audio formats registered in the native picker. WAV, AIFF, FLAC and Ogg are enabled in the current JUCE configuration; optional platform formats depend on build settings.
- Play/Pause/Stop, seek, independent backing level (−60 to +6 dB), and A/B section looping. Loop points must be at least 50 ms apart. Looping wraps inside callbacks rather than waiting for a UI timer. Loop edge fade offers Off/2/5/10/20 ms (default 5): raised-cosine fades at the end and start reduce abrupt seams without changing cycle length. The maximum fade is one quarter of the cycle. This creates a brief level dip rather than overlapping musical material. Off retains hard cuts; waveform-aware crossfade editing is future work.
- A worker decodes and uses JUCE's filtered resampler to prepare immutable stereo audio at the interface rate. The callback performs no file reading, decoding, allocation, ownership destruction, or mutex acquisition. Retired tracks are reclaimed on the worker using an audio-thread hazard pointer.
- Decoded audio is bounded to 256 MiB per track (about 11.7 minutes at 48 kHz, 5.8 at 96 kHz at 100%). Both decoded input and prepared output must fit this limit. At 50%, maximum source length halves. Stretch preparation can temporarily hold the old track, decoded input and stretched output (up to about 768 MiB plus processor scratch), with review retaining a separate buffer. Streaming longer tracks remains future work.
- Failed loads preserve the current track. Tracks are rebuilt following device preparation and never automatically resume. Transport state is not part of complete rigs or DAW project serialization.

## Waveform and named sections

The waveform uses up to 512 stereo min/max bins prepared on the track worker before stretching. Opposite-polarity stereo channels do not cancel in the display. It shows original-track seconds, the playhead and A/B region. Click to seek, or focus it and use Left/Right for one-second steps, Shift plus arrows for ten seconds, and Home/End. Seeking is disabled during loading, count-in and recording. The editor fetches envelope data once per successful track revision; normal status polling only moves the cursor.

Open **Sections** to save the current A/B loop with a name, replace a selected section, recall it or delete it. Recall pauses at A and enables the loop without starting playback. Deleting a section does not change the active loop. Up to 32 sections per track are supported, with names up to 48 characters and a minimum 50 ms loop. Loading, count-in and recording block section edits/recall.

Sections are keyed by the SHA-256 of the original file bytes and saved under the shared managed library's `practice` folder, separately from tone rigs and takes. Renamed identical copies share sections; a re-encoded or otherwise changed file has a separate bank. Speed and sample-rate preparation keep source-time points. Each disk mutation rereads the bank under an interprocess lock and atomically replaces it, preserving changes from other app instances. Invalid metadata reports an error, preserves its bytes, and does not prevent audio playback. When shared storage is unavailable, sections use a bounded session-only cache.

Failed or cancelled track imports preserve the previous waveform and bank. Waveform and section preparation add no audio-thread disk access or allocation. Playback still uses the existing bounded track buffer; this display does not add streaming support.

## Pitch-preserving speed

Speed offers 50/65/75/85/100/115/125/150%. Signalsmith Stretch prepares stereo audio on the disk worker after sample-rate conversion; 100% bypasses stretching. Changing speed pauses playback, preserves the cursor and A/B points, and reports preparation progress. Press Play to resume. **Cancel preparation** invalidates unfinished work and keeps the prior audio and speed. Replacing the track, changing device timing or closing the app also invalidates stale preparations.

The cursor and loop points remain in original-track seconds. At 50%, one second of playback advances the cursor by half a second. Count-in and metronome tempo retain the chosen BPM; set BPM for the accompaniment yourself. Speed and loop fades cannot change during count-in or recording. Guitar monitoring and recording remain at the interface's normal rate, with no additional guitar latency. Takes store backing speed in metadata, but exclude backing audio as before. Very short tracks that cannot supply the stretcher's analysis window are rejected for speed changes while previous audio remains available. Slowing complex mixes can soften transients or add spectral artifacts; synthetic pitch checks do not establish final listening quality.

## Count-in and recording

Choose Off, one bar or two bars. BPM and beats per bar are latched from the metronome when starting. The count-in precedes backing playback and guitar capture, with a sample-timed transition even inside a callback. Guitar monitoring remains active during the count-in. The ordinary metronome is suppressed during count-in; if enabled it resumes on the next host callback after count-in completion.

**Record guitar** asks for a parent folder and creates a new timestamped directory with a numeric suffix when needed. It never intentionally replaces a previous take. Recording also works without a backing track.

| File | Source | Format |
|---|---|---|
| Guitar dry.wav | Physical mono guitar input, before software Input gain and all guitar processing | 32-bit float mono WAV at the interface rate |
| Guitar processed.wav | Guitar after the amp, cabinets and effects, before backing/click addition and Master/limiter | 32-bit float stereo WAV at the interface rate |
| Original rig.json | Settings and asset references captured when recording is requested | Complete rig JSON; external assets are referenced rather than bundled |
| Cassian take.json | Name, creation time, frame count, rate and incomplete flag | Written after WAV finalization |

Files have the same start boundary and frame count. Effect and model latency is retained in the processed signal; this is not latency-compensated reamping. Processed samples above 0 dBFS are retained in float format rather than clipped. Import at a suitable playback gain. NaNs/infinities are written as silence. Backing tracks and metronome/count-in clicks are excluded from both files.

A preallocated 262,144-frame, three-channel single-producer/single-consumer FIFO sends audio to the disk worker. Both writers consume the same frame slices. **Finish take** pauses playback, stops capture, drains queued frames, and closes the writers to finalize WAV headers. **Stop** additionally rewinds. Finishing does not require another audio callback. The last take folder stays available after closing the panel.

Disk write failures, FIFO overflow and interruption of the guitar processing path stop recording and display an incomplete-take error; partial files remain available for inspection. Takes are limited to one hour or the standard WAV byte limit, whichever comes first. Rate/buffer changes stop transport and finalize active recording. Closing the app flushes remaining queued audio once host callbacks have stopped. Finished takes enter the [take library](TAKE-LIBRARY.md) automatically for naming, favorites, review and manual offline reamping. Recovery from an OS/process crash, waveform editing, punch-in, reference-mix export and automatic reamping remain future work.

## Verification

Native regression checks exercise 44.1/48/96 kHz playback, filtered sample-rate conversion, mono duplication, stereo level, pause/cursor, seek, EOF, loop wrapping inside variable callbacks, failed-load retention, count-in cancellation, unique take directories, float headroom, reopened dry/processed WAV frame equality, click/backing exclusion, deterministic FIFO overflow, and integration after high-gain processing but before Master. UI checks cover commands/count-in, record-time control locking, DAW ownership, error propagation and folder access.

Speed checks measure unchanged 440/660 Hz stereo tones at 50/75/150% on 44.1/48/96 kHz interfaces, original-time seek/loop/EOF behavior, cursor/loop retention, cancellation, stale-publication rejection, invalid/short-track errors and real-time recording boundaries. Fade tests measure seam-jump reduction with unchanged loop length. UI tests cover speed/fade commands, progress, preparation cancellation and recording locks.

Waveform/section checks cover stereo extrema without cancellation, bounded envelopes and revision-only fetching, pointer/keyboard seeks, failed/cancelled publication, source-content identity, renamed copies, speed/rate retention, paused recall, CRUD/limits, corrupted metadata preservation, stale writers and deleted targets, and loading/count-in/recording locks. No waveform peaks are repeated in ordinary status payloads.

Live guitar/interface listening and Linux native execution still require validation. Passing synthetic tests does not establish final listening quality or commercial release readiness.

Windows validation (2026-10-03): 103 UI tests and all four CTest suites pass; Release VST3 and the side-by-side standalone build succeed. The browser panel fits the editor's 860 × 620 minimum width with vertical scrolling inside the practice panel and no document overflow.
