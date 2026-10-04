#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
template <typename Predicate> void waitFor(Predicate ready) {
    for (int n = 0; n < 1000; ++n) { if (ready()) return; juce::Thread::sleep(5); }
    require(false, "Practice worker timed out");
}
void wav(const juce::File& path, int channels, int frames, double rate) {
    juce::AudioBuffer<float> audio(channels, frames);
    for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < frames; ++i) audio.setSample(ch, i, ch == 0 ? .4f : -.2f);
    juce::WavAudioFormat format; auto stream = path.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), rate, static_cast<unsigned>(channels), 32, {}, 0));
    require(writer && writer->writeFromAudioSampleBuffer(audio, 0, frames), "Practice fixture write failed");
}
void loaded(PracticeEngine& e, const juce::File& path) {
    e.load(path); waitFor([&] { return !static_cast<bool>(e.status()["loading"]); });
    require(e.status()["error"].toString().isEmpty(), "Backing fixture must load");
}
juce::AudioBuffer<float> render(PracticeEngine& e, int n, float wet = 0) {
    juce::AudioBuffer<float> audio(2, n), dry(1, n);
    for (int i = 0; i < n; ++i) { dry.setSample(0, i, .125f); audio.setSample(0, i, wet); audio.setSample(1, i, -wet * 2); }
    e.process(audio, dry.getReadPointer(0)); return audio;
}
juce::AudioBuffer<float> read(const juce::File& path, int channels, int frames) {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(path));
        require(reader && reader->numChannels == static_cast<unsigned>(channels) && reader->lengthInSamples == frames && reader->usesFloatingPointData && reader->sampleRate == 48000, "Take WAV headers, channel layout and frame counts must match");
    juce::AudioBuffer<float> audio(channels, frames); require(reader->read(&audio, 0, frames, 0, true, true), "Take WAV must read"); return audio;
}
void set(AmpSuiteAudioProcessor& p, const char* id, float x) { auto* param = p.apvts.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(x)); }
}

