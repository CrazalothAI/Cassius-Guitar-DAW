#include "../Source/PluginProcessor.h"
#include <algorithm>
#include <complex>
#include <iostream>
#include <numeric>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void set(AmpSuiteAudioProcessor& p, const char* id, float x) { auto* param = p.apvts.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(x)); }
void ready(AmpSuiteAudioProcessor& p) {
    for (int i = 0; i < 600; ++i) {
        if (p.status()["message"].toString().startsWith("Load failed:")) throw std::runtime_error(p.status()["message"].toString().toStdString());
        if (!p.getRig().hasProperty("error")) return;
        juce::Thread::sleep(5);
    }
    require(false, "Sound update assets did not load");
}
void neutral(AmpSuiteAudioProcessor& p) { set(p, "AMP_SOURCE", 4); set(p, "CAB_MODE", 3); set(p, "REVERB_MIX", 0); set(p, "GATE_ON", 0); set(p, "MASTER_VOL", -24); }
juce::AudioBuffer<float> impulse(int offset = 0) {
    juce::AudioBuffer<float> x(2, 128); x.clear(); x.setSample(0, offset, .8f); x.setSample(1, offset, .8f); return x;
}
void writeWav(const juce::File& file, const juce::AudioBuffer<float>& x, double rate) {
    juce::WavAudioFormat format; auto output = file.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(output.release(), rate, static_cast<unsigned>(x.getNumChannels()), 24, {}, 0));
    require(writer && writer->writeFromAudioSampleBuffer(x, 0, x.getNumSamples()), "Sound fixture must write");
}
juce::ValueTree stateOf(juce::var rig) { auto xml = juce::XmlDocument::parse(rig["state"].toString()); require(xml != nullptr, "Sound rig must parse"); return juce::ValueTree::fromXml(*xml); }
juce::var wrap(const juce::ValueTree& state) { auto o = std::make_unique<juce::DynamicObject>(); o->setProperty("schema", 1); o->setProperty("state", state.createXml()->toString()); return juce::var(o.release()); }
// Synthetic picked phrases for repeatable regression/benchmarks, not listening validation.
float phrase(int n, double rate) {
    const double t = n / rate, position = std::fmod(t, .25), notes[] {110, 164.8138, 220, 329.6276, 440, 587.3295, 659.2551, 880};
    const double hz = notes[static_cast<int>(t * 4) % 8];
    return static_cast<float>(.09 * std::exp(-position * 9) * (std::sin(juce::MathConstants<double>::twoPi * hz * t) + .3 * std::sin(juce::MathConstants<double>::twoPi * hz * 2 * t)));
}
}

