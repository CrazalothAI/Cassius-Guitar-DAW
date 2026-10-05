#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void set(AmpSuiteAudioProcessor& p, const char* id, float x) { auto* v = p.apvts.getParameter(id); v->setValueNotifyingHost(v->convertTo0to1(x)); }
template<typename F> void waitFor(F ready, const char* why = "Mix operation timed out") {
    for (int i = 0; i < 1500; ++i) { if (ready()) return; juce::Thread::sleep(5); }
    throw std::runtime_error(why);
}
void setup(AmpSuiteAudioProcessor& p, double rate, float level, float focus) {
    set(p, "AMP_SOURCE", 4); set(p, "CAB_MODE", 3); set(p, "GATE_ON", 0); set(p, "COMP_MODE", 3);
    set(p, "REVERB_MIX", 0); set(p, "DELAY_MIX", 0); set(p, "MASTER_VOL", -30);
    set(p, "GUITAR_MIX_LEVEL", level); set(p, "GUITAR_MIX_FOCUS", focus); p.prepareToPlay(rate, 128);
}
float signal(int i, double rate) { return .025f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi * 1200 * i / rate)); }
void fill(juce::AudioBuffer<float>& x, int offset, double rate) {
    x.clear(); for (int i = 0; i < x.getNumSamples(); ++i) x.setSample(0, i, signal(offset + i, rate));
}
juce::AudioBuffer<float> read(const juce::File& file) {
    juce::WavAudioFormat format; std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(file.createInputStream().release(), true));
    require(reader != nullptr, "Mix take must reopen");
    juce::AudioBuffer<float> x(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    require(reader->read(&x, 0, x.getNumSamples(), 0, true, true), "Mix take must read"); return x;
}
}
void runGuitarMixChecks()
{
    const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root = temp.getNonexistentChildFile("Cassian-mix-" + juce::Uuid().toString(), "", false);
    require(root.createDirectory().wasOk(), "Mix test folder must create");
    const auto neutralFolder = root.getChildFile("neutral"), changedFolder = root.getChildFile("changed");
    require(neutralFolder.createDirectory().wasOk() && changedFolder.createDirectory().wasOk(), "Separate recording destinations must create");
    struct Cleanup { juce::File root, base; ~Cleanup() { if (root.isAChildOf(base)) root.deleteRecursively(); } } cleanup {root, temp};
    for (const double rate : {44100., 48000., 96000.}) {
        for (int channels : {1, 2}) {
            GuitarMix mix; mix.prepare(rate, 128, 0, 0);
            juce::AudioBuffer<float> x(channels, 128), delta(channels, 128); fill(x, 0, rate);
            if (channels == 2) { x.copyFrom(1, 0, x, 0, 0, 128); x.applyGain(1, 0, 128, -.5f); }
            mix.difference(x, delta, 0, 0);
            require(delta.getMagnitude(0, 128) == 0, "Neutral mix must have a sample-exact zero delta");
            mix.prepare(rate, 128, 6, 0); mix.difference(x, delta, 6, 0);
            for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < 128; ++i)
                require(std::abs(delta.getSample(ch, i) + x.getSample(ch, i) - x.getSample(ch, i) * juce::Decibels::decibelsToGain(6.f)) < 1e-7f, "Listening gain must preserve stereo polarity and level");
            double flat = 0, focused = 0; mix.prepare(rate, 128, 0, 100);
            for (int b = 0; b < 40; ++b) {
                fill(x, b * 128, rate); mix.difference(x, delta, 0, 100);
                if (b > 20) for (int i = 0; i < 128; ++i) { flat += std::pow(x.getSample(0, i), 2); focused += std::pow(x.getSample(0, i) + delta.getSample(0, i), 2); }
            }
            require(focused > flat * 1.8, "Mix focus must improve measurable note-band definition");
            mix.prepare(rate, 128, 0, 0); x.clear(); x.applyGain(0); for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < 128; ++i) x.setSample(ch, i, .02f);
            mix.difference(x, delta, 12, 100); float previous = 0;
            for (int i = 0; i < 128; ++i) { require(std::abs(delta.getSample(0, i) - previous) < .002f, "Listening control changes must ramp without an abrupt gain jump"); previous = delta.getSample(0, i); }

            AmpSuiteAudioProcessor neutral(false), changed(false); setup(neutral, rate, 0, 0); setup(changed, rate, 6, 80);
            auto layout = neutral.getBusesLayout(); layout.outputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
            require(neutral.setBusesLayout(layout) && changed.setBusesLayout(layout), "Mono/stereo monitoring layout must work");
            neutral.prepareToPlay(rate, 128); changed.prepareToPlay(rate, 128);
            neutral.practice.setCountIn(0, 120, 4); changed.practice.setCountIn(0, 120, 4);
            require(neutral.practice.record(neutralFolder).isEmpty() && changed.practice.record(changedFolder).isEmpty(), "Both listening comparisons must arm recordings");
            waitFor([&] { return static_cast<int>(changed.practice.status()["recordMode"]) == 2 && static_cast<int>(neutral.practice.status()["recordMode"]) == 2; }, "Listening comparison recordings must finish arming");
            juce::AudioBuffer<float> a(channels, 128), b(channels, 128); juce::MidiBuffer midi; double plainPower = 0, monitorPower = 0;
            for (int n = 0; n < 32; ++n) {
                if (n == 8) { set(changed, "GUITAR_MIX_LEVEL", 9); set(changed, "GUITAR_MIX_FOCUS", 90); }
                fill(a, n * 128, rate); fill(b, n * 128, rate); neutral.processBlock(a, midi); changed.processBlock(b, midi);
                if (n > 16) for (int i = 0; i < 128; ++i) { plainPower += std::pow(a.getSample(0, i), 2); monitorPower += std::pow(b.getSample(0, i), 2); }
            }
            require(monitorPower > plainPower * 4, "Balance/focus must affect live monitored guitar");
            neutral.practice.command("pause"); changed.practice.command("pause");
            waitFor([&] { return static_cast<int>(changed.practice.status()["recordMode"]) == 0 && static_cast<int>(neutral.practice.status()["recordMode"]) == 0; }, "Listening comparison recordings must finalize");
            const juce::File take(changed.practice.status()["takePath"].toString()); auto dry = read(take.getChildFile("Guitar dry.wav")), wet = read(take.getChildFile("Guitar processed.wav"));
            const auto referenceWet = read(juce::File(neutral.practice.status()["takePath"].toString()).getChildFile("Guitar processed.wav"));
            require(wet.getNumSamples() == referenceWet.getNumSamples(), "Listening comparison takes must retain matching frame counts");
            for (int i = 0; i < dry.getNumSamples(); ++i) {
                require(dry.getSample(0, i) == signal(i, rate), "Listening mix must leave the recorded raw DI unchanged");
                for (int ch = 0; ch < wet.getNumChannels(); ++ch) require(wet.getSample(ch, i) == referenceWet.getSample(ch, i), "Processed recordings must be sample-identical with neutral and changing listening controls");
            }
            AmpSuiteAudioProcessor offlineA(false), offlineB(false); setup(offlineA, rate, 0, 0); setup(offlineB, rate, 12, 100); offlineA.setNonRealtime(true); offlineB.setNonRealtime(true);
            juce::AudioBuffer<float> renderA(2, 2048), renderB(2, 2048); fill(renderA, 0, rate); fill(renderB, 0, rate);
            offlineA.renderGuitarOffline(renderA, 2048); offlineB.renderGuitarOffline(renderB, 2048);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 2048; ++i) require(renderA.getSample(ch, i) == renderB.getSample(ch, i), "Offline reamp audio must be identical regardless of listening controls");
        }
        AmpSuiteAudioProcessor a(false), b(false); setup(a, rate, 0, 0); setup(b, rate, 12, 100);
        auto layout = a.getBusesLayout(); layout.inputBuses.set(1, juce::AudioChannelSet::stereo()); require(a.setBusesLayout(layout) && b.setBusesLayout(layout), "Auxiliary backing bus must enable");
        a.prepareToPlay(rate, 128); b.prepareToPlay(rate, 128); set(a, "METRO_ON", 1); set(b, "METRO_ON", 1);
        juce::AudioBuffer<float> x(3, 128), y(3, 128); juce::MidiBuffer midi;
        for (int n = 0; n < 50; ++n) {
            x.clear(); y.clear(); for (int i = 0; i < 128; ++i) { x.setSample(1, i, .04f); x.setSample(2, i, -.02f); y.setSample(1, i, .04f); y.setSample(2, i, -.02f); }
            a.processBlock(x, midi); b.processBlock(y, midi);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) require(x.getSample(ch, i) == y.getSample(ch, i), "Listening mix must leave backing and metronome audio unchanged");
        }
    }
    AmpSuiteAudioProcessor p(false); setup(p, 48000, 0, 0);
    juce::AudioBuffer<float> x(2, 128); juce::MidiBuffer midi; x.clear(); p.processBlock(x, midi); require(!static_cast<bool>(p.status()["outputPeakWarning"]), "Silence must not warn about mix peaks");
    set(p, "MASTER_VOL", 6); for (int n = 0; n < 30; ++n) { x.clear(); for (int i = 0; i < 128; ++i) x.setSample(0, i, .8f); p.processBlock(x, midi); }
    require(static_cast<bool>(p.status()["outputPeakWarning"]), "Pre-limiter peaks crossing the ceiling must hold the warning");
    for (int n = 0; n < 400; ++n) { x.clear(); p.processBlock(x, midi); }
    require(!static_cast<bool>(p.status()["outputPeakWarning"]), "Peak warning must clear after one second below the ceiling");
    set(p, "GUITAR_MIX_LEVEL", 5); set(p, "GUITAR_MIX_FOCUS", 65); const auto rig = p.getRig(); p.storeScene(0, "Keep mix");
    set(p, "GUITAR_MIX_LEVEL", -3); set(p, "GUITAR_MIX_FOCUS", 25); p.recallScene(0);
    require(p.apvts.getRawParameterValue("GUITAR_MIX_LEVEL")->load() == -3 && p.apvts.getRawParameterValue("GUITAR_MIX_FOCUS")->load() == 25, "Scene recall must preserve the listening mix");
    require(p.applyRig(rig).isEmpty(), "Complete rig/A-B recall must accept listening parameters");
    waitFor([&] { x.clear(); p.processBlock(x, midi); return !static_cast<bool>(p.status()["rigLoading"]); });
    require(p.apvts.getRawParameterValue("GUITAR_MIX_LEVEL")->load() == -3 && p.apvts.getRawParameterValue("GUITAR_MIX_FOCUS")->load() == 25, "Complete rig/A-B recall must preserve current listening controls");
    juce::MemoryBlock state; p.getStateInformation(state); AmpSuiteAudioProcessor restored(false); restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(restored.apvts.getRawParameterValue("GUITAR_MIX_LEVEL")->load() == -3 && restored.apvts.getRawParameterValue("GUITAR_MIX_FOCUS")->load() == 25, "Session restoration must retain saved listening controls");

    AmpSuiteAudioProcessor named(false); setup(named, 48000, 0, 0);
    require(named.saveRig("My clean").isEmpty(), "Save As must create a complete rig");
    const auto id = named.status()["activeRigId"].toString();
    require(id.isNotEmpty() && named.status()["activeRigName"].toString() == "My clean" && static_cast<bool>(named.status()["activeRigSaved"]) && !static_cast<bool>(named.status()["activeRigEdited"]), "Saving must name the active rig and establish a clean baseline");
    set(named, "GUITAR_MIX_LEVEL", 4); set(named, "GUITAR_MIX_FOCUS", 55); set(named, "MASTER_VOL", -25); set(named, "METRO_BPM", 135);
    require(!static_cast<bool>(named.status()["activeRigEdited"]), "Global listening controls must not mark the tone edited");
    set(named, "EQ_ON", 1); set(named, "EQ_FIZZ", -4);
    require(static_cast<bool>(named.status()["activeRigEdited"]), "Tone changes must mark the active saved rig edited");
    require(named.updateActiveRig().isEmpty() && named.getLibrary()["rigs"].size() == 1 && !static_cast<bool>(named.status()["activeRigEdited"]), "Save must update the same entry and reset its baseline");
    const auto a = named.getRig(); set(named, "EQ_FIZZ", -9);
    require(named.saveRig("Second clean").isEmpty(), "Save As must leave the original rig available");
    require(named.applyRig(a).isEmpty(), "A/B must restore complete rig identity");
    waitFor([&] { x.clear(); named.processBlock(x, midi); return !static_cast<bool>(named.status()["rigLoading"]); });
    require(named.status()["activeRigId"].toString() == id && named.status()["activeRigName"].toString() == "My clean" && !static_cast<bool>(named.status()["activeRigEdited"]), "A/B must restore the original name and baseline");
    set(named, "EQ_FIZZ", -6); require(named.updateActiveRig().isEmpty(), "In-place Save must update the library baseline");
    require(named.applyRig(a).isEmpty(), "An older A/B snapshot must remain usable");
    waitFor([&] { x.clear(); named.processBlock(x, midi); return !static_cast<bool>(named.status()["rigLoading"]); });
    require(static_cast<bool>(named.status()["activeRigEdited"]), "A/B snapshots older than an in-place Save must show Edited");
    set(named, "EQ_FIZZ", 0); require(named.loadRig(id).isEmpty(), "Saved rig must recall");
    waitFor([&] { x.clear(); named.processBlock(x, midi); return !static_cast<bool>(named.status()["rigLoading"]); });
    require(named.apvts.getRawParameterValue("EQ_FIZZ")->load() == -6 && !static_cast<bool>(named.status()["activeRigEdited"]), "Recall must use the updated rig rather than its original Save As state");
    require(!named.loadRig("missing").isEmpty() && named.status()["activeRigId"].toString() == id, "Failed recall must retain current rig identity");
    named.getStateInformation(state); AmpSuiteAudioProcessor namedSession(false); namedSession.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(namedSession.status()["activeRigId"].toString() == id && namedSession.status()["activeRigName"].toString() == "My clean", "Session restoration must retain active rig identity");
    named.storeScene(0, "New scene"); require(static_cast<bool>(named.status()["activeRigEdited"]), "Scene bank edits belong to the complete rig");
    require(named.removeRig(id) && !static_cast<bool>(named.status()["activeRigSaved"]) && !named.updateActiveRig().isEmpty(), "Deleted active rigs must require Save As without silently creating a duplicate");
    const auto blocked = root.getChildFile("not-a-folder"); require(blocked.replaceWithText("block storage"), "Failure fixture must create");
    AmpSuiteAudioProcessor fail(true, blocked); setup(fail, 48000, 0, 0);
    require(!fail.saveRig("Cannot save").isEmpty() && fail.status()["activeRigName"].toString().isEmpty() && fail.getLibrary()["rigs"].size() == 0, "Failed persistence must retain unsaved identity and roll back the new entry");
    std::cout << "Guitar listening mix, recording isolation and reamp checks passed\n";
}
