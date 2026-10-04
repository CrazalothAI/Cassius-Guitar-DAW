#include "PracticeEngine.h"
#include <cmath>

namespace {
// AudioFormatReaderSource discards decoder read failures; retain them so a
// truncated/unreadable track is not silently published as a successful import.
class CheckedDecoder final : public juce::AudioSource {
public:
    explicit CheckedDecoder(juce::AudioFormatReader& r) : reader(r) {}
    void prepareToPlay(int, double) override {}
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        info.clearActiveBufferRegion();
        const int n = static_cast<int>(juce::jlimit<juce::int64>(0, info.numSamples, reader.lengthInSamples - position));
        if (n > 0 && !reader.read(info.buffer, info.startSample, n, position, true, true)) failed = true;
        position += info.numSamples;
    }
    bool failed = false;
private:
    juce::AudioFormatReader& reader; juce::int64 position = 0;
};
}

PracticeEngine::PracticeEngine(int frames) : Thread("Cassian practice disk IO"), fifo(juce::jmax(32, frames)), recordingAudio(3, juce::jmax(32, frames))
{ gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(-12.0f)); startThread(); }
PracticeEngine::~PracticeEngine()
{
    playing.store(false); recordMode.store(4); signalThreadShouldExit(); notify(); stopThread(-1);
    // Processor destruction occurs after the host has stopped its callbacks.
}
void PracticeEngine::prepare(double sampleRate)
{
    playing.store(false); countActive.store(false); startRequested.store(false);
    if (recordMode.load() != 0) recordMode.store(4);
    rate.store(sampleRate); gain.reset(sampleRate, .02);
    gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(levelDb.load()));
    countSamples = countLength = 0; audioEpoch = startEpoch.load(); notify();
    const juce::ScopedLock lock(control);
    if (loadedFile.existsAsFile()) { pendingTrack = loadedFile; trackPending = true; loadingTrack.store(true); ++loadGeneration; }
}
void PracticeEngine::setCountIn(int n, double tempo, int meter)
{ bars.store(juce::jlimit(0, 2, n)); bpm.store(juce::jlimit(20., 400., tempo)); beats.store(juce::jlimit(1, 12, meter)); }
void PracticeEngine::startCount() { playing.store(false); countBeat.store(0); startRequested.store(true); startEpoch.fetch_add(1); }
void PracticeEngine::load(const juce::File& file)
{
    const juce::ScopedLock lock(control);
    if (recordMode.load() != 0) { error = "Stop and finish the take before loading another track."; return; }
    pendingTrack = file; trackPending = true; loadingTrack.store(true); ++loadGeneration; error.clear(); notify();
}
juce::String PracticeEngine::command(const juce::String& name, double x)
{
    const juce::ScopedLock lock(control); // UI thread only; process() never takes it.
    if (!std::isfinite(x)) return "Invalid practice control.";
    if (name == "stop") { startRequested.store(false); startEpoch.fetch_add(1); playing.store(false); countActive.store(false); if (recordMode.load() != 0) recordMode.store(4); seek.store(0); notify(); }
    else if (name == "pause") { startRequested.store(false); startEpoch.fetch_add(1); playing.store(false); countActive.store(false); if (recordMode.load() != 0) recordMode.store(4); notify(); }
    else if (name == "play") { if (loadingTrack.load()) return "Wait for the backing track to load."; if (recordMode.load() != 0) return "Finish the current take first."; if (duration.load() <= 0) return "Load a backing track first."; startCount(); }
    else if (name == "level") levelDb.store(static_cast<float>(juce::jlimit(-60., 6., x)));
    else if (name == "seek") { if (recordMode.load() != 0 || countActive.load()) return "Stop the take or count-in before seeking."; seek.store(juce::jlimit(0., duration.load(), x)); }
    else if (name == "a" || name == "b") {
        if (recordMode.load() != 0) return "Stop the take before changing loop points.";
        (name == "a" ? loopA : loopB).store(juce::jlimit(0., duration.load(), x));
        if (loopB.load() - loopA.load() < .05) loop.store(false);
    }
    else if (name == "loop") { if (x != 0 && loopB.load() - loopA.load() < .05) return "Set B at least 0.05 seconds after A."; loop.store(x != 0); }
    else return "Unknown practice control.";
    return {};
}
juce::String PracticeEngine::record(const juce::File& parent)
{
    const juce::ScopedLock lock(control);
    if (loadingTrack.load()) return "Wait for the backing track to load.";
    int idle = 0;
    if (!recordMode.compare_exchange_strong(idle, 1)) return "Finish the current take first.";
    recordingFault.store(0);
    playing.store(false); countActive.store(false);
    pendingRecording = parent; recordPending = true; error.clear(); notify(); return {};
}
void PracticeEngine::readTrack(const juce::File& file, unsigned generation)
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    juce::String failure;
    std::unique_ptr<Track> next;
    // Bound decoded memory, not compressed file size. Stereo float audio <= 256 MiB.
    constexpr juce::int64 maximumFrames = 256 * 1024 * 1024 / (2 * sizeof(float));
    const double targetRate = rate.load();
    const double targetFrames = reader && reader->sampleRate > 0 ? std::ceil(reader->lengthInSamples * targetRate / reader->sampleRate) : 0;
    if (!reader || !std::isfinite(targetFrames) || targetFrames < 1 || targetFrames > maximumFrames || reader->numChannels < 1 || reader->numChannels > 2)
        failure = "Choose a supported mono/stereo audio file under 256 MiB when decoded.";
    else {
        next = std::make_unique<Track>(); next->rate = targetRate; next->name = file.getFileName();
        next->audio.setSize(2, static_cast<int>(targetFrames));
        // JUCE's filtered resampler and reader run entirely on this disk thread.
        // The callback sees immutable audio already at the interface sample rate.
        const auto inputRate = reader->sampleRate; const bool mono = reader->numChannels == 1;
        CheckedDecoder input(*reader);
        juce::ResamplingAudioSource resampler(&input, false, 2);
        resampler.setResamplingRatio(inputRate / targetRate); resampler.prepareToPlay(1024, targetRate);
        for (int offset = 0; offset < next->audio.getNumSamples(); offset += 1024) {
            if (threadShouldExit()) return;
            resampler.getNextAudioBlock({&next->audio, offset, juce::jmin(1024, next->audio.getNumSamples() - offset)});
        }
        resampler.releaseResources();
        if (input.failed) { next.reset(); failure = "Could not decode the backing track."; }
        else {
            if (mono) next->audio.copyFrom(1, 0, next->audio, 0, 0, next->audio.getNumSamples());
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < next->audio.getNumSamples(); ++i)
                if (!std::isfinite(next->audio.getSample(ch, i))) next->audio.setSample(ch, i, 0);
        }
    }
    const juce::ScopedLock lock(control);
    if (generation != loadGeneration) return;
    loadingTrack.store(false);
    error = failure;
    if (!next) return; // A failed import preserves the previous track.
    startRequested.store(false); startEpoch.fetch_add(1);
    playing.store(false); countActive.store(false); seek.store(0); loop.store(false); loopA.store(0);
    const auto seconds = next->audio.getNumSamples() / next->rate;
    duration.store(seconds); loopB.store(seconds); trackName = next->name; loadedFile = file;
    auto old = std::move(ownedTrack); ownedTrack = std::move(next); track.store(ownedTrack.get());
    if (old) retired.push_back(std::move(old));
}
void PracticeEngine::reclaimTracks()
{
    const auto* inUse = hazard.load();
    std::erase_if(retired, [inUse](const auto& old) { return old.get() != inUse; });
}
void PracticeEngine::beginRecording(const juce::File& parent)
{
    if (recordMode.load() != 1) return; // Stop may cancel the pending request.
    juce::String failure;
    const auto folder = parent.getNonexistentChildFile("Cassian take " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S"), "", true);
    juce::WavAudioFormat wav;
    recordingRate.store(rate.load());
    if (!parent.isDirectory() || folder.createDirectory().failed()) failure = "Could not create the take folder.";
    else {
        const auto make = [&](const juce::File& file, unsigned channels) {
            auto stream = file.createOutputStream();
            if (!stream) return std::unique_ptr<juce::AudioFormatWriter>();
            auto writer = std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(), recordingRate.load(), channels, 32, {}, 0));
            if (writer) stream.release();
            return writer;
        };
        dryWriter = make(folder.getChildFile("Guitar dry.wav"), 1);
        wetWriter = make(folder.getChildFile("Guitar processed.wav"), 2);
        if (!dryWriter || !wetWriter) failure = "Could not open both recording files.";
    }
    { const juce::ScopedLock lock(control); takePath = folder.getFullPathName(); error = failure; }
    if (failure.isNotEmpty()) { dryWriter.reset(); wetWriter.reset(); recordMode.store(0); return; }
    fifo.reset(); recordedFrames.store(0); recordingFault.store(0);
    // Only publish the writers after both files and the FIFO are ready.
    const juce::ScopedLock lock(control);
    int preparing = 1;
    if (recordMode.compare_exchange_strong(preparing, 2)) startCount();
}
void PracticeEngine::drainRecording()
{
    if (!dryWriter || !wetWriter) return;
    const auto write = [&](int start, int size) {
        if (size == 0) return true;
        const float* dry[] {recordingAudio.getReadPointer(0, start)};
        const float* wet[] {recordingAudio.getReadPointer(1, start), recordingAudio.getReadPointer(2, start)};
        // Attempt both writes so a failure can never be reported as a successful pair.
        const bool a = dryWriter->writeFromFloatArrays(dry, 1, size);
        const bool b = wetWriter->writeFromFloatArrays(wet, 2, size); return a && b;
    };
    int a, na, b, nb; fifo.prepareToRead(fifo.getNumReady(), a, na, b, nb);
    const bool first = write(a, na), second = write(b, nb); fifo.finishedRead(na + nb);
    if (!first || !second) { recordingFault.store(1); recordMode.store(4); }
}
void PracticeEngine::run()
{
    while (!threadShouldExit()) {
        juce::File file, destination; unsigned generation = 0; bool loading = false, recording = false;
        { const juce::ScopedLock lock(control);
          if (trackPending) { file = pendingTrack; generation = loadGeneration; trackPending = false; loading = true; }
          if (recordPending) { destination = pendingRecording; recordPending = false; recording = true; } }
        try {
            if (loading) readTrack(file, generation);
            if (recording) beginRecording(destination);
        } catch (const std::exception& e) {
            const juce::ScopedLock lock(control); error = "Practice file error: " + juce::String(e.what());
            if (generation == loadGeneration) loadingTrack.store(false);
            if (recording) recordMode.store(4);
        }
        drainRecording(); reclaimTracks();
        if (recordMode.load() == 4 && callbacks.load() == 0) {
            drainRecording(); dryWriter.reset(); wetWriter.reset();
            if (recordingFault.load() != 0) {
                const juce::ScopedLock lock(control);
                error = recordingFault.load() == 3 ? "Recording stopped at the take duration/size limit. The take has been saved."
                    : recordingFault.load() == 2 ? "Recording stopped: guitar processing was interrupted. Check the incomplete take before using it."
                    : "Recording stopped: disk write failed or recording buffer filled. Check the incomplete take before using it.";
            }
            recordMode.store(0);
        }
        wait(recordMode.load() != 0 || !retired.empty() ? 10 : 250);
    }
    // Destruction is off the audio thread and flushes the remaining FIFO/header.
    drainRecording(); dryWriter.reset(); wetWriter.reset();
}
bool PracticeEngine::process(juce::AudioBuffer<float>& output, const float* dry, bool guitarAvailable)
{
    callbacks.fetch_add(1);
    Track* source;
    do { source = track.load(); hazard.store(source); } while (source != track.load());
    const auto sampleRate = rate.load();
    const auto epoch = startEpoch.load();
    if (epoch != audioEpoch) {
        audioEpoch = epoch; const bool starting = startRequested.exchange(false);
        countActive.store(starting); if (!starting) playing.store(false);
        beatSamples = sampleRate * 60 / bpm.load();
        countLength = std::round(beatSamples * beats.load() * bars.load()); countSamples = 0; clickAge = 1e9;
        if (starting && source && position >= source->audio.getNumSamples() / source->rate) position = 0;
    }
    const auto requestedSeek = seek.exchange(-1);
    if (requestedSeek >= 0) position = requestedSeek;
    gain.setTargetValue(juce::Decibels::decibelsToGain(levelDb.load()));
    if (!playing.load() && !countActive.load() && recordMode.load() != 2 && recordMode.load() != 3) {
        gain.skip(output.getNumSamples()); reportedPosition.store(position);
        hazard.store(nullptr); callbacks.fetch_sub(1); return false;
    }
    const bool counted = countActive.load() && countLength > 0;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0, written = 0;
    if (recordMode.load() == 2 || recordMode.load() == 3)
        fifo.prepareToWrite(output.getNumSamples(), start1, size1, start2, size2);
    const bool looping = loop.load(); const double a = loopA.load(), b = loopB.load();
    // Standard WAV's 32-bit chunk lengths also bound takes at very high rates.
    const auto frameLimit = static_cast<juce::int64>(juce::jmin(recordingRate.load() * 3600., (4294967295. - 1048576.) / 8));
    for (int i = 0; i < output.getNumSamples(); ++i) {
        float click = 0;
        if (countActive.load()) {
            if (countSamples >= countLength) {
                countActive.store(false); playing.store(source != nullptr);
                int armed = 2; recordMode.compare_exchange_strong(armed, 3);
            } else {
                const auto beat = static_cast<int>(countSamples / beatSamples);
                const auto previous = static_cast<int>((countSamples - 1) / beatSamples);
                if (countSamples == 0 || beat != previous) { clickAge = 0; countBeat.store(beat + 1); }
                if (clickAge < sampleRate * .03) {
                    const double t = clickAge / sampleRate;
                    click = static_cast<float>(.12 * std::min(1., t / .001) * std::exp(-t * 150) * std::sin(juce::MathConstants<double>::twoPi * 1600 * t)); ++clickAge;
                }
                ++countSamples;
            }
        }
        if (!guitarAvailable) interrupted();
        if (recordMode.load() == 3) {
            if (recordedFrames.load() + written >= frameLimit) { recordingFault.store(3); recordMode.store(4); }
            else if (written >= size1 + size2) { recordingFault.store(1); recordMode.store(4); }
            else {
                const auto index = written < size1 ? start1 + written : start2 + written - size1;
                recordingAudio.setSample(0, index, std::isfinite(dry[i]) ? dry[i] : 0);
                for (int ch = 0; ch < 2; ++ch) {
                    const auto x = output.getSample(juce::jmin(ch, output.getNumChannels() - 1), i);
                    recordingAudio.setSample(ch + 1, index, std::isfinite(x) ? x : 0);
                }
                ++written;
            }
        }
        const float volume = gain.getNextValue();
        if (source && playing.load()) {
            const double end = source->audio.getNumSamples() / source->rate;
            if (looping && b > a && b <= end && position >= b) position = a + std::fmod(position - a, b - a);
            if (position >= end) { playing.store(false); position = end; }
            else {
                const double frame = position * source->rate;
                const int index = juce::jlimit(0, source->audio.getNumSamples() - 1, static_cast<int>(frame));
                const float fraction = static_cast<float>(frame - index);
                int next = juce::jmin(index + 1, source->audio.getNumSamples() - 1);
                if (looping && b > a && (index + 1) / source->rate >= b) next = juce::jlimit(0, source->audio.getNumSamples() - 1, static_cast<int>(a * source->rate));
                for (int ch = 0; ch < output.getNumChannels(); ++ch) {
                    const auto* samples = source->audio.getReadPointer(juce::jmin(ch, 1));
                    output.addSample(ch, i, volume * (samples[index] + fraction * (samples[next] - samples[index])));
                }
                position += 1 / sampleRate;
                if ((!looping || b <= a || b > end) && position >= end) { position = end; playing.store(false); }
            }
        }
        for (int ch = 0; ch < output.getNumChannels(); ++ch) output.addSample(ch, i, click);
    }
    if (written > 0) { fifo.finishedWrite(written); recordedFrames.fetch_add(written); }
    reportedPosition.store(position); hazard.store(nullptr); callbacks.fetch_sub(1);
    return counted;
}
juce::var PracticeEngine::status()
{
    auto o = std::make_unique<juce::DynamicObject>();
    { const juce::ScopedLock lock(control); o->setProperty("track", trackName); o->setProperty("error", error); o->setProperty("takePath", takePath); }
    o->setProperty("loading", loadingTrack.load());
    o->setProperty("duration", duration.load()); o->setProperty("position", reportedPosition.load());
    o->setProperty("playing", playing.load()); o->setProperty("counting", countActive.load()); o->setProperty("countBeat", countBeat.load());
    o->setProperty("recordMode", recordMode.load()); o->setProperty("recordSeconds", recordedFrames.load() / recordingRate.load());
    o->setProperty("level", levelDb.load()); o->setProperty("loop", loop.load()); o->setProperty("a", loopA.load()); o->setProperty("b", loopB.load());
    return juce::var(o.release());
}