void runPracticeChecks()
{
    const auto base = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto folder = base.getNonexistentChildFile("Cassian-practice-tests-" + juce::Uuid().toString(), "", false);
    require(folder.createDirectory().wasOk(), "Practice test directory must create");
    struct Cleanup { juce::File folder, base; ~Cleanup() { if (folder.isAChildOf(base)) folder.deleteRecursively(); } } cleanup {folder, base};
    const auto stereo = folder.getChildFile("stereo.wav"), mono = folder.getChildFile("mono.wav");
    wav(stereo, 2, 96000, 48000); wav(mono, 1, 44100, 44100);
    // Track level, sample-rate conversion, channel duplication, pause, seek, EOF,
    // and a loop boundary inside a variable-sized callback.
    for (double rate : {44100., 48000., 96000.}) {
        PracticeEngine e; e.prepare(rate); e.setCountIn(0, 120, 4); e.command("level", 0); loaded(e, stereo);
        render(e, static_cast<int>(rate * .025)); // settle gain and consume seek
        require(e.command("play").isEmpty(), "Track playback must start");
        const auto out = render(e, 173);
        require(std::abs(out.getSample(0, 100) - .4f) < .002f && std::abs(out.getSample(1, 100) + .2f) < .002f, "Backing must preserve stereo and level at all interface rates");
        e.command("pause"); const double at = e.status()["position"];
        require(render(e, 511).getMagnitude(0, 0, 511) == 0 && static_cast<double>(e.status()["position"]) == at, "Pause must freeze position and output silence");
        require(e.command("loop", 1).isEmpty(), "Full-track loop must be valid");
        e.command("a", .1); e.command("b", .2); e.command("seek", .199); e.command("play"); render(e, static_cast<int>(rate * .01));
        const double loopPosition = e.status()["position"];
        require(loopPosition >= .108 && loopPosition <= .11, "Section loop must wrap sample accurately inside the callback");
        e.command("loop", 0); e.command("seek", 1.999); render(e, 511);
        require(!static_cast<bool>(e.status()["playing"]), "EOF must stop without reading beyond the track");
        e.command("stop"); render(e, 37); require(static_cast<double>(e.status()["position"]) == 0, "Stop must rewind");
        loaded(e, mono); require(std::abs(static_cast<double>(e.status()["duration"]) - 1) < .0001, "Rate conversion must preserve duration");
        e.command("play"); const auto duplicated = render(e, 511);
        require(std::abs(duplicated.getSample(0, 100) - duplicated.getSample(1, 100)) < .000001f, "Mono backing must duplicate to both output channels");
        e.load(folder.getChildFile("missing.wav")); waitFor([&] { return !static_cast<bool>(e.status()["loading"]); });
        require(e.status()["error"].toString().isNotEmpty() && e.status()["track"].toString() == "mono.wav", "Failed track load must preserve the previous track and report its error");
    }
    // A one-beat bar at 240 BPM is exactly 12,000 samples. The same callback
    // crosses from count-in to recording/backing without recording any click.
    {
        PracticeEngine e; e.prepare(48000); loaded(e, stereo); e.command("level", 0); render(e, 1024);
        e.setCountIn(1, 240, 1); require(e.record(folder).isEmpty(), "Record must arm both files");
        waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 2; });
        int total = 0, block = 0; constexpr int captured = 777, count = 12000; bool audibleClick = false;
        const int sizes[] {37, 128, 511};
        while (total < count + captured) {
            const int n = juce::jmin(sizes[block++ % 3], count + captured - total);
            const auto out = render(e, n, 1.25f);
            if (total < count - 1000 && out.getSample(0, juce::jmin(n - 1, 10)) != 1.25f) audibleClick = true;
            total += n;
        }
        require(audibleClick && static_cast<int>(e.status()["recordMode"]) == 3, "Count-in must sound and transition to recording");
        require(std::abs(static_cast<double>(e.status()["position"]) - captured / 48000.) < .000001, "Backing must start exactly when recording starts");
        require(!e.command("seek", .5).isEmpty(), "Seeking during a take must be rejected");
        e.command("pause"); waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 0; });
        const juce::File take(e.status()["takePath"].toString());
        const auto dry = read(take.getChildFile("Guitar dry.wav"), 1, captured);
        const auto wet = read(take.getChildFile("Guitar processed.wav"), 2, captured);
        for (int i = 0; i < captured; ++i) {
            require(dry.getSample(0, i) == .125f, "Dry take must preserve raw input before any processing");
            require(wet.getSample(0, i) == 1.25f && wet.getSample(1, i) == -2.5f, "Processed float take must exclude backing/click and preserve headroom");
        }
        // The next take uses a new directory; stop can cancel its count-in.
        require(e.record(folder).isEmpty(), "Next take must re-arm after draining");
        waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 2; }); e.command("stop");
        waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 0; });
        require(e.status()["takePath"].toString() != take.getFullPathName(), "Takes must never overwrite one another");
        const auto silent = render(e, 173); require(!static_cast<bool>(e.status()["counting"]) && silent.getMagnitude(0, 0, 173) == 0, "Stop must cancel an armed count-in");
    }
    // Guitar-only recording, device re-preparation and writer finalization do not
    // depend on a backing track or a subsequent audio callback.
    {
        PracticeEngine e; e.prepare(48000); e.setCountIn(0, 120, 4);
        require(e.record(folder).isEmpty(), "Guitar-only recording must arm");
        waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 2; }); render(e, 257, .2f);
        e.prepare(96000); waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 0; });
        const juce::File take(e.status()["takePath"].toString());
        const auto dry = read(take.getChildFile("Guitar dry.wav"), 1, 257);
        require(dry.getSample(0, 256) == .125f && std::abs(static_cast<double>(e.status()["recordSeconds"]) - 257 / 48000.) < .000001, "Device change must preserve the take's original rate and duration");
        require(!static_cast<bool>(e.status()["playing"]) && !static_cast<bool>(e.status()["counting"]), "Device change must not resume transport");
    }
    // A deliberately tiny FIFO makes overflow deterministic without a slow disk.
    {
        PracticeEngine e(32); e.prepare(48000); e.setCountIn(0, 120, 4); e.record(folder);
        waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 2; }); render(e, 128, .2f);
        waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 0; });
        require(e.status()["error"].toString().contains("Recording stopped"), "Recording overflow must stop and report an incomplete take");
    }
    // Never capture backing audio during a skipped guitar-processing callback.
    {
        PracticeEngine e; e.prepare(48000); e.setCountIn(0, 120, 4); e.record(folder);
        waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 2; });
        juce::AudioBuffer<float> audio(2, 128); audio.clear();
        e.process(audio, audio.getReadPointer(0), false);
        waitFor([&] { return static_cast<int>(e.status()["recordMode"]) == 0; });
        require(e.status()["error"].toString().contains("interrupted"), "An unavailable guitar path must stop the take instead of recording auxiliary audio");
    }
    // Exercise the processor's actual raw-DI tap before software calibration.
    {
        AmpSuiteAudioProcessor p(false); set(p, "AMP_SOURCE", 4); set(p, "CAB_MODE", 3); set(p, "INPUT_GAIN", 6);
        set(p, "GATE_ON", 0); set(p, "REVERB_MIX", 0); set(p, "DELAY_MIX", 0); p.prepareToPlay(48000, 128);
        p.practice.setCountIn(0, 120, 4); p.practice.record(folder);
        waitFor([&] { return static_cast<int>(p.practice.status()["recordMode"]) == 2; });
        juce::MidiBuffer midi; juce::AudioBuffer<float> input(2, 128);
        for (int block = 0; block < 8; ++block) {
            input.clear(); for (int i = 0; i < 128; ++i) input.setSample(0, i, .125f); p.processBlock(input, midi);
        }
        p.practice.command("pause"); waitFor([&] { return static_cast<int>(p.practice.status()["recordMode"]) == 0; });
        const juce::File take(p.practice.status()["takePath"].toString());
        const auto dry = read(take.getChildFile("Guitar dry.wav"), 1, 1024), wet = read(take.getChildFile("Guitar processed.wav"), 2, 1024);
        require(dry.getSample(0, 1000) == .125f, "Processor dry tap must precede software Input gain");
        // Inspect after the existing 20 ms gain ramps settle; the legacy reverb
        // dry path retains its 2x scale even at zero mix.
        require(std::abs(wet.getSample(0, 1000) - .125f * 2 * juce::Decibels::decibelsToGain(6.f)) < .0001f && std::abs(wet.getSample(1, 1000) - wet.getSample(0, 1000)) < .0001f, "Processed tap must include calibration and stereo guitar but exclude Master");
    }
    // The integrated backing player bypasses the overdrive, high-gain amp and cab.
    {
        AmpSuiteAudioProcessor p(false); set(p, "AMP_SOURCE", 2); set(p, "OD_ON", 1); set(p, "OD_DRIVE", 100);
        set(p, "GATE_ON", 0); set(p, "DRIVE_GAIN", 20); set(p, "REVERB_MIX", 0); set(p, "DELAY_MIX", 0); set(p, "MASTER_VOL", -12);
        p.prepareToPlay(48000, 128); p.practice.command("level", 0); p.practice.setCountIn(0, 120, 4); loaded(p.practice, stereo);
        juce::AudioBuffer<float> audio(2, 128); juce::MidiBuffer midi;
        for (int i = 0; i < 20; ++i) { audio.clear(); p.processBlock(audio, midi); }
        p.practice.command("play");
        for (int i = 0; i < 20; ++i) { audio.clear(); p.processBlock(audio, midi); }
        // Account for the existing JUCE output limiter's makeup gain. The oracle
        // contains only a backing mix and output protection, no guitar stages.
        juce::dsp::Limiter<float> protection; protection.prepare({48000, 128, 2}); protection.setThreshold(-.5f); protection.setRelease(60);
        juce::AudioBuffer<float> reference(2, 128);
        for (int n = 0; n < 40; ++n) {
            for (int i = 0; i < 128; ++i) { reference.setSample(0, i, n < 20 ? 0 : .4f * juce::Decibels::decibelsToGain(-12.f)); reference.setSample(1, i, n < 20 ? 0 : -.2f * juce::Decibels::decibelsToGain(-12.f)); }
            juce::dsp::AudioBlock<float> block(reference); juce::dsp::ProcessContextReplacing<float> context(block); protection.process(context);
        }
        const auto expected = reference.getSample(0, 100);
        require(std::abs(audio.getSample(0, 100) - expected) < .003f && std::abs(audio.getSample(1, 100) + expected * .5f) < .003f, "Integrated backing must bypass high-gain processing and respect Master");
    }
    std::cout << "Practice checks passed\n";
}
