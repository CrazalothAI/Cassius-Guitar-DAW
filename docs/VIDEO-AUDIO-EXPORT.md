# Record and export audio for video

Use Cassian for the audio and Clipchamp (or another editor) for the video. No camera capture or video encoder is required in Cassian.

1. Choose a complete rig. If you want backing in the exported soundtrack, load the backing audio in **Practice**. Browser/YouTube playback is not captured.
2. Press **Record guitar take**, choose a folder and play. Stop to finish writing the files. Cassian saves mono `Guitar dry.wav`, stereo `Guitar processed.wav`, and a synchronized stereo `Backing track.wav`, plus the original rig and take metadata. With no backing loaded, that stem contains silence.
3. Open **Takes**, select your recording and choose **Original processed**, **Dry DI**, or a reamp in **Take version**. Use Listen to check it.
4. Expand **Video soundtrack**. Set Guitar balance, choose whether to include the recorded backing and adjust Backing balance. Press **Export for video** and choose a new WAV filename.
5. Import the exported WAV and your camera video into Clipchamp and align them. Microsoft lists WAV among [supported Clipchamp audio inputs](https://support.microsoft.com/en-au/clipchamp/video-file-formats-supported-by-clipchamp). `Show exported audio` reveals the finished file.

The export is 48 kHz, 24-bit PCM stereo, independently of the original interface rate. It preserves the take duration and chosen guitar/backing balance. A two-pass sample-peak check attenuates the combined soundtrack only when needed to keep peaks at −1 dBFS; it does not boost quiet passages, compress dynamics or claim true-peak/LUFS mastering. Master, listening-only Play Along focus, metronome and count-in are excluded. Backing follows the actual Practice speed, loops and playback level during recording. These remain separate from the original guitar files.

Older takes without a backing stem can export guitar alone. Incomplete recordings, unknown versions, mismatched stems and existing output filenames are rejected. Export runs on the take worker with bounded buffers, progress and cancellation; partial outputs are removed and original recordings are preserved. Reamping remains available before soundtrack export.

Automated checks cover count-in-to-record alignment, independent backing/guitar/click content, 44.1/48/96 kHz conversion, duration, 24-bit stereo headers, requested balance, peak headroom and overwrite protection. Real camera sync and importing the produced file into the owner's Clipchamp session still need a user check.
