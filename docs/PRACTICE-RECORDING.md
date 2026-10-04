# Practice player and paired guitar recording

The standalone app's **Practice & record** button switches the lower controls to a backing-track transport without hiding the amp. The VST3 leaves practice playback and recording to the DAW. No tone parameter IDs, rig schema, or owner attribution markers change.

## Playback

- Import a local mono/stereo track using the audio formats registered in the native picker. WAV, AIFF, FLAC and Ogg are enabled in the current JUCE configuration; optional platform formats depend on build settings.
- Play/Pause/Stop, seek, independent backing level (−60 to +6 dB), and A/B section looping. Loop points must be at least 50 ms apart. Looping wraps inside callbacks rather than waiting for a UI timer. Hard loop cuts can click when the endpoints differ; crossfaded musical loop editing is future work.
- A worker decodes and uses JUCE's filtered resampler to prepare immutable stereo audio at the interface rate. The callback performs no file reading, decoding, allocation, ownership destruction, or mutex acquisition. Retired tracks are reclaimed on the worker using an audio-thread hazard pointer.
- Decoded audio is bounded to 256 MiB per track (about 11.7 minutes at 48 kHz, 5.8 at 96 kHz). Replacing a track can temporarily retain both old and new buffers. Tracks of greater length, streaming and time stretching are future work.
- Failed loads preserve the current track. Tracks are rebuilt following device preparation and never automatically resume. Transport state is not part of complete rigs or DAW project serialization.

## Count-in and recording

Choose Off, one bar or two bars. BPM and beats per bar are latched from the metronome when starting. The count-in precedes backing playback and guitar capture, with a sample-timed transition even inside a callback. Guitar monitoring remains active during the count-in. The ordinary metronome is suppressed during count-in; if enabled it resumes on the next host callback after count-in completion.

**Record guitar** asks for a parent folder and creates a new timestamped directory with a numeric suffix when needed. It never intentionally replaces a previous take. Recording also works without a backing track.

| File | Source | Format |
|---|---|---|
| Guitar dry.wav | Physical mono guitar input, before software Input gain and all guitar processing | 32-bit float mono WAV at the interface rate |
| Guitar processed.wav | Guitar after the amp, cabinets and effects, before backing/click addition and Master/limiter | 32-bit float stereo WAV at the interface rate |

Files have the same start boundary and frame count. Effect and model latency is retained in the processed signal; this is not latency-compensated reamping. Processed samples above 0 dBFS are retained in float format rather than clipped. Import at a suitable playback gain. NaNs/infinities are written as silence. Backing tracks and metronome/count-in clicks are excluded from both files.

A preallocated 262,144-frame, three-channel single-producer/single-consumer FIFO sends audio to the disk worker. Both writers consume the same frame slices. **Finish take** pauses playback, stops capture, drains queued frames, and closes the writers to finalize WAV headers. **Stop** additionally rewinds. Finishing does not require another audio callback. The last take folder stays available after closing the panel.

Disk write failures, FIFO overflow and interruption of the guitar processing path stop recording and display an incomplete-take error; partial files remain available for inspection. Takes are limited to one hour or the standard WAV byte limit, whichever comes first. Rate/buffer changes stop transport and finalize active recording. Closing the app flushes remaining queued audio once host callbacks have stopped. Recovery from an OS/process crash, take editing, punch-in, reference-mix export and automatic reamping remain future work.

## Verification

Native regression checks exercise 44.1/48/96 kHz playback, filtered sample-rate conversion, mono duplication, stereo level, pause/cursor, seek, EOF, loop wrapping inside variable callbacks, failed-load retention, count-in cancellation, unique take directories, float headroom, reopened dry/processed WAV frame equality, click/backing exclusion, deterministic FIFO overflow, and integration after high-gain processing but before Master. UI checks cover commands/count-in, record-time control locking, DAW ownership, error propagation and folder access.

Live guitar/interface listening and Linux native execution still require validation. Passing synthetic tests does not establish final listening quality or commercial release readiness.

Windows validation (2026-10-03): 71 UI tests and all four CTest suites pass; Release VST3 and the side-by-side standalone build succeed. The browser panel fits the editor's 860 × 620 minimum width with vertical scrolling inside the practice panel and no horizontal overflow.
