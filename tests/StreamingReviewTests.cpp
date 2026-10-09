#include "../Source/PracticeEngine.h"
#include "../Source/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <thread>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void waitFor(F ready) {
    for (int n = 0; n < 2000; ++n) { if (ready()) return; juce::Thread::sleep(5); }
    require(false, "Streaming review worker timed out");
}
void fixture(const juce::File& file, int channels, double rate, juce::int64 frames) {
    juce::WavAudioFormat format; auto stream = file.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), rate, static_cast<unsigned>(channels), 32, {}, 0));
    require(writer != nullptr, "Streaming fixture writer must open");
    juce::AudioBuffer<float> scratch(channels, 8192);
    for (juce::int64 offset = 0; offset < frames; offset += 8192) {
        const int n = static_cast<int>(juce::jmin<juce::int64>(8192, frames - offset));
        for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < n; ++i)
            scratch.setSample(ch, i, static_cast<float>(.3 * std::sin(juce::MathConstants<double>::twoPi * (ch ? 731 : 397) * (offset + i) / rate)));
        require(writer->writeFromAudioSampleBuffer(scratch, 0, n), "Streaming fixture must write");
    }
}
juce::AudioBuffer<float> render(PracticeEngine& engine, int n) {
    juce::AudioBuffer<float> audio(2, n); audio.clear(); engine.process(audio, audio.getReadPointer(0)); return audio;
}
void load(PracticeEngine& engine, const juce::File& file, double rate) {
    engine.command("level", 0); engine.prepare(rate); engine.setCountIn(0, 120, 4); engine.load(file);
    waitFor([&] { return !static_cast<bool>(engine.status()["loading"]); });
    require(engine.status()["error"].toString().isEmpty(), "Streaming fixture must load"); render(engine, 1);
}
// Wait using single-frame callbacks, then restore the requested point. This
// checks actual publication rather than assuming a sleep made a seek ready.
void at(PracticeEngine& engine, double seconds) {
    engine.command("pause"); engine.command("seek", seconds); engine.command("play");
    waitFor([&] { render(engine, 1); return !static_cast<bool>(engine.status()["buffering"]); });
    engine.command("pause"); engine.command("seek", seconds); engine.command("play");
}
class AuditedReader final : public juce::AudioFormatReader {
public:
    explicit AuditedReader(std::unique_ptr<juce::AudioFormatReader> r) : AudioFormatReader(nullptr, "audited"), source(std::move(r)), worker(std::this_thread::get_id()) {
        sampleRate = source->sampleRate; bitsPerSample = source->bitsPerSample; lengthInSamples = source->lengthInSamples;
        numChannels = source->numChannels; usesFloatingPointData = source->usesFloatingPointData;
    }
    bool readSamples(int* const* dest, int channels, int offset, juce::int64 start, int n) override {
        require(std::this_thread::get_id() == worker, "Reader IO must never occur on the playback callback thread"); ++reads;
        std::vector<int*> shifted(static_cast<size_t>(channels));
        for (int ch = 0; ch < channels; ++ch) shifted[ch] = dest[ch] ? dest[ch] + offset : nullptr;
        return !failReads && source->read(shifted.data(), channels, start, n, false);
    }
    bool failReads = false; int reads = 0;
private:
    std::unique_ptr<juce::AudioFormatReader> source; std::thread::id worker;
};
}
void runStreamingReviewChecks() {
    const auto base = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto folder = base.getNonexistentChildFile("Cassian-stream-tests-" + juce::Uuid().toString(), "", false);
    require(folder.createDirectory().wasOk(), "Stream test directory must create");
    struct Cleanup { juce::File folder, base; ~Cleanup() { if (folder.isAChildOf(base)) folder.deleteRecursively(); } } cleanup {folder, base};
    // Match the existing decoder, including fractional rate phase at distant
    // seeks, stereo orientation, block seams and the final output sample.
    for (double sourceRate : {44100., 48000., 96000.}) for (double targetRate : {44100., 48000., 96000.}) for (int channels : {1, 2}) {
        const auto file = folder.getChildFile("compare.wav"); fixture(file, channels, sourceRate, static_cast<juce::int64>(sourceRate * 2));
        PracticeEngine decoded, streamed(262144, {}, 0); load(decoded, file, targetRate); load(streamed, file, targetRate);
        require(static_cast<bool>(streamed.status()["streaming"]) && static_cast<bool>(streamed.status()["loopAvailable"]), "Streaming status must describe loop capabilities");
        require(static_cast<juce::int64>(streamed.status()["reviewCacheBytes"]) == ReviewStream::cacheBytes && ReviewStream::cacheBytes < 4 * 1024 * 1024, "Cache allocation must be fixed and bounded");
        for (double seconds : {0., (ReviewStream::blockFrames - 64) / targetRate, 1.417, 2. - 64 / targetRate}) {
            at(streamed, seconds); at(decoded, seconds);
            const auto a = render(decoded, 128), b = render(streamed, 128);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i)
                require(std::abs(a.getSample(ch, i) - b.getSample(ch, i)) < .00003f, "Streaming samples and seek phase must match decoded audio");
            require(std::abs(static_cast<double>(decoded.status()["position"]) - static_cast<double>(streamed.status()["position"])) < 1e-8, "Streamed and decoded cursors must align");
        }
        require(!static_cast<bool>(streamed.status()["playing"]), "Streaming EOF must stop without stale samples");
        require(!streamed.command("speed", .75).isEmpty(), "Unsupported streaming speed changes must fail explicitly");
        // Same-block tiny loops, distant boundaries and B exactly at EOF.
        // Compare fractional seam interpolation and fades against the
        // existing player at every mono/stereo source/interface rate pair.
        for (const auto& range : {std::pair<double,double>{.12345, .17378}, {.23147, 1.31791}, {.81, 2.}}) for (double fade : {0., 5.}) {
            for (auto* e : {&decoded, &streamed}) {
                e->command("pause"); e->command("a", range.first); e->command("b", range.second); e->command("fade", fade);
                require(e->command("loop", 1).isEmpty(), "Streamed loops must be available");
                e->command("seek", range.second - 40.375 / targetRate); render(*e, 1);
            }
            waitFor([&] { render(streamed, 1); return static_cast<bool>(streamed.status()["loopPrefetchReady"]); });
            at(streamed, range.second - 40.375 / targetRate); at(decoded, range.second - 40.375 / targetRate);
            const auto waits = static_cast<juce::int64>(streamed.status()["reviewUnderruns"]);
            const auto reference = render(decoded, 128), actual = render(streamed, 128);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i)
                require(std::abs(reference.getSample(ch,i) - actual.getSample(ch,i)) < .00003f, "Loop fades and fractional seam samples must match decoded review");
            require(static_cast<bool>(streamed.status()["playing"]) && std::abs(static_cast<double>(decoded.status()["position"]) - static_cast<double>(streamed.status()["position"])) < 1e-8,
                "Loop wrapping including EOF must preserve original-time cursor");
            require(static_cast<juce::int64>(streamed.status()["reviewUnderruns"]) == waits, "Prefetched loop boundary must not introduce a buffer wait");
        }
        streamed.command("loop", 0);
        streamed.command("stop"); render(streamed, 1); require(static_cast<double>(streamed.status()["position"]) == 0, "Stop must return to zero");
    }
    // A 25 MB mono source expands past the old 256 MiB limit at 48 kHz.
    const auto longFile = folder.getChildFile("long.wav"); fixture(longFile, 1, 8000, 6000000);
    {
        PracticeEngine engine(262144, folder.getChildFile("sections"), 256 * 1024 * 1024 / (2 * sizeof(float)));
        load(engine, longFile, 48000);
        require(static_cast<bool>(engine.status()["streaming"]) && static_cast<double>(engine.status()["duration"]) == 750, "Long source must load past the decoded-memory limit");
        engine.command("seek", 50); engine.command("seek", 100); engine.command("seek", 749.5); render(engine, 1);
        require(static_cast<double>(engine.status()["position"]) == 749.5, "Latest replaced seek must win");
        at(engine, 749.5); require(render(engine, 128).getMagnitude(0, 128) > .1f, "Long EOF-area seek must produce real audio");
        engine.command("pause"); render(engine, 1); const double paused = engine.status()["position"]; render(engine, 512);
        require(static_cast<double>(engine.status()["position"]) == paused, "Pause must hold the streamed cursor");
        engine.command("a", 700); engine.command("b", 710); require(engine.saveSection("Ending").isEmpty(), "Long-take export ranges must save");
        const auto section = engine.status()["sections"][0]["id"].toString(); require(engine.recallSection(section).isEmpty(), "Long-take sections must recall"); render(engine, 1);
        require(static_cast<double>(engine.status()["position"]) == 700 && static_cast<bool>(engine.status()["loop"]), "Streaming section recall must seek and enable looping");
        engine.command("a", 100.125); engine.command("b", 749.125); engine.command("seek", 749.124); render(engine, 1);
        waitFor([&] { render(engine, 1); return static_cast<bool>(engine.status()["loopPrefetchReady"]); });
        at(engine, 749.124); const auto before = static_cast<juce::int64>(engine.status()["reviewUnderruns"]); render(engine, 128);
        require(static_cast<double>(engine.status()["position"]) < 100.13 && static_cast<juce::int64>(engine.status()["reviewUnderruns"]) == before, "Distant long-take loop must wrap into the reserved loop-head cache");
        engine.command("pause"); engine.command("loop", 0); render(engine, 1);
        engine.command("a", 200); engine.command("b", 200.051); engine.command("loop", 1); engine.command("seek", 200); render(engine, 1);
        waitFor([&] { render(engine, 1); return static_cast<bool>(engine.status()["loopPrefetchReady"]); });
        at(engine, 200); const auto tinyBefore = static_cast<juce::int64>(engine.status()["reviewUnderruns"]);
        for (int n = 0; n < 100; ++n) render(engine, 128);
        require(static_cast<double>(engine.status()["position"]) >= 200 && static_cast<double>(engine.status()["position"]) < 200.051 && static_cast<juce::int64>(engine.status()["reviewUnderruns"]) == tinyBefore,
            "Repeated short loops must remain in range without starving pinned slots");
        engine.load(folder.getChildFile("missing.wav")); waitFor([&] { return !static_cast<bool>(engine.status()["loading"]); });
        require(static_cast<bool>(engine.status()["streaming"]) && engine.status()["track"].toString() == "long.wav", "Failed replacement must retain the previous stream");
        const auto shortFile = folder.getChildFile("replacement.wav"); fixture(shortFile, 2, 48000, 48000);
        engine.load(longFile); engine.command("cancelLoad"); engine.load(shortFile); waitFor([&] { return !static_cast<bool>(engine.status()["loading"]); });
        require(engine.status()["track"].toString() == "replacement.wav" && !static_cast<bool>(engine.status()["streaming"]), "Replacement must win over cancelled long preparation");
    }
    // Deliberately withhold the worker. Callback stays silent at the exact
    // cursor, counts one interruption, resumes only after publication, and
    // never calls the reader even when the requested range is missing.
    {
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        auto audited = std::make_unique<AuditedReader>(std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(longFile))); auto* reader = audited.get();
        ReviewStream stream(longFile, std::move(audited), 48000); stream.validate();
        stream.service([] { return false; }); const auto reads = reader->reads;
        stream.request(48000 * 700);
        bool silence = true;
        std::thread callback([&] { ReviewStream::Read read(stream); float l = 99, r = 99;
            for (int n = 0; n < 128; ++n) silence &= !read.sample(48000 * 700, 0, l, r);
        }); callback.join();
        require(silence && reader->reads == reads && stream.underrunCount() == 1 && stream.isBuffering(), "Underrun must be silent, counted once and free of reader IO");
        require(stream.service([] { return false; }), "Worker must refill the requested range");
        bool resumed = false; std::thread resume([&] { ReviewStream::Read read(stream); float l = 0, r = 0; resumed = read.sample(48000 * 700, 0, l, r) && std::isfinite(l) && l == r; }); resume.join();
        require(resumed && !stream.isBuffering() && reader->reads > reads, "Worker refill must resume mono duplication");
        // Hold a slot across concurrent eviction requests. The worker must
        // never rewrite a Reading block, even after its range becomes old.
        std::atomic<bool> held {false}, release {false}, stable {true};
        std::thread pin([&] { ReviewStream::Read read(stream); float first = 0, right = 0;
            if (!read.sample(48000 * 700 + 11, 0, first, right)) stable.store(false);
            held.store(true);
            while (!release.load()) { float l = 0, r = 0; if (!read.sample(48000 * 700 + 11, 0, l, r) || l != first) stable.store(false); std::this_thread::yield(); }
        });
        waitFor([&] { return held.load(); });
        for (int n = 0; n < 30; ++n) { stream.request(48000 * (n * 11)); stream.service([] { return false; }); }
        release.store(true); pin.join(); require(stable.load(), "Cache eviction must preserve samples held by a callback");
        stream.request(48000 * 300); require(stream.service([] { return false; }), "Seam current range must prepare");
        const juce::int64 tail = 48000 * 300 + 111, head = 48000 * 651 + 111;
        stream.requestLoop(head); require(!stream.ready(head), "Loop-head withholding fixture must be uncached");
        const auto seamWaits = stream.underrunCount(); const int readsBeforeSeam = reader->reads;
        for (int attempt = 0; attempt < 3; ++attempt) {
            bool missing = false; std::thread seam([&] { ReviewStream::Read read(stream); float l=0,r=0; missing = !read.sample(tail, .5f, l, r, head); }); seam.join();
            require(missing, "Uncached seam must wait without replaying a stale range");
        }
        require(stream.underrunCount() == seamWaits + 1 && reader->reads == readsBeforeSeam, "A continuous seam wait must count once without callback IO");
        for (int n = 0; n < 5; ++n) stream.service([] { return false; }); require(stream.loopReady() && stream.ready(head), "Loop head must be prefetched within the same cache budget");
        bool paired = false;
        std::thread wrap([&] { float l=0,r=0, start=0,sr=0,end=0,er=0;
            { ReviewStream::Read read(stream); read.sample(head,0,start,sr); }
            ReviewStream::Read read(stream); read.sample(tail,0,end,er);
            paired = read.sample(tail,.5f,l,r,head) && std::abs(l - .5f * (start + end)) < 1e-6f && read.sample(head,0,l,r);
        }); wrap.join(); require(paired, "A distant fractional seam must interpolate the true loop head and release its temporary claim before wrap");
        for (int n = 0; n < 24; ++n) stream.service([] { return false; });
        stream.request(48000 * 500); for (int n = 0; n < 24; ++n) stream.service([] { return false; });
        require(stream.ready(head), "Moving playback window must retain the reserved loop head");
        stream.requestLoop(-1);
        reader->failReads = true; stream.request(48000 * 350); stream.service([] { return false; });
        require(stream.hasFailed() && stream.failure().isNotEmpty(), "Reader failure must stop streaming with an explanation");
    }
    {
        // Review bypasses live guitar and clicks at the real processor mix
        // point, including the normal streaming path and an uncached seek.
        AmpSuiteAudioProcessor processor(false);
        auto* master = processor.apvts.getParameter("MASTER_VOL"); master->setValueNotifyingHost(master->convertTo0to1(0));
        auto* click = processor.apvts.getParameter("METRO_ON"); click->setValueNotifyingHost(click->convertTo0to1(1));
        processor.prepareToPlay(48000, 128);
        load(processor.takeReview, longFile, 48000);
        processor.takeReview.command("play");
        juce::AudioBuffer<float> audio(2, 128); juce::MidiBuffer midi;
        const auto play = [&] { for (int ch = 0; ch < 2; ++ch) juce::FloatVectorOperations::fill(audio.getWritePointer(ch), .8f, 128); processor.processBlock(audio, midi); };
        play(); require(audio.getMagnitude(0, 128) <= .301f, "Streamed review must mute the live guitar path");
        processor.takeReview.command("seek", 700); play();
        // Even when the first request buffers, incoming guitar must not leak
        // into the paused review cursor or overwhelm resumed cached audio.
        require(audio.getMagnitude(0, 128) <= .301f, "Buffering review must keep live-guitar muting active");
        waitFor([&] { play(); return !static_cast<bool>(processor.takeReview.status()["buffering"]); });
        require(audio.getMagnitude(0, 128) > .1f, "Streamed processor review must resume audible audio");
        processor.releaseResources();
    }
    {
        const auto began = juce::Time::getMillisecondCounter();
        { PracticeEngine interrupted(262144, {}, 0); interrupted.prepare(48000); interrupted.load(longFile); }
        require(juce::Time::getMillisecondCounter() - began < 5000, "Shutdown must cancel long waveform/hash preparation promptly");
    }
    {
        // JUCE can zero-fill a truncated RIFF payload; independent physical
        // bounds must refuse it before it becomes a reviewed take.
        const auto broken = folder.getChildFile("truncated.wav"); require(longFile.copyFileTo(broken), "Truncation fixture must copy");
        { juce::FileOutputStream output(broken); require(output.setPosition(broken.getSize() - 100) && output.truncate().wasOk(), "Fixture must truncate"); }
        PracticeEngine engine(262144, {}, 0); engine.prepare(48000); engine.load(broken);
        waitFor([&] { return !static_cast<bool>(engine.status()["loading"]); });
        require(engine.status()["error"].toString().isNotEmpty() && static_cast<double>(engine.status()["duration"]) == 0, "Truncated source must not publish silence as successful audio");
    }
    {
        PracticeEngine engine(262144, {}, 0); load(engine, longFile, 48000);
        require(longFile.deleteFile(), "Changed-media fixture must delete");
        waitFor([&] { return engine.status()["error"].toString().isNotEmpty(); });
        require(!engine.command("play").isEmpty(), "Changed/deleted audio must block replay until reload");
    }
    std::cout << "Bounded streaming review: resampling/seek/EOF, long WAV, cancellation, sections, callback IO guard, underrun and media-failure checks passed\n";
}
