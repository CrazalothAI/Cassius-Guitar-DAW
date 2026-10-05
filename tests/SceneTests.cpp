#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void set(AmpSuiteAudioProcessor& p, const char* id, float x) { auto* v = p.apvts.getParameter(id); v->setValueNotifyingHost(v->convertTo0to1(x)); }
float get(AmpSuiteAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue(id)->load(); }
juce::ValueTree unwrap(const juce::var& rig) { auto xml = juce::XmlDocument::parse(rig["state"].toString()); require(xml != nullptr, "Scene rig must contain XML"); return juce::ValueTree::fromXml(*xml); }
juce::var wrap(const juce::ValueTree& state) { auto o = std::make_unique<juce::DynamicObject>(); o->setProperty("schema", 1); o->setProperty("state", state.createXml()->toString()); return juce::var(o.release()); }
template<typename Predicate> void waitFor(Predicate ready) {
    for (int i = 0; i < 1500; ++i) { if (ready()) return; juce::Thread::sleep(5); }
    require(false, "Scene operation timed out");
}
void tick(AmpSuiteAudioProcessor& p) { juce::AudioBuffer<float> audio(2, 128); audio.clear(); juce::MidiBuffer midi; p.processBlock(audio, midi); }
void settle(AmpSuiteAudioProcessor& p) { waitFor([&] { tick(p); return !static_cast<bool>(p.status()["rigLoading"]); }); }
void di(AmpSuiteAudioProcessor& p) {
    set(p, "AMP_SOURCE", 4); set(p, "CAB_MODE", 3); set(p, "COMP_MODE", 1); set(p, "GATE_ON", 0);
    set(p, "REVERB_MIX", 0); set(p, "DELAY_TIME", 80); set(p, "DELAY_MIX", 50); set(p, "DELAY_FEEDBACK", 35);
    p.prepareToPlay(48000, 128);
}
juce::ValueTree sceneBoard(const juce::var& bank, int slot) {
    const auto xml = juce::XmlDocument::parse(bank["slots"][slot]["board"].toString());
    require(xml != nullptr && xml->hasTagName("PEDALBOARD"), "Scene must contain a valid board XML root");
    return juce::ValueTree::fromXml(*xml);
}
void makeLegacyBank(juce::ValueTree& parent) {
    auto scenes = parent.getChildWithName("SCENES"); auto bank = juce::JSON::parse(scenes["json"].toString());
    bank.getDynamicObject()->setProperty("version", 1);
    for (auto& slot : *bank["slots"].getArray()) if (slot.isObject()) slot.getDynamicObject()->removeProperty("board");
    scenes.setProperty("json", juce::JSON::toString(bank), nullptr);
}
}
void runSceneChecks()
{
    AmpSuiteAudioProcessor p(false); p.prepareToPlay(48000, 128);
    require(!p.recallScene(0).isEmpty() && !p.storeScene(4, "No").isEmpty() && !p.clearScene(-1).isEmpty(), "Empty and invalid scene slots must reject commands");
    require(!p.storeScene(0, "   ").isEmpty() && !p.storeScene(0, juce::String::repeatedString("x", 49)).isEmpty(), "Invalid scene names must be rejected");
    // Exercise every nonglobal parameter, not just the four amp knobs.
    for (const auto& d : Params::definitions) if (!PerformanceScenes::global(d.id)) set(p, d.id, d.min);
    require(p.storeScene(0, "  Rhythm  ").isEmpty(), "Scene must store a trimmed name");
    for (const auto& d : Params::definitions) if (!PerformanceScenes::global(d.id)) set(p, d.id, d.max);
    require(p.storeScene(1, "Lead").isEmpty(), "A second independent scene must store");
    set(p, "INPUT_GAIN", -3); set(p, "MASTER_VOL", -22); set(p, "METRO_ON", 1); set(p, "METRO_BPM", 157); set(p, "METRO_BEATS", 7); set(p, "METRO_LEVEL", -25);
    require(!static_cast<bool>(p.scenes.status(p.apvts)["edited"]), "Globals must not mark a scene edited");
    require(p.recallScene(0).isEmpty(), "First scene must recall");
    for (const auto& d : Params::definitions) if (!PerformanceScenes::global(d.id)) require(std::abs(get(p, d.id) - d.min) < .011f, "Scene must recall every saved minimum including routing");
    require(get(p, "INPUT_GAIN") == -3 && get(p, "MASTER_VOL") == -22 && get(p, "METRO_ON") == 1 && get(p, "METRO_BPM") == 157 && get(p, "METRO_BEATS") == 7 && get(p, "METRO_LEVEL") == -25, "Scenes must preserve input, Master and all click settings");
    require(p.recallScene(1).isEmpty(), "Second scene must recall");
    for (const auto& d : Params::definitions) if (!PerformanceScenes::global(d.id)) require(std::abs(get(p, d.id) - d.max) < .011f, "Scene must recall every saved maximum");
    set(p, "EQ_MUD", 0); require(static_cast<bool>(p.scenes.status(p.apvts)["edited"]), "Tone edits must mark the active scene");
    require(p.storeScene(2, "Clean").isEmpty() && p.storeScene(3, "Ambient").isEmpty(), "All four slots must store");
    auto rig = p.getRig(); const auto original = unwrap(rig); const auto bank = p.scenes.save()["json"].toString();
    require(PerformanceScenes::validate(original.getChildWithName("SCENES")).isEmpty(), "Extreme parameter scenes must validate after JSON/XML round-trip");
    juce::MemoryBlock session; p.getStateInformation(session); AmpSuiteAudioProcessor restored(false); restored.prepareToPlay(48000, 128);
    restored.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(restored);
    require(restored.scenes.save()["json"].toString() == bank && static_cast<int>(restored.scenes.status(restored.apvts)["active"]) == -1, "Session must preserve scene bank without falsely claiming an active scene");
    require(restored.recallScene(0).isEmpty() && get(restored, "AMP_SOURCE") == 0, "Restored scene must remain recallable");

    // Invalid complete rig documents leave both the live parameters and bank intact.
    const auto badBank = [&](auto mutate) {
        auto state = original.createCopy(); auto tree = state.getChildWithName("SCENES"); auto data = juce::JSON::parse(tree["json"].toString()); mutate(data);
        tree.setProperty("json", juce::JSON::toString(data), nullptr);
        require(!p.applyRig(wrap(state)).isEmpty() && get(p, "EQ_MUD") == 0 && p.scenes.save()["json"].toString() == bank, "Invalid scene document must preserve the current tone and bank");
    };
    badBank([](juce::var& v) { v.getDynamicObject()->setProperty("version", 9); });
    badBank([](juce::var& v) { v["slots"].getArray()->remove(3); });
    badBank([](juce::var& v) { v["slots"][0]["parameters"].getDynamicObject()->setProperty("MASTER_VOL", -10); });
    badBank([](juce::var& v) { v["slots"][0]["parameters"].getDynamicObject()->removeProperty("OD_ON"); });
    badBank([](juce::var& v) { v["slots"][0]["parameters"].getDynamicObject()->setProperty("DRIVE_GAIN", "NaN"); });
    badBank([](juce::var& v) { v["slots"][0]["parameters"].getDynamicObject()->setProperty("OD_DRIVE", 101); });
    badBank([](juce::var& v) { v["slots"][0]["parameters"].getDynamicObject()->setProperty("AMP_SOURCE", .5); });
    badBank([](juce::var& v) {
        auto board = sceneBoard(v, 0); board.setProperty("runtime", "serial-v99", nullptr);
        v["slots"][0].getDynamicObject()->setProperty("board", board.toXmlString());
    });
    auto duplicate = original.createCopy(); duplicate.addChild(duplicate.getChildWithName("SCENES").createCopy(), -1, nullptr);
    require(!p.applyRig(wrap(duplicate)).isEmpty(), "Duplicate scene banks must reject a rig");
    auto malformed = original.createCopy(); malformed.getChildWithName("SCENES").setProperty("json", "{broken", nullptr);
    require(!p.applyRig(wrap(malformed)).isEmpty(), "Malformed scene JSON must reject a rig");
    juce::AudioProcessor::copyXmlToBinary(*malformed.createXml(), session); restored.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(restored);
    require(restored.scenes.status(restored.apvts)["error"].toString().isNotEmpty() && !restored.recallScene(0).isEmpty(), "Invalid native session bank must clear scenes and report its error");

    // Old scene banks inherit the shared rig's block identities, while current
    // scenes restore their own saved identities without changing DSP routing.
    AmpSuiteAudioProcessor identities(false); identities.prepareToPlay(48000, 128);
    auto currentBoard = identities.apvts.state.getChildWithName("PEDALBOARD");
    require(currentBoard.isValid(), "New processor must initialize its board state");
    currentBoard.getChild(1).setProperty("id", "owner.overdrive.1", nullptr);
    set(identities, "OD_DRIVE", 37);
    require(identities.storeScene(0, "Identity").isEmpty(), "Scene must store a custom stable identity");
    const auto identityBank = juce::JSON::parse(identities.scenes.save()["json"].toString());
    require(static_cast<int>(identityBank["version"]) == 2 && sceneBoard(identityBank, 0).getChild(1)["id"].toString() == "owner.overdrive.1", "New scene format must preserve stable block identities");
    currentBoard.getChild(1).setProperty("id", "owner.overdrive.2", nullptr);
    require(static_cast<bool>(identities.scenes.status(identities.apvts)["edited"]), "Changing a block identity must mark the active scene edited");
    require(identities.recallScene(0).isEmpty() && identities.apvts.state.getChildWithName("PEDALBOARD").getChild(1)["id"].toString() == "owner.overdrive.1", "Scene recall must restore its stable identity");
    require(!static_cast<bool>(identities.scenes.status(identities.apvts)["edited"]), "Recalled board identity must match its scene baseline");
    auto oldSceneRig = unwrap(identities.getRig()); makeLegacyBank(oldSceneRig);
    require(identities.applyRig(wrap(oldSceneRig)).isEmpty(), "Version1 bank must migrate through complete rig recall"); settle(identities);
    const auto migratedBank = juce::JSON::parse(identities.scenes.save()["json"].toString());
    require(static_cast<int>(migratedBank["version"]) == 2 && sceneBoard(migratedBank, 0).getChild(1)["id"].toString() == "owner.overdrive.1", "Legacy scene migration must inherit shared rig identities");
    require(identities.recallScene(0).isEmpty() && get(identities, "OD_DRIVE") == 37, "Migrated scene must retain its original control values");
    auto noBoardSceneRig = unwrap(identities.getRig()); makeLegacyBank(noBoardSceneRig);
    noBoardSceneRig.removeChild(noBoardSceneRig.getChildWithName("PEDALBOARD"), nullptr);
    require(identities.applyRig(wrap(noBoardSceneRig)).isEmpty(), "Old rig without board metadata must migrate its scene bank"); settle(identities);
    const auto oldestBank = juce::JSON::parse(identities.scenes.save()["json"].toString());
    require(sceneBoard(oldestBank, 0).getChild(1)["id"].toString() == "legacy.overdrive", "Old scene bank must receive deterministic default block IDs");
    require(identities.recallScene(0).isEmpty() && get(identities, "OD_DRIVE") == 37, "Old scene parameters must survive default identity migration");
    // A future board in a host session must reject the whole recall, before
    // either tone parameters or the existing bank are replaced.
    auto unsupportedSession = unwrap(identities.getRig());
    auto unsupportedScenes = unsupportedSession.getChildWithName("SCENES");
    auto unsupportedData = juce::JSON::parse(unsupportedScenes["json"].toString());
    auto unsupportedBoard = sceneBoard(unsupportedData, 0); unsupportedBoard.setProperty("version", 99, nullptr);
    unsupportedData["slots"][0].getDynamicObject()->setProperty("board", unsupportedBoard.toXmlString());
    unsupportedScenes.setProperty("json", juce::JSON::toString(unsupportedData), nullptr);
    unsupportedSession.getChildWithProperty("id", "OD_DRIVE").setProperty("value", 99, nullptr);
    const auto preservedScenes = identities.scenes.save(); const auto preservedState = identities.apvts.state.createCopy();
    juce::AudioProcessor::copyXmlToBinary(*unsupportedSession.createXml(), session);
    identities.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(identities);
    require(get(identities, "OD_DRIVE") == 37 && PerformanceScenes::equivalent(preservedScenes, identities.scenes.save())
            && PedalboardState::equal(preservedState, identities.apvts.state), "Unsupported scene board must atomically preserve native session tone and state");
    auto futureSceneSession = unwrap(identities.getRig()); auto futureScenes = futureSceneSession.getChildWithName("SCENES");
    auto futureData = juce::JSON::parse(futureScenes["json"].toString());
    futureData.getDynamicObject()->setProperty("version", static_cast<juce::int64>(4294967298LL));
    futureScenes.setProperty("json", juce::JSON::toString(futureData), nullptr);
    futureSceneSession.getChildWithProperty("id", "OD_DRIVE").setProperty("value", 99, nullptr);
    juce::AudioProcessor::copyXmlToBinary(*futureSceneSession.createXml(), session);
    identities.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(identities);
    require(get(identities, "OD_DRIVE") == 37 && PerformanceScenes::equivalent(preservedScenes, identities.scenes.save())
            && PedalboardState::equal(preservedState, identities.apvts.state), "64-bit future scene version must reject atomically without wrapping to a supported version");

    // Saved active-rig baselines from before board metadata remain unedited
    // when only their scene-bank representation is upgraded.
    AmpSuiteAudioProcessor baselineProcessor(false); baselineProcessor.prepareToPlay(48000, 128);
    require(baselineProcessor.storeScene(0, "Legacy baseline").isEmpty(), "Legacy baseline scene must store");
    ActiveRig baseline; baseline.set("baseline-id", "Baseline", unwrap(baselineProcessor.getRig()));
    auto savedBaseline = baseline.save(); auto priorBaseline = savedBaseline.getChildWithName("BASELINE");
    priorBaseline.removeChild(priorBaseline.getChildWithName("PEDALBOARD"), nullptr); makeLegacyBank(priorBaseline);
    ActiveRig upgradedBaseline; upgradedBaseline.restore(savedBaseline);
    require(upgradedBaseline.name == "Baseline" && !upgradedBaseline.edited(baselineProcessor.apvts, {}, {}, baselineProcessor.scenes.save()), "Legacy baseline migration must preserve identity without false edited status");
    baselineProcessor.apvts.state.getChildWithName("PEDALBOARD").getChild(1).setProperty("id", "changed.overdrive", nullptr);
    require(upgradedBaseline.edited(baselineProcessor.apvts, {}, {}, baselineProcessor.scenes.save()), "Active rig must track stable block identity edits");
    require(baselineProcessor.storeScene(0, "Custom baseline").isEmpty(), "Custom baseline scene must store");
    baseline.set("custom-id", "Custom baseline", unwrap(baselineProcessor.getRig()));
    auto customBaseline = baseline.save(); auto customPrior = customBaseline.getChildWithName("BASELINE"); makeLegacyBank(customPrior);
    upgradedBaseline.restore(customBaseline);
    require(upgradedBaseline.name == "Custom baseline" && !upgradedBaseline.edited(baselineProcessor.apvts, {}, {}, baselineProcessor.scenes.save()), "Version1 bank baseline must inherit its shared custom IDs without false edited status");

    p.clearScene(0); require(get(p, "EQ_MUD") == 0 && !p.recallScene(0).isEmpty(), "Clearing a scene must leave the current tone unchanged");
    require(p.applyRig(rig).isEmpty(), "Complete rig must recall its original bank"); settle(p);
    require(p.scenes.save()["json"].toString() == bank && p.recallScene(0).isEmpty(), "Complete rig recall must restore all four scenes");
    require(p.validateRigDocument(p.getRig()).isEmpty(), "Complete rigs exported after recalling decimal minima must validate");
    auto legacy = original.createCopy(); legacy.removeChild(legacy.getChildWithName("SCENES"), nullptr);
    require(p.applyRig(wrap(legacy)).isEmpty(), "Legacy complete rig must still recall"); settle(p);
    require(!p.recallScene(0).isEmpty(), "Legacy complete rig must use an empty bank rather than unrelated old scenes");
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), session); restored.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(restored);
    require(restored.scenes.status(restored.apvts)["error"].toString().isEmpty() && !restored.recallScene(1).isEmpty(), "Legacy native sessions must restore a clean empty bank");

    // A failed asset preparation cannot replace a valid current bank.
    p.storeScene(0, "Survivor"); const auto survivor = p.scenes.save()["json"].toString();
    juce::TemporaryFile corrupt(".nam"); require(corrupt.getFile().replaceWithText("{}"), "Broken capture fixture must write");
    auto broken = original.createCopy(); broken.setProperty("modelPath", corrupt.getFile().getFullPathName(), nullptr); broken.setProperty("modelId", "", nullptr);
    require(p.applyRig(wrap(broken)).isEmpty(), "Broken capture must reach asynchronous preparation");
    waitFor([&] { tick(p); return p.status()["message"].toString().startsWith("Load failed:"); });
    require(p.scenes.save()["json"].toString() == survivor, "Failed rig preparation must preserve the old scene bank");
    p.requestFile(true, corrupt.getFile().getSiblingFile("missing-scene-check.nam"));
    require(!p.storeScene(1, "Blocked").isEmpty() && !p.recallScene(0).isEmpty() && !p.clearScene(0).isEmpty(), "Scene operations must block pending or unresolved stage loads");

    // PC scene switching uses the control worker, and old mappings need no new field.
    AmpSuiteAudioProcessor midi(false); midi.prepareToPlay(48000, 128); set(midi, "AMP_SOURCE", 1); midi.storeScene(0, "Clean"); set(midi, "AMP_SOURCE", 2); midi.storeScene(1, "Lead"); set(midi, "AMP_SOURCE", 1);
    auto mapping = std::make_unique<juce::DynamicObject>(); mapping->setProperty("type", "pc"); mapping->setProperty("number", 7); mapping->setProperty("channel", 1); mapping->setProperty("action", "scene"); mapping->setProperty("scene", 1);
    const juce::var assignment(mapping.release()); require(midi.midiControl.setMapping(0, assignment).isEmpty(), "Scene MIDI assignment must save"); midi.midiControl.enable(true);
    juce::AudioBuffer<float> audio(2, 128); audio.clear(); juce::MidiBuffer events; events.addEvent(juce::MidiMessage::programChange(1, 7), 0); midi.processBlock(audio, events);
    waitFor([&] { return get(midi, "AMP_SOURCE") == 2; }); require(events.isEmpty(), "Scene MIDI must be consumed by the processor");
    assignment.getDynamicObject()->setProperty("scene", static_cast<juce::int64>(4294967296LL)); require(!midi.midiControl.setMapping(0, assignment).isEmpty(), "MIDI scene index must reject 64-bit wrapping");
    assignment.getDynamicObject()->removeProperty("scene"); require(midi.midiControl.setMapping(0, assignment).isEmpty() && static_cast<int>(midi.midiControl.configuration()["mappings"][0]["scene"]) == 0, "Mappings without a scene field must retain compatibility");

    // Identical scenes must not reset delay history: verify the delayed impulse
    // against an uninterrupted processor through the first repeat and decay.
    AmpSuiteAudioProcessor switched(false), control(false); di(switched); di(control); switched.storeScene(0, "Same effects");
    for (int i = 0; i < 50; ++i) { tick(switched); tick(control); }
    double echoEnergy = 0;
    for (int b = 0; b < 150; ++b) {
        if (b == 20) require(switched.recallScene(0).isEmpty(), "Scene must recall during a delay tail");
        juce::AudioBuffer<float> a(2, 128), c(2, 128); a.clear(); c.clear(); if (b == 0) { a.setSample(0, 0, .1f); c.setSample(0, 0, .1f); }
        juce::MidiBuffer noMidi; switched.processBlock(a, noMidi); control.processBlock(c, noMidi);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) {
            require(std::isfinite(a.getSample(ch, i)) && std::abs(a.getSample(ch, i) - c.getSample(ch, i)) < 1e-6f, "Unchanged-effect scene must preserve the running delay history");
            if (b > 20) echoEnergy += std::abs(a.getSample(ch, i));
        }
    }
    require(echoEnergy > .001, "Delay continuity check must contain a real audible repeat");
    // Exercise driven/clean/source recalls during continuous input, with all
    // effects running, rather than only testing snapshot values on silent audio.
    AmpSuiteAudioProcessor performance(false); set(performance, "GATE_ON", 0); set(performance, "COMP_MODE", 3); set(performance, "DELAY_MIX", 20); set(performance, "REVERB_MIX", 15);
    set(performance, "AMP_SOURCE", 1); set(performance, "DRIVE_GAIN", 0); performance.storeScene(0, "Clean");
    set(performance, "AMP_SOURCE", 2); set(performance, "DRIVE_GAIN", 8); set(performance, "OD_ON", 1); performance.storeScene(1, "Driven"); performance.prepareToPlay(48000, 128);
    for (int b = 0; b < 500; ++b) {
        if (b % 80 == 0) require(performance.recallScene((b / 80) % 2).isEmpty(), "Performance scene must switch during input");
        juce::AudioBuffer<float> signal(2, 128); signal.clear();
        for (int i = 0; i < 128; ++i) signal.setSample(0, i, .05f * std::sin(juce::MathConstants<float>::twoPi * 220.f * static_cast<float>(b * 128 + i) / 48000.f));
        juce::MidiBuffer noMidi; performance.processBlock(signal, noMidi);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) require(std::isfinite(signal.getSample(ch, i)) && std::abs(signal.getSample(ch, i)) <= 1.f, "Repeated clean/driven scenes must keep monitored output finite and protected");
    }
    // Packs carry the scene bank without requiring new assets or changing the live tone at import.
    juce::TemporaryFile portable(".cassian.zip"); require(switched.exportRigPack(portable.getFile()).isEmpty(), "Scene bank must export in a portable pack");
    struct TempLibrary {
        juce::File root = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("CassianScenes", "", false);
        ~TempLibrary() { root.deleteRecursively(); }
    } folder;
    AmpSuiteAudioProcessor imported(true, folder.root); imported.prepareToPlay(48000, 128); require(imported.importRigPack(portable.getFile()).isEmpty(), "Scene pack must import");
    const auto rigs = imported.getLibrary()["rigs"]; require(rigs.size() == 1 && !imported.recallScene(0).isEmpty(), "Pack import must leave the live scene bank unchanged");
    require(imported.loadRig(rigs[0]["id"].toString()).isEmpty(), "Imported scene rig must recall"); settle(imported);
    require(imported.recallScene(0).isEmpty() && get(imported, "DELAY_TIME") == 80, "Portable pack recall must preserve scene controls");
    std::cout << "Scene bank, stable board identities, globals, migration, MIDI, pack, failure rollback and delay continuity checks passed\n";
}
