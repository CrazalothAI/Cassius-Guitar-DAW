#include "PracticeEngine.h"
#include "TakeRecovery.h"
#include <cmath>
#include <optional>
#include <signalsmith-stretch/signalsmith-stretch.h>

namespace {
class CheckedHashInput final : public juce::InputStream {
public:
    CheckedHashInput(juce::FileInputStream& s, std::function<bool()> c) : input(s), cancelled(std::move(c)) {}
    juce::int64 getTotalLength() override { return input.getTotalLength(); }
    juce::int64 getPosition() override { return input.getPosition(); }
    bool setPosition(juce::int64 p) override { return input.setPosition(p); }
    bool isExhausted() override { return input.isExhausted(); }
    int read(void* out, int n) override {
        if (cancelled()) throw std::runtime_error("Track preparation cancelled.");
        const int got = input.read(out, juce::jmin(n, 65536));
        if (got == 0 && !input.isExhausted()) throw std::runtime_error("Could not read track identity."); return got;
    }
private:
    juce::FileInputStream& input; std::function<bool()> cancelled;
};
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

PracticeEngine::PracticeEngine(int frames, juce::File sectionsDirectory, juce::int64 threshold) : Thread("Cassian practice disk IO"), sections(std::move(sectionsDirectory)), streamingThreshold(threshold), fifo(juce::jmax(32, frames)), recordingAudio(5, juce::jmax(32, frames))
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
    if (loadedFile.existsAsFile()) { pendingTrack = loadedFile; trackPending = true; preserveTrackPosition = false; loadingTrack.store(true); loadProgress.store(0); ++loadGeneration; }
}
void PracticeEngine::setCountIn(int n, double tempo, int meter)
{ bars.store(juce::jlimit(0, 2, n)); bpm.store(juce::jlimit(20., 400., tempo)); beats.store(juce::jlimit(1, 12, meter)); }
void PracticeEngine::startCount() { playing.store(false); countBeat.store(0); startRequested.store(true); startEpoch.fetch_add(1); }
void PracticeEngine::load(const juce::File& file)
{
    const juce::ScopedLock lock(control);
    if (recordMode.load() != 0) { error = "Stop and finish the take before loading another track."; return; }
    pendingTrack = file; trackPending = true; preserveTrackPosition = false; loadingTrack.store(true); loadProgress.store(0); ++loadGeneration; error.clear(); notify();
}
juce::String PracticeEngine::command(const juce::String& name, double x)
{
    const juce::ScopedLock lock(control); // UI thread only; process() never takes it.
    if (!std::isfinite(x)) return "Invalid practice control.";
    if (name == "cancelLoad") {
        ++loadGeneration; trackPending = false; loadingTrack.store(false); requestedSpeed.store(playbackSpeed.load()); loadProgress.store(0); error.clear(); notify(); return {};
    }
    if (name == "stop") { startRequested.store(false); startEpoch.fetch_add(1); playing.store(false); countActive.store(false); if (recordMode.load() != 0) recordMode.store(4); seek.store(0); notify(); }
    else if (name == "pause") { startRequested.store(false); startEpoch.fetch_add(1); playing.store(false); countActive.store(false); if (recordMode.load() != 0) recordMode.store(4); notify(); }
    else if (name == "play") { if (loadingTrack.load()) return "Wait for the backing track to load."; if (ownedTrack && ownedTrack->stream && ownedTrack->stream->hasFailed()) return "Reload this take version before listening."; if (recordMode.load() != 0) return "Finish the current take first."; if (duration.load() <= 0) return "Load a backing track first."; startCount(); }
    else if (name == "level") levelDb.store(static_cast<float>(juce::jlimit(-60., 6., x)));
    else if (name == "speed") {
        if (ownedTrack && ownedTrack->stream && x != 1) return "Long-take review currently plays at normal speed.";
        if (x < .5 || x > 1.5) return "Choose a practice speed between 50% and 150%.";
        if (recordMode.load() != 0 || countActive.load() || startRequested.load()) return "Stop the take or count-in before changing speed.";
        if (loadingTrack.load()) return "Wait for the track to finish preparing.";
        if (!loadedFile.existsAsFile()) return "Load a backing track first.";
        if (x == playbackSpeed.load()) return {};
        playing.store(false); startEpoch.fetch_add(1);
        requestedSpeed.store(x); pendingTrack = loadedFile; trackPending = true; preserveTrackPosition = true;
        loadingTrack.store(true); loadProgress.store(0); ++loadGeneration; error.clear(); notify();
    }
    else if (name == "fade") { if (loadingTrack.load() || recordMode.load() != 0 || countActive.load() || startRequested.load()) return "Finish preparation, the take or count-in before changing loop fades."; loopFadeMs.store(juce::jlimit(0., 20., x)); }
    else if (name == "seek") { if (loadingTrack.load() || recordMode.load() != 0 || countActive.load() || startRequested.load()) return "Finish track preparation, the take or count-in before seeking."; seek.store(juce::jlimit(0., duration.load(), x)); }
    else if (name == "a" || name == "b") {
        if (loadingTrack.load() || recordMode.load() != 0 || countActive.load() || startRequested.load()) return "Finish preparation, the take or count-in before changing loop points.";
        (name == "a" ? loopA : loopB).store(juce::jlimit(0., duration.load(), x));
        if (loopB.load() - loopA.load() < .05) loop.store(false);
    }
    else if (name == "loop") { if (loadingTrack.load() || recordMode.load() != 0 || countActive.load() || startRequested.load()) return "Finish preparation, the take or count-in before changing looping."; if (x != 0 && loopB.load() - loopA.load() < .05) return "Set B at least 0.05 seconds after A."; loop.store(x != 0); }
    else return "Unknown practice control.";
    return {};
}
juce::String PracticeEngine::record(const juce::File& parent, const juce::var& rig)
{
    const juce::ScopedLock lock(control);
    if (loadingTrack.load()) return "Wait for the backing track to load.";
    if (ownedTrack && ownedTrack->stream) return "Record with the Practice transport; long-take review is for listening only.";
    int idle = 0;
    if (!recordMode.compare_exchange_strong(idle, 1)) return "Finish the current take first.";
    recordingFault.store(0);
    playing.store(false); countActive.store(false);
    pendingRecording = parent; pendingRigJson = rig.isObject() ? juce::JSON::toString(rig) : juce::String();
    recordPending = true; error.clear(); notify(); return {};
}
bool PracticeEngine::cancelled(unsigned generation)
{ const juce::ScopedLock lock(control); return threadShouldExit() || generation != loadGeneration; }
void PracticeEngine::readTrack(const juce::File& file, unsigned generation, double speed, bool preservePosition)
{
    const auto fileSize = file.getSize(); const auto modified = file.getLastModificationTime();
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    juce::String failure;
    std::unique_ptr<Track> next;
    juce::var nextPeaks, nextSections {juce::Array<juce::var>()}; juce::String nextKey, nextSectionError;
    // Bound decoded memory, not compressed file size. Stereo float audio <= 256 MiB.
    constexpr juce::int64 maximumFrames = 256 * 1024 * 1024 / (2 * sizeof(float));
    const double targetRate = rate.load();
    const double targetFrames = reader && reader->sampleRate > 0 ? std::ceil(reader->lengthInSamples * targetRate / reader->sampleRate) : 0;
    const double stretchedFrames = std::ceil(targetFrames / speed);
    const bool streaming = reader && std::isfinite(targetFrames) && targetFrames < 1e12 && targetFrames > streamingThreshold && streamingThreshold >= 0 && file.hasFileExtension("wav");
    if (streaming && speed == 1 && reader->numChannels >= 1 && reader->numChannels <= 2) {
        next = std::make_unique<Track>(); next->rate = targetRate; next->name = file.getFileName(); next->duration = targetFrames / targetRate;
        next->stream = std::make_unique<ReviewStream>(file, std::move(reader), targetRate);
        next->stream->validate();
        nextPeaks = next->stream->envelope([&] { return cancelled(generation); }, [&](double p) { loadProgress.store(p * .7); });
        if (cancelled(generation)) return;
        // Two blocks are ready before publication; the worker fills the rest
        // afterward. A cached block is always identified by its source frame.
        for (int i = 0; i < 2; ++i) next->stream->service([&] { return cancelled(generation); });
        if (next->stream->hasFailed()) { failure = next->stream->failure(); next.reset(); }
    }
    else if (!reader || !std::isfinite(targetFrames) || targetFrames < 1 || targetFrames > maximumFrames || stretchedFrames > maximumFrames || reader->numChannels < 1 || reader->numChannels > 2)
        failure = "Choose a supported mono/stereo file under 256 MiB when decoded at the selected speed.";
    else {
        next = std::make_unique<Track>(); next->rate = targetRate; next->name = file.getFileName();
        next->speed = speed; next->duration = targetFrames / targetRate;
        next->audio.setSize(2, static_cast<int>(targetFrames));
        // JUCE's filtered resampler and reader run entirely on this disk thread.
        // The callback sees immutable audio already at the interface sample rate.
        const auto inputRate = reader->sampleRate; const bool mono = reader->numChannels == 1;
        CheckedDecoder input(*reader);
        juce::ResamplingAudioSource resampler(&input, false, 2);
        resampler.setResamplingRatio(inputRate / targetRate); resampler.prepareToPlay(1024, targetRate);
        for (int offset = 0; offset < next->audio.getNumSamples(); offset += 1024) {
            if (cancelled(generation)) return;
            resampler.getNextAudioBlock({&next->audio, offset, juce::jmin(1024, next->audio.getNumSamples() - offset)});
            loadProgress.store((offset + juce::jmin(1024, next->audio.getNumSamples() - offset)) / targetFrames * (speed == 1 ? 1 : .5));
        }
        resampler.releaseResources();
        if (input.failed) { next.reset(); failure = "Could not decode the backing track."; }
        else {
            if (mono) next->audio.copyFrom(1, 0, next->audio, 0, 0, next->audio.getNumSamples());
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < next->audio.getNumSamples(); ++i)
                if (!std::isfinite(next->audio.getSample(ch, i))) next->audio.setSample(ch, i, 0);
            // A bounded stereo min/max envelope in original-track time. Calculate
            // before stretching; status polls never scan or resend this audio.
            juce::Array<juce::var> peaks; const int samples = next->audio.getNumSamples(), bins = juce::jmin(512, samples);
            for (int bin = 0; bin < bins; ++bin) {
                if (cancelled(generation)) return;
                const int begin = static_cast<int>(static_cast<juce::int64>(bin) * samples / bins), end = static_cast<int>(static_cast<juce::int64>(bin + 1) * samples / bins);
                float low = 0, high = 0;
                for (int ch = 0; ch < 2; ++ch) for (int i = begin; i < end; ++i) { const auto x = next->audio.getSample(ch, i); low = juce::jmin(low, x); high = juce::jmax(high, x); }
                peaks.add(juce::var(juce::Array<juce::var> {low, high}));
            }
            nextPeaks = juce::var(peaks);
            if (speed != 1) {
                signalsmith::stretch::SignalsmithStretch<float> stretch(0); stretch.presetDefault(2, static_cast<float>(targetRate));
                const int inputFrames = next->audio.getNumSamples(), outputFrames = static_cast<int>(stretchedFrames);
                const double ratio = inputFrames / static_cast<double>(outputFrames);
                const int head = stretch.outputSeekLength(static_cast<float>(ratio));
                if (inputFrames <= head) { failure = "This track is too short for pitch-preserving speed changes."; next.reset(); }
                else {
                    juce::AudioBuffer<float> stretched(2, outputFrames);
                    // Follow upstream exact() alignment while chunking the expensive
                    // work so a replacement/device change/shutdown can cancel it.
                    stretch.outputSeek(next->audio.getArrayOfReadPointers(), head);
                    const int mainFrames = outputFrames - static_cast<int>(head / ratio);
                    int inputOffset = head;
                    for (int offset = 0; offset < mainFrames; offset += 1024) {
                        if (cancelled(generation)) return;
                        const int n = juce::jmin(1024, mainFrames - offset);
                        const int end = head + static_cast<int>(std::round((inputFrames - head) * static_cast<double>(offset + n) / mainFrames));
                        const float* inputs[] {next->audio.getReadPointer(0, inputOffset), next->audio.getReadPointer(1, inputOffset)};
                        float* outputs[] {stretched.getWritePointer(0, offset), stretched.getWritePointer(1, offset)};
                        stretch.process(inputs, end - inputOffset, outputs, n); inputOffset = end;
                        loadProgress.store(.5 + .5 * (offset + n) / outputFrames);
                    }
                    if (cancelled(generation)) return;
                    float* tail[] {stretched.getWritePointer(0, mainFrames), stretched.getWritePointer(1, mainFrames)};
                    stretch.flush(tail, outputFrames - mainFrames, static_cast<float>(ratio));
                    next->audio = std::move(stretched);
                    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < outputFrames; ++i)
                        if (!std::isfinite(next->audio.getSample(ch, i))) next->audio.setSample(ch, i, 0);
                }
            }
        }
    }
    if (next) {
        if (cancelled(generation)) return;
        auto input = file.createInputStream(); if (!input) throw std::runtime_error("Could not read track identity.");
        CheckedHashInput hashInput(*input, [&] { return cancelled(generation); });
        nextKey = juce::SHA256(hashInput).toHexString(); // Cancellable, bounded worker hash.
        if (cancelled(generation)) return;
        if (!file.existsAsFile() || file.getSize() != fileSize || file.getLastModificationTime() != modified) { next.reset(); failure = "The backing track changed during preparation. Reload it."; }
        else try { nextSections = sections.load(nextKey, next->duration); } catch (const std::exception& e) { nextSectionError = e.what(); }
    }
    const juce::ScopedLock lock(control);
    if (generation != loadGeneration) return;
    loadingTrack.store(false);
    error = failure;
    if (!next) { requestedSpeed.store(playbackSpeed.load()); return; } // Failed preparation preserves the previous track/speed.
    startRequested.store(false); startEpoch.fetch_add(1);
    playing.store(false); countActive.store(false);
    const auto seconds = next->duration;
    seek.store(preservePosition ? juce::jlimit(0., seconds, reportedPosition.load()) : 0);
    if (!preservePosition) { loop.store(false); loopA.store(0); loopB.store(seconds); }
    duration.store(seconds); playbackSpeed.store(next->speed); loadProgress.store(1); trackName = next->name; loadedFile = file;
    trackKey = nextKey; wavePeaks = nextPeaks; sectionRows = nextSections; sectionError = nextSectionError; ++waveRevision; ++sectionRevision;
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
    { const juce::ScopedLock lock(control); activeRigJson = pendingRigJson; }
    if (!parent.isDirectory() || folder.createDirectory().failed()) failure = "Could not create the take folder.";
    else {
        recordingLock = std::make_unique<juce::InterProcessLock>(TakeRecovery::lockName(folder));
        if (!recordingLock->enter(0)) failure = "The new recording folder is busy.";
        if (failure.isEmpty()) {
            auto journal=std::make_unique<juce::DynamicObject>(); journal->setProperty("schema",1); journal->setProperty("name",folder.getFileName());
            journal->setProperty("created",juce::Time::getCurrentTime().toISO8601(true)); journal->setProperty("sampleRate",recordingRate.load()); journal->setProperty("frames",0);
            journal->setProperty("incomplete",true); journal->setProperty("recordingState","recording");
            TakeRecovery::writeMetadata(folder.getChildFile("Cassian take.json"),juce::var(journal.release()));
        }
        if (activeRigJson.isNotEmpty() && !folder.getChildFile("Original rig.json").replaceWithText(activeRigJson))
            failure = "Could not save the take's original rig settings.";
        const auto make = [&](const juce::File& file, unsigned channels, size_t index) {
            auto stream = file.createOutputStream();
            if (!stream) return std::unique_ptr<juce::AudioFormatWriter>();
            auto writer = std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(), recordingRate.load(), channels, 32, {}, 0));
            if (writer) recordingStreams[index] = stream.release();
            return writer;
        };
        if (failure.isEmpty()) {
            dryWriter = make(folder.getChildFile("Guitar dry.wav"), 1, 0);
            wetWriter = make(folder.getChildFile("Guitar processed.wav"), 2, 1);
            backingWriter = make(folder.getChildFile("Backing track.wav"), 2, 2);
            if (!dryWriter || !wetWriter || !backingWriter) failure = "Could not open the recording stems.";
        }
    }
    { const juce::ScopedLock lock(control); takePath = folder.getFullPathName(); error = failure; }
    if (failure.isNotEmpty()) { dryWriter.reset(); wetWriter.reset(); backingWriter.reset(); recordingStreams.fill(nullptr); recordingLock.reset(); recordMode.store(0); return; }
    activeTake = folder;
    diskFrames=0; checkpointFrames.store(0); checkpointTime=juce::Time::getMillisecondCounter();
    fifo.reset(); recordedFrames.store(0); recordingFault.store(0);
    // Only publish the writers after all three stems and the FIFO are ready.
    const juce::ScopedLock lock(control);
    int preparing = 1;
    if (recordMode.compare_exchange_strong(preparing, 2)) startCount();
}
void PracticeEngine::drainRecording()
{
    if (!dryWriter || !wetWriter || !backingWriter) return;
    const auto write = [&](int start, int size) {
        if (size == 0) return true;
        const float* dry[] {recordingAudio.getReadPointer(0, start)};
        const float* wet[] {recordingAudio.getReadPointer(1, start), recordingAudio.getReadPointer(2, start)};
        const float* backing[] {recordingAudio.getReadPointer(3, start), recordingAudio.getReadPointer(4, start)};
        // Attempt every stem write so a partial take cannot be reported as complete.
        const bool a = dryWriter->writeFromFloatArrays(dry, 1, size);
        const bool b = wetWriter->writeFromFloatArrays(wet, 2, size);
        const bool c = backingWriter->writeFromFloatArrays(backing, 2, size);
        if (a && b && c) diskFrames += size;
        return a && b && c;
    };
    int a, na, b, nb; fifo.prepareToRead(fifo.getNumReady(), a, na, b, nb);
    const bool first = write(a, na), second = write(b, nb); fifo.finishedRead(na + nb);
    if (!first || !second) { recordingFault.store(1); recordMode.store(4); }
    const auto now=juce::Time::getMillisecondCounter();
    if (first && second && diskFrames > checkpointFrames.load() && now-checkpointTime >= 2000) {
        // All seeks, header updates and stream flushes stay on this disk worker.
        const bool a=dryWriter->flush(), b=wetWriter->flush(), c=backingWriter->flush();
        bool flushed=a && b && c;
        for (auto* stream:recordingStreams) { if (stream) { stream->flush(); flushed=stream->getStatus().wasOk() && flushed; } else flushed=false; }
        if (flushed) { checkpointFrames.store(diskFrames); checkpointTime=now; }
        else { recordingFault.store(1); recordMode.store(4); }
    }
}
void PracticeEngine::finishTake()
{
    drainRecording(); dryWriter.reset(); wetWriter.reset(); backingWriter.reset(); recordingStreams.fill(nullptr);
    if (activeTake == juce::File()) { recordingLock.reset(); return; }
    auto metadata = std::make_unique<juce::DynamicObject>();
    metadata->setProperty("schema", 1); metadata->setProperty("name", activeTake.getFileName());
    metadata->setProperty("created", juce::Time::getCurrentTime().toISO8601(true));
    metadata->setProperty("frames", diskFrames); metadata->setProperty("sampleRate", recordingRate.load());
    metadata->setProperty("incomplete", recordingFault.load() == 1 || recordingFault.load() == 2);
    metadata->setProperty("recordingState","finished"); metadata->setProperty("checkpointFrames",checkpointFrames.load());
    metadata->setProperty("backingSpeed", playbackSpeed.load());
    bool saved=false;
    try { TakeRecovery::writeMetadata(activeTake.getChildFile("Cassian take.json"),juce::var(metadata.release())); saved=true; }
    catch (const std::exception&) { /* The initial incomplete journal is retained. */ }
    if (!saved) { const juce::ScopedLock lock(control); error = "Audio saved, but take metadata could not be saved."; }
    if (onTakeFinished) onTakeFinished(activeTake);
    activeTake = {};
    recordingLock.reset();
}
void PracticeEngine::run()
{
    while (!threadShouldExit()) {
        juce::File file, destination; unsigned generation = 0; bool loading = false, recording = false, preservePosition = false; double speed = 1;
        { const juce::ScopedLock lock(control);
          if (trackPending) { file = pendingTrack; generation = loadGeneration; speed = requestedSpeed.load(); preservePosition = preserveTrackPosition; trackPending = false; loading = true; }
          if (recordPending) { destination = pendingRecording; recordPending = false; recording = true; } }
        try {
            if (loading) readTrack(file, generation, speed, preservePosition);
            if (recording) beginRecording(destination);
        } catch (const std::exception& e) {
            const juce::ScopedLock lock(control);
            if (!loading || generation == loadGeneration) error = "Practice file error: " + juce::String(e.what());
            if (generation == loadGeneration) { loadingTrack.store(false); requestedSpeed.store(playbackSpeed.load()); }
            if (recording) recordMode.store(4);
        }
        drainRecording(); reclaimTracks();
        if (ownedTrack && ownedTrack->stream) {
            auto& stream = *ownedTrack->stream;
            try { stream.service([&] { return threadShouldExit() || loadingTrack.load(); }); }
            catch (const std::exception& e) { const juce::ScopedLock lock(control); error = e.what(); playing.store(false); startRequested.store(false); }
            if (stream.hasFailed()) { const juce::ScopedLock lock(control); error = stream.failure(); playing.store(false); countActive.store(false); startRequested.store(false); }
        }
        if (recordMode.load() == 4 && callbacks.load() == 0) {
            finishTake();
            if (recordingFault.load() != 0) {
                const juce::ScopedLock lock(control);
                error = recordingFault.load() == 3 ? "Recording stopped at the take duration/size limit. The take has been saved."
                    : recordingFault.load() == 2 ? "Recording stopped: guitar processing was interrupted. Check the incomplete take before using it."
                    : "Recording stopped: disk write failed or recording buffer filled. Check the incomplete take before using it.";
            }
            recordMode.store(0);
        }
        wait(ownedTrack && ownedTrack->stream ? (transportActive() ? 2 : 20) : recordMode.load() != 0 || !retired.empty() ? 10 : 250);
    }
    // Destruction is off the audio thread and flushes the remaining FIFO/header.
    finishTake();
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
        if (starting && source && position >= source->duration) position = 0;
    }
    const auto requestedSeek = seek.exchange(-1);
    if (requestedSeek >= 0) position = requestedSeek;
    const bool looping = loop.load(); const double a = loopA.load(), b = loopB.load();
    std::optional<ReviewStream::Read> streamRead;
    if (source && source->stream) {
        source->stream->request(static_cast<juce::int64>(position * source->rate));
        source->stream->requestLoop(looping && b > a && b <= source->duration ? static_cast<juce::int64>(a * source->rate) : -1);
        streamRead.emplace(*source->stream);
    }
    gain.setTargetValue(juce::Decibels::decibelsToGain(levelDb.load()));
    if (!playing.load() && !countActive.load() && recordMode.load() != 2 && recordMode.load() != 3) {
        gain.skip(output.getNumSamples()); reportedPosition.store(position);
        streamRead.reset(); hazard.store(nullptr); callbacks.fetch_sub(1); return false;
    }
    const bool counted = countActive.load() && countLength > 0;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0, written = 0;
    if (recordMode.load() == 2 || recordMode.load() == 3)
        fifo.prepareToWrite(output.getNumSamples(), start1, size1, start2, size2);
    const double fadeSeconds = loopFadeMs.load() * .001;
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
        int recordingIndex = -1;
        if (recordMode.load() == 3) {
            if (recordedFrames.load() + written >= frameLimit) { recordingFault.store(3); recordMode.store(4); }
            else if (written >= size1 + size2) { recordingFault.store(1); recordMode.store(4); }
            else {
                const auto index = written < size1 ? start1 + written : start2 + written - size1;
                recordingIndex = index;
                recordingAudio.setSample(0, index, std::isfinite(dry[i]) ? dry[i] : 0);
                for (int ch = 0; ch < 2; ++ch) {
                    const auto x = output.getSample(juce::jmin(ch, output.getNumChannels() - 1), i);
                    recordingAudio.setSample(ch + 1, index, std::isfinite(x) ? x : 0);
                    recordingAudio.setSample(ch + 3, index, 0);
                }
                ++written;
            }
        }
        const float volume = gain.getNextValue();
        if (source && playing.load()) {
            const double end = source->duration, frameRate = source->rate / source->speed;
            if (looping && b > a && b <= end && position >= b) position = a + std::fmod(position - a, b - a);
            if (position >= end) { playing.store(false); position = end; }
            else {
                const double frame = position * frameRate;
                const bool validLoop = looping && b > a && b <= end;
                float edgeGain = 1;
                if (validLoop && fadeSeconds > 0 && position >= a) {
                    const double width = juce::jmin(fadeSeconds * source->speed, (b - a) * .25);
                    const double distance = juce::jmin(position - a, b - position);
                    if (distance < width) {
                        const double edge = juce::jlimit(0., 1., distance / width);
                        edgeGain = static_cast<float>(.5 - .5 * std::cos(juce::MathConstants<double>::pi * edge));
                    }
                }
                if (source->stream) {
                    float left = 0, right = 0;
                    const auto index = juce::jlimit<juce::int64>(0, source->stream->frameCount() - 1, static_cast<juce::int64>(frame));
                    const auto alternate = validLoop && (index + 1) / frameRate >= b ? static_cast<juce::int64>(a * frameRate) : -1;
                    if (streamRead->sample(index, static_cast<float>(frame - index), left, right, alternate)) {
                        if (output.getNumChannels() > 0) output.addSample(0, i, volume * edgeGain * left);
                        if (output.getNumChannels() > 1) output.addSample(1, i, volume * edgeGain * right);
                        position += 1. / sampleRate;
                        if (!validLoop && position >= end) { position = end; playing.store(false); }
                    } // Underruns hold the cursor and never skip or reuse stale audio.
                    else if (source->stream->hasFailed()) playing.store(false);
                } else {
                const int index = juce::jlimit(0, source->audio.getNumSamples() - 1, static_cast<int>(frame));
                const float fraction = static_cast<float>(frame - index);
                int next = juce::jmin(index + 1, source->audio.getNumSamples() - 1);
                if (validLoop && (index + 1) / frameRate >= b) next = juce::jlimit(0, source->audio.getNumSamples() - 1, static_cast<int>(a * frameRate));
                for (int ch = 0; ch < 2; ++ch) {
                    const auto* samples = source->audio.getReadPointer(ch);
                    const auto x = volume * edgeGain * (samples[index] + fraction * (samples[next] - samples[index]));
                    if (ch < output.getNumChannels()) output.addSample(ch, i, x);
                    if (recordingIndex >= 0) recordingAudio.setSample(ch + 3, recordingIndex, std::isfinite(x) ? x : 0);
                }
                position += source->speed / sampleRate;
                if ((!looping || b <= a || b > end) && position >= end) { position = end; playing.store(false); }
                }
            }
        }
        for (int ch = 0; ch < output.getNumChannels(); ++ch) output.addSample(ch, i, click);
    }
    if (written > 0) { fifo.finishedWrite(written); recordedFrames.fetch_add(written); }
    if (source && source->stream) source->stream->request(static_cast<juce::int64>(position * source->rate));
    streamRead.reset(); reportedPosition.store(position); hazard.store(nullptr); callbacks.fetch_sub(1);
    return counted;
}
juce::var PracticeEngine::status()
{
    auto o = std::make_unique<juce::DynamicObject>();
    { const juce::ScopedLock lock(control); o->setProperty("track", trackName); o->setProperty("error", error); o->setProperty("takePath", takePath);
      o->setProperty("waveRevision", waveRevision); o->setProperty("sections", sectionRows); o->setProperty("sectionRevision", sectionRevision); o->setProperty("sectionError", sectionError); }
    { const juce::ScopedLock lock(control); const auto* stream = ownedTrack ? ownedTrack->stream.get() : nullptr;
      o->setProperty("streaming", stream != nullptr); o->setProperty("loopAvailable", true);
      o->setProperty("loopPrefetchReady", !stream || stream->loopReady());
      o->setProperty("buffering", stream && stream->isBuffering() && transportActive()); o->setProperty("reviewUnderruns", stream ? stream->underrunCount() : 0);
      o->setProperty("reviewCacheBytes", stream ? ReviewStream::cacheBytes : 0); }
    o->setProperty("loading", loadingTrack.load());
    o->setProperty("speed", playbackSpeed.load()); o->setProperty("requestedSpeed", requestedSpeed.load()); o->setProperty("loadProgress", loadProgress.load()); o->setProperty("fade", loopFadeMs.load());
    o->setProperty("duration", duration.load()); o->setProperty("position", reportedPosition.load());
    o->setProperty("playing", playing.load()); o->setProperty("counting", countActive.load()); o->setProperty("countBeat", countBeat.load());
    o->setProperty("starting", startRequested.load());
    o->setProperty("recordMode", recordMode.load()); o->setProperty("recordSeconds", recordedFrames.load() / recordingRate.load());
    o->setProperty("recordingCheckpointSeconds",checkpointFrames.load() / recordingRate.load());
    o->setProperty("level", levelDb.load()); o->setProperty("loop", loop.load()); o->setProperty("a", loopA.load()); o->setProperty("b", loopB.load());
    return juce::var(o.release());
}
juce::var PracticeEngine::waveform()
{
    const juce::ScopedLock lock(control); auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty("revision", waveRevision); o->setProperty("trackId", trackKey); o->setProperty("duration", duration.load()); o->setProperty("peaks", wavePeaks);
    return juce::var(o.release());
}
bool PracticeEngine::sectionsBlocked() const
{ return duration.load() <= 0 || loadingTrack.load() || recordMode.load() != 0 || countActive.load() || startRequested.load(); }
juce::String PracticeEngine::saveSection(const juce::String& name, const juce::String& id)
{
    const juce::ScopedLock lock(control);
    return saveSectionRange(name, id, loopA.load(), loopB.load());
}
juce::String PracticeEngine::saveSectionRange(const juce::String& name, const juce::String& id, double a, double b)
{
    const juce::ScopedLock lock(control);
    if (sectionsBlocked()) return "Load a track and finish preparation, the take or count-in before editing sections.";
    try { sectionRows = sections.change(trackKey, duration.load(), id, name, a, b, false); sectionError.clear(); ++sectionRevision; return {}; }
    catch (const std::exception& e) { return e.what(); }
}
juce::String PracticeEngine::removeSection(const juce::String& id)
{
    const juce::ScopedLock lock(control);
    if (sectionsBlocked()) return "Finish preparation, the take or count-in before editing sections.";
    try { sectionRows = sections.change(trackKey, duration.load(), id, {}, 0, 0, true); sectionError.clear(); ++sectionRevision; return {}; }
    catch (const std::exception& e) { return e.what(); }
}
juce::String PracticeEngine::recallSection(const juce::String& id)
{
    const juce::ScopedLock lock(control);
    if (sectionsBlocked()) return "Finish preparation, the take or count-in before recalling a section.";
    try {
        auto latest = sections.load(trackKey, duration.load());
        for (const auto& row : *latest.getArray()) if (row["id"].toString() == id) {
            const double a = row["a"], b = juce::jmin(duration.load(), static_cast<double>(row["b"]));
            playing.store(false); startRequested.store(false); startEpoch.fetch_add(1);
            loop.store(false); loopA.store(a); loopB.store(b); seek.store(a); loop.store(true);
            sectionRows = latest; sectionError.clear(); ++sectionRevision; return {};
        }
        sectionRows = latest; ++sectionRevision; return "That practice section no longer exists. Reload the track.";
    } catch (const std::exception& e) { return e.what(); }
}
