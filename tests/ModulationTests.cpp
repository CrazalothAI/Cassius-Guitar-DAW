#include "../Source/PluginProcessor.h"
#include <complex>
#include <iostream>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void set(AmpSuiteAudioProcessor& p, const char* id, float x) { auto* v = p.apvts.getParameter(id); v->setValueNotifyingHost(v->convertTo0to1(x)); }
double toneRms(int mode, double hz, double rate) {
    ModulationPedal pedal; pedal.prepare({rate, 128, 2}, {true, mode, .8f, 0, 100, 0, 0});
    juce::AudioBuffer<float> audio(2, 128); double energy = 0; int count = 0;
    for (int b = 0; b < 500; ++b) {
        for (int i = 0; i < 128; ++i) for (int ch = 0; ch < 2; ++ch) audio.setSample(ch, i, static_cast<float>(.2 * std::sin(juce::MathConstants<double>::twoPi * hz * (b * 128 + i) / rate)));
        pedal.process(audio);
        if (b > 100) for (int i = 0; i < 128; ++i) { energy += std::pow(audio.getSample(0, i), 2); ++count; }
    }
    return std::sqrt(energy / count);
}
}
void runModulationChecks()
{
    for (double rate : {44100., 48000., 96000.}) {
        ModulationPedal pedal; pedal.prepare({rate, 128, 2}, {false, 0, .8f, 100, 100, 70, 100});
        juce::AudioBuffer<float> audio(2, 128), original(2, 128); juce::Random random(93);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) audio.setSample(ch, i, .1f * (random.nextFloat() - .5f));
        original.makeCopyOf(audio); pedal.process(audio);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) require(audio.getSample(ch, i) == original.getSample(ch, i), "Modulation must bypass exactly by default");
        float previous = 0, largestStep = 0;
        for (int b = 0; b < 1200; ++b) {
            if (b == 20 || b == 300 || b == 600 || b == 900) pedal.configure({b != 900, b == 300 ? 1 : b == 600 ? 2 : 0, 8.f, 100, 100, 70, 100});
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) audio.setSample(ch, i, .04f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi * 110 * (b * 128 + i) / rate)));
            const float dry = audio.getSample(0, 127); pedal.process(audio);
            for (int i = 0; i < 128; ++i) for (int ch = 0; ch < 2; ++ch) require(std::isfinite(audio.getSample(ch, i)) && std::abs(audio.getSample(ch, i)) < .8f, "Maximum modulation feedback and fast mode changes must stay finite and bounded");
            for (int i = 0; i < 128; ++i) { const float y = audio.getSample(0, i); largestStep = juce::jmax(largestStep, std::abs(y - previous)); previous = y; }
            if (b > 1000) require(audio.getSample(0, 127) == dry, "Disabling active modulation must settle to exact bypass");
        }
        require(largestStep < .02f, "Enable, mode changes and bypass must avoid abrupt clicks");
        pedal.prepare({rate, 128, 1}, {true, 1, 10, 100, 0, 70, 100});
        juce::AudioBuffer<float> mono(1, 128); for (int i = 0; i < 128; ++i) mono.setSample(0, i, .02f);
        pedal.process(mono); require(mono.getSample(0, 100) == .02f, "Zero modulation mix must preserve mono audio exactly");

        // A full-depth tremolo on a DC probe exposes its actual envelope and rate.
        pedal.prepare({rate, 128, 2}, {true, 2, 4, 100, 100, 70, 100});
        std::complex<double> at4 {}, at2 {}; double difference = 0; float low = 1, high = 0;
        const int frames = static_cast<int>(rate * 2);
        for (int n = 0; n < frames; n += 128) {
            const int samples = juce::jmin(128, frames - n); audio.setSize(2, samples);
            for (int i = 0; i < samples; ++i) { audio.setSample(0, i, .2f); audio.setSample(1, i, .2f); }
            pedal.process(audio);
            for (int i = 0; i < samples; ++i) {
                const double t = (n + i) / rate, y = audio.getSample(0, i);
                at4 += y * std::polar(1.0, -juce::MathConstants<double>::twoPi * 4 * t); at2 += y * std::polar(1.0, -juce::MathConstants<double>::twoPi * 2 * t);
                low = juce::jmin(low, static_cast<float>(y)); high = juce::jmax(high, static_cast<float>(y)); difference += std::abs(y - audio.getSample(1, i));
            }
        }
        require(low < .001f && high > .199f && high <= .201f, "Tremolo must attenuate between silence and dry level without boosting");
        require(std::abs(at4) / frames > .049 && std::abs(at2) / frames < .001, "Tremolo must modulate at the selected rate");
        require(difference / frames > .05, "Stereo Spread must offset left/right modulation");
        require(toneRms(0, 570, rate) < toneRms(0, 40, rate) * .05, "Phaser must create an all-pass cancellation notch");
        require(toneRms(1, 1000, rate) < toneRms(1, 1 / .0035, rate) * .05, "Flanger must produce short-delay comb filtering");
    }
    require(std::abs(ModulationPedal::syncedRate(120, 0) - .5f) < .001f && ModulationPedal::syncedRate(120, 2) == 2 && ModulationPedal::syncedRate(120, 3) == 4, "Tempo subdivisions must set full LFO cycles");
    {
        AmpSuiteAudioProcessor p(false); set(p, "MOD_ON", 1); set(p, "MOD_TYPE", 2); set(p, "MOD_RATE", 3.5f); set(p, "MOD_STEREO", 60);
        p.prepareToPlay(48000, 128);
        juce::MemoryBlock state; p.getStateInformation(state); AmpSuiteAudioProcessor restored(false); restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        require(restored.apvts.getRawParameterValue("MOD_ON")->load() == 1 && std::abs(restored.apvts.getRawParameterValue("MOD_RATE")->load() - 3.5f) < .001f && std::abs(restored.apvts.getRawParameterValue("MOD_STEREO")->load() - 60) < .001f, "Sessions must retain modulation settings");
        auto rig = p.getRig(); auto xml = juce::XmlDocument::parse(rig["state"].toString()); require(xml != nullptr, "Modulation rig must serialize");
        auto legacy = juce::ValueTree::fromXml(*xml);
        legacy.removeChild(legacy.getChildWithName("PEDALBOARD"), nullptr);
        for (size_t i = 76; i < Params::definitions.size(); ++i) legacy.removeChild(legacy.getChildWithProperty("id", Params::definitions[i].id), nullptr);
        rig.getDynamicObject()->setProperty("schema", 1);
        rig.getDynamicObject()->setProperty("state", legacy.createXml()->toString()); require(p.applyRig(rig).isEmpty(), "Older 76-control rigs must accept compatible modulation defaults");
        for (int b = 0; b < 1000 && p.apvts.getRawParameterValue("MOD_ON")->load() > .5f; ++b) { juce::AudioBuffer<float> audio(2, 128); audio.clear(); juce::MidiBuffer midi; p.processBlock(audio, midi); juce::Thread::sleep(2); }
        require(p.apvts.getRawParameterValue("MOD_ON")->load() == 0, "Older complete rigs must recall with modulation bypassed");
        auto mapping = std::make_unique<juce::DynamicObject>(); mapping->setProperty("type", "cc"); mapping->setProperty("action", "modulation"); mapping->setProperty("channel", 1); mapping->setProperty("number", 42);
        require(p.midiControl.setMapping(0, juce::var(mapping.release())).isEmpty(), "MIDI must accept modulation bypass assignment"); p.midiControl.enable(true);
        juce::AudioBuffer<float> audio(2, 128); audio.clear(); juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::controllerEvent(1, 42, 127), 0); p.processBlock(audio, midi);
        for (int i = 0; i < 1000 && p.apvts.getRawParameterValue("MOD_ON")->load() < .5f; ++i) juce::Thread::sleep(2);
        require(p.apvts.getRawParameterValue("MOD_ON")->load() == 1, "MIDI footswitch must toggle modulation");
    }
    {
        AmpSuiteAudioProcessor p(false); p.setNonRealtime(true);
        set(p, "AMP_SOURCE", 4); set(p, "CAB_MODE", 3); set(p, "GATE_ON", 0); set(p, "REVERB_MIX", 0);
        set(p, "MOD_ON", 1); set(p, "MOD_TYPE", 2); set(p, "MOD_DEPTH", 100); set(p, "MOD_MIX", 100); set(p, "MOD_SYNC", 1); set(p, "MOD_DIVISION", 2); set(p, "METRO_BPM", 90);
        p.prepareToPlay(48000, 128); juce::AudioBuffer<float> audio(2, 96000); audio.clear();
        for (int i = 0; i < audio.getNumSamples(); ++i) audio.setSample(0, i, .05f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi * 600 * i / 48000)));
        p.renderGuitarOffline(audio, audio.getNumSamples());
        std::complex<double> synced {}, wrong {};
        for (int i = 0; i < audio.getNumSamples(); ++i) {
            const double y = audio.getSample(0, i), t = i / 48000.;
            synced += y * std::polar(1.0, -juce::MathConstants<double>::twoPi * 601.5 * t);
            wrong += y * std::polar(1.0, -juce::MathConstants<double>::twoPi * 602 * t);
        }
        require(std::abs(synced) / 96000 > .01 && std::abs(wrong) < std::abs(synced) * .05, "Offline modulation must follow saved 90 BPM, not a stale 120 BPM clock");
        set(p, "MOD_ON", 0); set(p, "DELAY_SYNC", 1); set(p, "DELAY_DIVISION", 0); set(p, "DELAY_MIX", 100); set(p, "DELAY_FEEDBACK", 0);
        p.prepareToPlay(48000, 128); audio.clear(); p.renderGuitarOffline(audio, audio.getNumSamples());
        audio.clear(); audio.setSample(0, 0, .1f); p.renderGuitarOffline(audio, audio.getNumSamples());
        float peak = 0; int peakAt = 0;
        for (int i = 256; i < audio.getNumSamples(); ++i) if (std::abs(audio.getSample(0, i)) > peak) { peak = std::abs(audio.getSample(0, i)); peakAt = i; }
        require(peak > .02f && std::abs(peakAt - 32000) <= 1, "Offline sync delay must also follow the saved 90 BPM clock");
    }
    {
        ModulationPedal pedal; pedal.prepare({48000, 128, 2}, {true, 0, 5, 100, 100, 70, 100}); juce::AudioBuffer<float> audio(2, 128);
        double total = 0, peak = 0;
        for (int b = 0; b < 1000; ++b) {
            for (int i = 0; i < 128; ++i) { const float y = .05f * std::sin(static_cast<float>((b * 128 + i) * .03)); audio.setSample(0, i, y); audio.setSample(1, i, y); }
            const double start = juce::Time::getMillisecondCounterHiRes(); pedal.process(audio); const double elapsed = juce::Time::getMillisecondCounterHiRes() - start;
            if (b >= 100) { total += elapsed; peak = juce::jmax(peak, elapsed); }
        }
        std::cout << "Modulation benchmark / 48000 Hz / 128: mean " << total / 900 << " ms, max " << peak << " ms; callback budget 2.66667 ms\n";
    }
    std::cout << "Modulation bypass, sweep, tremolo, stereo, smoothing and recall checks passed\n";
}
