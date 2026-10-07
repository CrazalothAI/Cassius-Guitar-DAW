#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
template<typename Predicate> void waitFor(Predicate ready) {
    for (int i = 0; i < 1500; ++i) { if (ready()) return; juce::Thread::sleep(5); }
    require(false, "MIDI worker timed out");
}
juce::var map(const char* action, int number, int channel = 0, const char* type = "cc", juce::String rig = {}, bool inverted = false) {
    auto o = std::make_unique<juce::DynamicObject>(); o->setProperty("type", type); o->setProperty("number", number); o->setProperty("channel", channel);
    o->setProperty("action", action); o->setProperty("rig", rig); o->setProperty("inverted", inverted); return juce::var(o.release());
}
void cc(MidiControl& m, int channel, int number, int value) { juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::controllerEvent(channel, number, value), 0); m.receive(midi); }
void set(AmpSuiteAudioProcessor& p, const char* id, float x) { auto* v = p.apvts.getParameter(id); v->setValueNotifyingHost(v->convertTo0to1(x)); }
void send(AmpSuiteAudioProcessor& p, const juce::MidiMessage& event) {
    juce::AudioBuffer<float> audio(2, 128); audio.clear(); juce::MidiBuffer midi; midi.addEvent(event, 63); p.processBlock(audio, midi);
    require(midi.isEmpty(), "Cassian must consume MIDI without producing MIDI output");
    require(std::isfinite(audio.getMagnitude(0, 0, 128)), "MIDI commands must leave audio finite");
}
}
void runMidiChecks()
{
    {
        MidiControl control; std::atomic<int> actions {0}; control.start([&](const auto&, int) { ++actions; return juce::String(); });
        require(control.setMapping(0, map("overdrive", 20, 2)).isEmpty(), "Valid MIDI mapping must save");
        cc(control, 2, 20, 127); juce::Thread::sleep(25); require(actions.load() == 0, "MIDI mapping must start disabled");
        control.enable(true); cc(control, 2, 20, 0); cc(control, 1, 20, 127); cc(control, 2, 20, 127);
        waitFor([&] { return actions.load() == 1; });
        cc(control, 2, 20, 127); cc(control, 2, 20, 90); juce::Thread::sleep(25); require(actions.load() == 1, "Held CC switch must toggle only once");
        cc(control, 2, 20, 63); cc(control, 2, 20, 64); waitFor([&] { return actions.load() == 2; });
        require(!control.setMapping(1, map("eq", 20, 0)).isEmpty(), "Omni assignments must reject an overlapping channel binding");
        require(control.setMapping(1, map("eq", 20, 3)).isEmpty(), "Distinct channels may reuse a controller number");
        require(!control.setMapping(2, map("master", 1, 1, "pc")).isEmpty(), "PC cannot act as expression");
        require(!control.setMapping(8, map("eq", 2)).isEmpty() && !control.setMapping(2, map("eq", 128)).isEmpty() && !control.setMapping(2, map("eq", 2, 17)).isEmpty(), "Invalid slots and message ranges must be rejected");
        auto huge = map("eq", 2); huge.getDynamicObject()->setProperty("number", static_cast<juce::int64>(4294967296LL));
        require(!control.setMapping(2, huge).isEmpty(), "64-bit MIDI numbers must not wrap into a valid binding");
        control.enable(false); require(control.learn(3).isEmpty(), "Learn must work while mapping is disabled");
        cc(control, 4, 42, 17); waitFor([&] { return static_cast<int>(control.status()["learning"]) == -1; });
        const auto learned = control.configuration()["mappings"][3];
        require(learned["type"].toString() == "cc" && static_cast<int>(learned["channel"]) == 4 && static_cast<int>(learned["number"]) == 42 && actions.load() == 2, "Learn must bind the actual channel/number without executing an action");
        control.learn(4); control.learn(-1); cc(control, 1, 44, 0); juce::Thread::sleep(25);
        require(static_cast<int>(control.configuration()["mappings"][4]["number"]) == 20, "Cancelled Learn must not alter a binding");
        const auto saved = control.configuration(); MidiControl restored; require(restored.restore(saved).isEmpty(), "MIDI settings must restore");
        require(juce::JSON::toString(restored.configuration()) == juce::JSON::toString(saved), "MIDI configuration must round-trip exactly");
        auto bad = std::make_unique<juce::DynamicObject>(); bad->setProperty("version", 99);
        require(!restored.restore(juce::var(bad.release())).isEmpty() && !static_cast<bool>(restored.configuration()["enabled"]), "Invalid MIDI state must disable mapping");
    }
    {
        MidiControl control; std::atomic<int> calls {0};
        control.setMapping(0, map("eq", 2)); control.enable(true);
        control.start([&](const auto&, int) { if (++calls == 1) throw std::runtime_error("action failure"); return juce::String(); });
        cc(control, 1, 2, 127); waitFor([&] { return control.status()["error"].toString().isNotEmpty(); });
        cc(control, 1, 2, 0); cc(control, 1, 2, 127); waitFor([&] { return calls.load() == 2 && control.status()["error"].toString().isEmpty(); });
    }
    {
        MidiControl control; require(control.setMapping(0, map("master", 1)).isEmpty(), "Expression mapping must save"); control.enable(true);
        juce::MidiBuffer flood; for (int i = 0; i < 600; ++i) flood.addEvent(juce::MidiMessage::controllerEvent(1, 1, i % 128), i);
        control.receive(flood); require(static_cast<int>(control.status()["dropped"]) > 0, "MIDI flood must be bounded and reported");
        control.enable(false); std::atomic<int> actions {0}; control.start([&](const auto&, int) { ++actions; return juce::String(); }); juce::Thread::sleep(30);
        require(actions.load() == 0, "Disabling mappings must invalidate all queued commands");
    }
    {
        AmpSuiteAudioProcessor p(false); p.prepareToPlay(48000, 128);
        require(p.acceptsMidi() && !p.producesMidi(), "Processor must advertise MIDI input only");
        p.midiControl.setMapping(0, map("overdrive", 20)); p.midiControl.setMapping(1, map("master", 21)); p.midiControl.setMapping(2, map("reverb", 22, 0, "cc", {}, true)); p.midiControl.enable(true);
        set(p, "OD_ON", 0); send(p, juce::MidiMessage::controllerEvent(1, 20, 127)); waitFor([&] { return p.apvts.getRawParameterValue("OD_ON")->load() == 1; });
        send(p, juce::MidiMessage::controllerEvent(1, 21, 127)); waitFor([&] { return p.apvts.getRawParameterValue("MASTER_VOL")->load() == 0; });
        send(p, juce::MidiMessage::controllerEvent(1, 21, 0)); waitFor([&] { return p.apvts.getRawParameterValue("MASTER_VOL")->load() == -60; });
        send(p, juce::MidiMessage::controllerEvent(1, 22, 127)); waitFor([&] { return p.apvts.getRawParameterValue("REVERB_MIX")->load() == 0; });
        require(p.midiControl.setMapping(3,map("wah1",23)).isEmpty() && p.midiControl.setMapping(4,map("wah2",24,0,"cc",{},true)).isEmpty(),"Both wah expression assignments must be accepted");
        require(p.midiControl.setMapping(5,map("wah1",25,0,"pc")).isNotEmpty(),"Wah expression must reject program changes");
        send(p,juce::MidiMessage::controllerEvent(1,23,127)); waitFor([&]{return p.apvts.getRawParameterValue("BOARD_WAH_0_WAH_POSITION")->load()==100;});
        send(p,juce::MidiMessage::controllerEvent(1,24,127)); waitFor([&]{return p.apvts.getRawParameterValue("BOARD_WAH_1_WAH_POSITION")->load()==0;});
        require(p.apvts.getRawParameterValue("BOARD_WAH_0_WAH_POSITION")->load()==100,"Second expression assignment must not change the first wah");
        // SysEx, note messages and MIDI clocks cannot activate CC bindings.
        const juce::uint8 sysex[] {0x7d, 0x14, 0x7f};
        set(p, "OD_ON", 0); send(p, juce::MidiMessage::noteOn(1, 20, static_cast<juce::uint8>(127))); send(p, juce::MidiMessage::midiClock()); send(p, juce::MidiMessage::createSysExMessage(sysex, 3)); juce::Thread::sleep(25);
        require(p.apvts.getRawParameterValue("OD_ON")->load() == 0, "Unmapped message types must leave the rig unchanged");
        juce::MemoryBlock state; p.getStateInformation(state); AmpSuiteAudioProcessor restored(false); restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        require(juce::JSON::toString(restored.midiControl.configuration()) == juce::JSON::toString(p.midiControl.configuration()), "MIDI assignments must survive native session restore");
        auto invalid = juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize())));
        invalid.getChildWithName("MIDICONTROL").setProperty("json", "broken JSON", nullptr);
        juce::AudioProcessor::copyXmlToBinary(*invalid.createXml(), state); restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        require(!static_cast<bool>(restored.midiControl.configuration()["enabled"]) && restored.midiControl.status()["error"].toString().isNotEmpty(), "Malformed MIDI JSON must disable mapping and report the problem");
        const auto snapshot = p.getRig(); require(!snapshot["state"].toString().contains("MIDICONTROL"), "Exported rigs must exclude session MIDI bindings");
        const auto old = juce::ValueTree::fromXml(R"(<AmpSuiteState><PARAM id="AMP_CLEAN" value="1"/></AmpSuiteState>)");
        juce::AudioProcessor::copyXmlToBinary(*old.createXml(), state); restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        require(!static_cast<bool>(restored.midiControl.configuration()["enabled"]), "Legacy sessions must start with MIDI mapping disabled");
    }
    {
        AmpSuiteAudioProcessor p(false); p.prepareToPlay(48000, 128); set(p, "AMP_SOURCE", 1); require(p.saveRig("Clean foot rig").isEmpty(), "Clean foot rig must save");
        set(p, "AMP_SOURCE", 2); require(p.saveRig("Lead foot rig").isEmpty(), "Lead foot rig must save");
        juce::String id; const auto catalog = p.getLibrary(); for (const auto& rig : *catalog["rigs"].getArray()) if (rig["name"].toString() == "Lead foot rig") id = rig["id"].toString();
        require(id.isNotEmpty(), "Saved lead rig must have an ID");
        p.midiControl.setMapping(0, map("rig", 3, 1, "pc", id)); p.midiControl.enable(true); set(p, "AMP_SOURCE", 1); set(p, "MASTER_VOL", -18); set(p, "INPUT_GAIN", -2);
        send(p, juce::MidiMessage::programChange(1, 3));
        waitFor([&] { juce::AudioBuffer<float> audio(2, 128); audio.clear(); juce::MidiBuffer midi; p.processBlock(audio, midi); return p.apvts.getRawParameterValue("AMP_SOURCE")->load() == 2 && !p.getRig().hasProperty("error"); });
        require(p.apvts.getRawParameterValue("MASTER_VOL")->load() == -18 && p.apvts.getRawParameterValue("INPUT_GAIN")->load() == -2, "MIDI recall must preserve global listening level/calibration");
        require(static_cast<bool>(p.midiControl.configuration()["enabled"]), "Rig recall must preserve MIDI assignments");
        p.removeRig(id); send(p, juce::MidiMessage::programChange(1, 3)); waitFor([&] { return p.midiControl.status()["error"].toString().isNotEmpty(); });
        require(p.apvts.getRawParameterValue("AMP_SOURCE")->load() == 2, "Missing MIDI target must preserve the current rig");
    }
    std::cout << "MIDI learn, mapping, bounded queue and rig-recall checks passed\n";
}