void runSoundChecks(const juce::File& fixture)
{
    for (double rate : {44100., 48000., 96000.}) {
        // Known static ratio and linked stereo: a louder right channel must reduce both.
        StudioCompressor comp; comp.prepare(rate, {100, -20, 4, 1, 100, 0});
        juce::AudioBuffer<float> x(2, 128);
        for (int b = 0; b < 100; ++b) { for (int i = 0; i < 128; ++i) { x.setSample(0, i, .2f); x.setSample(1, i, .4f); } comp.process(x.getArrayOfWritePointers(), 2, 128); }
        require(std::abs(x.getSample(1, 127) / x.getSample(0, 127) - 2) < .001f, "Compression must preserve stereo balance");
        const float expected = .4f * juce::Decibels::decibelsToGain(-.75f * (juce::Decibels::gainToDecibels(.4f) + 20));
        require(std::abs(x.getSample(1, 127) - expected) < .002f && comp.reductionDb() > 8, "Universal compressor must apply its specified static ratio");
        comp.configure({0, -20, 4, 1, 100, 12});
        for (int b = 0; b < 100; ++b) { for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) x.setSample(ch, i, .02f); comp.process(x.getArrayOfWritePointers(), 2, 128); }
        require(x.getSample(0, 0) == .02f, "Compressor zero mix must be sample-exact even with makeup gain");

        // Distortion must generate harmonics, rather than just turn up the clean signal.
        Overdrive od; od.prepare(rate, 128, {false, 70, 70, 0, 80});
        double dryEnergy = 0, wetEnergy = 0; std::complex<double> fundamental {}, third {};
        for (int b = 0; b < 600; ++b) {
            if (b == 20) od.configure({true, 70, 70, 0, 80});
            for (int i = 0; i < 128; ++i) x.setSample(0, i, .15f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi * 375 * (b * 128 + i) / rate)));
            const float dry = x.getSample(0, 17); od.process(x.getWritePointer(0), 128);
            if (b < 20) require(x.getSample(0, 17) == dry, "Overdrive bypass must be sample-exact");
            if (b > 100) for (int i = 0; i < 128; ++i) {
                const double phase = juce::MathConstants<double>::twoPi * 375 * (b * 128 + i) / rate, y = x.getSample(0, i);
                require(std::isfinite(y) && std::abs(y) < 1.1, "Oversampled drive must remain finite and bounded");
                fundamental += y * std::polar(1.0, -phase); third += y * std::polar(1.0, -phase * 3);
                dryEnergy += std::pow(.15 * std::sin(phase), 2); wetEnergy += y * y;
            }
        }
        require(std::abs(third) / std::abs(fundamental) > .08 && wetEnergy > dryEnergy, "Built-in overdrive must produce audible harmonic distortion");
        od.configure({false, 70, 70, 12, 200});
        for (int b = 0; b < 100; ++b) { for (int i = 0; i < 128; ++i) x.setSample(0, i, .01f); od.process(x.getWritePointer(0), 128); }
        require(x.getSample(0, 127) == .01f, "Overdrive must return exactly to dry after its bypass fade");
        std::cout << "Overdrive oversampling latency at " << rate << " Hz: " << od.latencySamples() << " samples\n";

        // Identical cabinets at 50/50 must preserve level; opposite polarity cancels.
        DualCab c; c.load(impulse(), rate); c.load(impulse(), rate, 1);
        DualCab::Settings s; s.second = true; c.prepare({rate, 128, 2}, s);
        auto renderCab = [&](int blocks) {
            for (int b = 0; b < blocks; ++b) {
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) x.setSample(ch, i, .05f);
                juce::dsp::AudioBlock<float> block(x); juce::dsp::ProcessContextReplacing<float> context(block); c.process(context);
            }
        };
        renderCab(100); const float matched = x.getSample(0, 127);
        s.second = false; c.configure(s); renderCab(100); const float single = x.getSample(0, 127);
        require(std::abs(matched - single) < .0001f && std::abs(single - .00625f) < .0001f, "Parallel dual cabinets must preserve single-cab level at 50/50");
        s.second = true; s.invertB = true; c.configure(s); renderCab(100);
        require(x.getMagnitude(0, 128) < .0001f, "Polarity inversion of equal IRs must cancel the blend");
        s.invertB = false; s.panA = -100; s.panB = -100; c.configure(s); renderCab(100);
        require(x.getMagnitude(1, 0, 128) < .0001f && x.getMagnitude(0, 0, 128) > .003f, "Cabinet pan must balance both responses without crosstalk");
        s.panA = s.panB = 0; s.delayA = s.delayB = 2; c.configure(s); renderCab(100);
        x.clear(); // Clear alignment history before an impulse timing measurement.
        for (int b = 0; b < 100; ++b) { x.clear(); juce::dsp::AudioBlock<float> block(x); juce::dsp::ProcessContextReplacing<float> context(block); c.process(context); }
        int peakAt = 0; float peak = 0;
        for (int b = 0; b < 6; ++b) {
            x.clear(); if (b == 0) { x.setSample(0, 0, .05f); x.setSample(1, 0, .05f); }
            juce::dsp::AudioBlock<float> block(x); juce::dsp::ProcessContextReplacing<float> context(block); c.process(context);
            for (int i = 0; i < 128; ++i) if (std::abs(x.getSample(0, i)) > peak) { peak = std::abs(x.getSample(0, i)); peakAt = b * 128 + i; }
        }
        require(std::abs(peakAt - rate * .002) < 2, "Manual cabinet alignment must delay the impulse by its stated milliseconds");
        c.clear(); s.blend = 0; s.delayB = 0; c.configure(s); renderCab(100);
        require(std::abs(x.getSample(0, 127) - single) < .0001f, "Only cabinet B must remain audible even with blend at A's endpoint");
    }

    // Pre/post compression must work on all explicit sources, including a real NAM.
    for (int source : {1, 2, 3, 4}) {
        AmpSuiteAudioProcessor p(false); neutral(p); p.requestFile(true, fixture); ready(p); set(p, "AMP_SOURCE", static_cast<float>(source)); set(p, "COMP_MODE", 1);
        set(p, "CLEAN_COMP", 100); set(p, "COMP_THRESH", -40); set(p, "COMP_MAKEUP", 0); set(p, "COMP_RATIO", 6); p.prepareToPlay(48000, 128);
        juce::AudioBuffer<float> x(2, 128); juce::MidiBuffer midi;
        auto render = [&] { for (int b = 0; b < 150; ++b) { x.clear(); for (int i = 0; i < 128; ++i) x.setSample(0, i, .3f * std::sin(static_cast<float>(b * 128 + i) * .05f)); p.processBlock(x, midi); } };
        render(); require(static_cast<float>(p.status()["compressionDb"]) > 6, "Pre-amp compressor must work on every amp source");
        set(p, "COMP_MODE", 2); render(); require(static_cast<float>(p.status()["compressionDb"]) > 1, "Post-cab compressor must work on every amp source");
        set(p, "OD_ON", 1); render(); require(x.getMagnitude(0, 0, 128) > .0001f, "Built-in overdrive must remain audible on every source");
    }

    // Both cabinet files, new control values, and older defaults survive recall/packs.
    {
        juce::TemporaryFile a(".wav"), b(".wav"), pack(".cassian.zip"); writeWav(a.getFile(), impulse(), 48000); writeWav(b.getFile(), impulse(8), 48000);
        juce::TemporaryFile token(".sound-library"); auto root = token.getFile(); require(root.createDirectory().wasOk(), "Temporary sound library must open");
        {
            AmpSuiteAudioProcessor p(true, root); neutral(p); p.requestFile(false, a.getFile()); p.requestCabB(b.getFile()); p.requestFile(true, fixture); p.requestPedal(fixture); ready(p);
            set(p, "CAB_MODE", 1); set(p, "CAB_B_ON", 1); set(p, "CAB_BLEND", 67); set(p, "CAB_B_DELAY", 1.25f); set(p, "OD_ON", 1); set(p, "COMP_MODE", 2);
            auto rig = p.getRig(); require(p.applyRig(rig).isEmpty(), "Dual IR rig must queue"); ready(p);
            require(p.status()["irB"].toString().isNotEmpty() && p.apvts.getRawParameterValue("CAB_BLEND")->load() == 67, "Cabinet B and controls must recall together");
            require(p.exportRigPack(pack.getFile()).isEmpty(), "Four-asset pack must export"); juce::ZipFile zip(pack.getFile()); require(zip.getNumEntries() == 5, "Four-asset rig pack must include both cabinet files");
            AmpSuiteAudioProcessor imported(true, root.getChildFile("Other")); neutral(imported);
            require(imported.importRigPack(pack.getFile()).isEmpty(), "Four-asset pack must import");
            auto rigs = imported.getLibrary()["rigs"]; require(rigs.size() == 1 && imported.loadRig(rigs[0]["id"].toString()).isEmpty(), "Portable dual IR rig must recall"); ready(imported);
            require(stateOf(imported.getRig())["irBPath"].toString().contains("Other") && imported.apvts.getRawParameterValue("COMP_MODE")->load() == 2, "Portable recall must use cabinet B in destination storage");
            p.requestCabB(a.getFile()); ready(p); require(p.exportRigPack(pack.getFile()).isEmpty(), "Duplicate A/B cabinet asset must export once");
            require(imported.importRigPack(pack.getFile()).isEmpty(), "Deduplicated A/B cabinet pack must import");
            auto old = stateOf(rig); for (size_t i = 58; i < Params::definitions.size(); ++i) old.removeChild(old.getChildWithProperty("id", Params::definitions[i].id), nullptr);
            old.removeProperty("irBPath", nullptr); old.removeProperty("irBId", nullptr);
            require(p.applyRig(wrap(old)).isEmpty(), "Pre-update rigs must still recall"); ready(p);
            require(p.status()["irB"].toString().isEmpty() && p.apvts.getRawParameterValue("OD_ON")->load() == 0 && p.apvts.getRawParameterValue("COMP_MODE")->load() == 0, "Old rigs must clear additions and retain legacy compression routing");
            auto invalid = stateOf(rig); invalid.getChildWithProperty("id", "COMP_MODE").setProperty("value", 1.5, nullptr);
            require(!p.applyRig(wrap(invalid)).isEmpty(), "Fractional compressor routing must be rejected");
            invalid = stateOf(rig); invalid.setProperty("irBPath", root.getChildFile("missing.wav").getFullPathName(), nullptr); invalid.removeProperty("irBId", nullptr);
            require(!p.applyRig(wrap(invalid)).isEmpty(), "Missing second cabinet must reject complete recall without changing the rig");
        }
        if (root.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory)) && root.getFileName().startsWith("temp_")) root.deleteRecursively();
    }

    // Realistic-length stereo cabinet fixtures for the timing check.
    juce::TemporaryFile benchA(".wav"), benchB(".wav");
    juce::AudioBuffer<float> response(2, 4096);
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < response.getNumSamples(); ++i)
        response.setSample(ch, i, static_cast<float>(.2 * std::exp(-i / 500.0) * std::cos(i * (.2 + ch * .01))));
    writeWav(benchA.getFile(), response, 48000);
    response.setSample(1, 16, .5f); writeWav(benchB.getFile(), response, 48000);
    // Local timing data is diagnostic, never a flaky pass/fail CPU threshold.
    for (int profile : {0, 1}) for (const double rate : {44100., 48000., 96000.}) for (const int blockSize : {64, 128, 256}) {
        AmpSuiteAudioProcessor p(false); neutral(p); set(p, "AMP_SOURCE", profile == 0 ? 2.0f : 3.0f); set(p, "OD_ON", 1); set(p, "COMP_MODE", 2);
        p.requestFile(false, benchA.getFile()); p.requestCabB(benchB.getFile());
        if (profile == 1) { p.requestFile(true, fixture); p.requestPedal(fixture); set(p, "PEDAL_ON", 1); }
        ready(p); set(p, "CAB_MODE", 1); set(p, "CAB_B_ON", 1);
        set(p, "CHORUS_MIX", 25); set(p, "DELAY_MIX", 15); set(p, "REVERB_MIX", 20); p.prepareToPlay(rate, blockSize);
        juce::AudioBuffer<float> x(2, blockSize); juce::MidiBuffer midi; std::vector<double> timings;
        for (int b = 0; b < 260; ++b) {
            x.clear(); for (int i = 0; i < blockSize; ++i) x.setSample(0, i, phrase(b * blockSize + i, rate));
            const auto start = juce::Time::getMillisecondCounterHiRes(); p.processBlock(x, midi); const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
            if (b >= 20) timings.push_back(ms);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < blockSize; ++i) require(std::isfinite(x.getSample(ch, i)) && std::abs(x.getSample(ch, i)) <= 1, "Full sound update rig must render bounded audio at all rates and small buffers");
        }
        std::sort(timings.begin(), timings.end()); const double budget = blockSize * 1000 / rate;
        std::cout << "Sound benchmark " << (profile == 0 ? "Ferrum + dual IR" : "NAM amp/pedal + dual IR") << " / " << rate << " Hz / " << blockSize << ": mean " << std::accumulate(timings.begin(), timings.end(), 0.0) / timings.size()
            << " ms, p99 " << timings[timings.size() * 99 / 100] << " ms, max " << timings.back() << " ms, callback budget " << budget << " ms\n";
    }
    std::cout << "Dual cabinets, universal compression, overdrive, compatibility, and sound timing checks passed\n";
}
