#include "../Source/PluginProcessor.h"
#include "../Source/SoundPack.h"
#include <iostream>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void set(AmpSuiteAudioProcessor& p, const juce::String& id, float value) { auto* param = p.apvts.getParameter(id); require(param != nullptr, "Board parameter must exist"); param->setValueNotifyingHost(param->convertTo0to1(value)); }
void settle(AmpSuiteAudioProcessor& p) { for (int i = 0; i < 600; ++i) { if (!static_cast<bool>(p.status()["rigLoading"])) { require(!p.status()["message"].toString().startsWith("Load failed:"), "Board preparation must succeed"); return; } juce::Thread::sleep(10); } require(false, "Board preparation timed out"); }
juce::var args(std::initializer_list<std::pair<juce::Identifier, juce::var>> fields) { auto o = std::make_unique<juce::DynamicObject>(); for (const auto& field : fields) o->setProperty(field.first, field.second); return juce::var(o.release()); }
juce::ValueTree state(AmpSuiteAudioProcessor& p) { return juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString())); }
juce::String id(AmpSuiteAudioProcessor& p, const juce::String& type, int slot = 0) { for (const auto& row : state(p).getChildWithName("PEDALBOARD")) if (row["type"].toString() == type && static_cast<int>(row["automationSlot"]) == slot) return row["id"].toString(); return {}; }
}

void runSerialBoardChecks(const juce::File& fixture)
{
    AmpSuiteAudioProcessor p(false); p.prepareToPlay(48000, 128);
    require(p.boardCommand("convert", {}).isEmpty(), "Legacy board must explicitly convert"); settle(p);
    require(PedalboardState::serial(state(p)), "Conversion must publish a serial runtime");
    set(p, "EQ_FOCUS", 3);
    const auto first = id(p, "eq");
    require(p.boardCommand("duplicate", args({{"id", first}})).isEmpty(), "EQ must duplicate"); settle(p);
    const auto second = id(p, "eq", 1);
    require(second != first && second.isNotEmpty(), "Duplicate must have an independent identity");
    require(p.apvts.getRawParameterValue("BOARD_EQ_1_EQ_FOCUS")->load() == 3, "Duplicate must copy its independent controls");
    set(p, "BOARD_EQ_1_EQ_FOCUS", -3);
    require(p.apvts.getRawParameterValue("EQ_FOCUS")->load() == 3, "Editing a duplicate must not change the original");
    require(p.boardCommand("move", args({{"id", second}, {"direction", -1}})).isEmpty(), "Reordering must succeed"); settle(p);
    require(id(p, "eq", 1) == second, "Moving must preserve identity and slot");
    require(p.boardCommand("lane", args({{"id", second}, {"lane", "pre"}})).isEmpty(), "EQ must move before amp"); settle(p);
    const auto intact = p.getRig()["state"].toString();
    require(!p.boardCommand("lane", args({{"id", id(p, "neural-pedal")}, {"lane", "post"}})).isEmpty() && p.getRig()["state"].toString() == intact, "Mono captured pedals cannot move after cabinet and rejection must be atomic");
    require(p.storeScene(0, "Two EQs").isEmpty(), "Serial scene must store");
    require(p.boardCommand("remove", args({{"id", second}})).isEmpty(), "Removal must succeed"); settle(p);
    require(!p.boardCommand("add", args({{"type", "eq"}, {"lane", "post"}})).isEmpty(), "Deleted automation slots must not be reused");
    require(p.boardCommand("undo", {}).isEmpty(), "Removal must undo"); settle(p);
    require(static_cast<int>(state(p).getChildWithName("PEDALBOARD").getChildWithProperty("id", second)["deleted"]) == 0, "Undo must restore the same pedal identity");
    require(p.boardCommand("redo", {}).isEmpty(), "Removal must redo"); settle(p);
    require(p.recallScene(0).isEmpty(), "Scene must recall a different serial topology"); settle(p);
    require(static_cast<int>(p.status()["scenes"]["active"]) == 0 && static_cast<int>(state(p).getChildWithName("PEDALBOARD").getChildWithProperty("id", second)["deleted"]) == 0, "Scene must restore its graph and highlight");
    juce::MemoryBlock session; p.getStateInformation(session);
    AmpSuiteAudioProcessor restored(false); restored.prepareToPlay(96000, 257); restored.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(restored);
    require(PedalboardState::equal(state(p), state(restored)) && restored.apvts.getRawParameterValue("BOARD_EQ_1_EQ_FOCUS")->load() == -3, "Native recall must rebuild the serial graph at the host rate");
    auto malformed = state(p); malformed.getChildWithProperty("id", "BOARD_EQ_1_EQ_FOCUS").setProperty("value", "nan", nullptr);
    const auto snapshot = p.getRig()["state"].toString();
    require(!p.applyRig(args({{"schema", 3}, {"state", malformed.toXmlString()}})).isEmpty() && p.getRig()["state"].toString() == snapshot, "Invalid independent parameters must reject atomically");

    // Verify order actually reaches the audio graph: EQ before saturation must
    // differ from EQ after saturation, with the same independent settings.
    set(p, "OD_ON", 1); set(p, "OD_DRIVE", 65); set(p, "EQ_ON", 1); set(p, "EQ_FOCUS", 8);
    auto ordered = state(p); ordered.removeChild(ordered.getChildWithName("PEDALBOARD"), nullptr);
    auto board = PedalboardState::emptySerial();
    for (const auto* type : {"eq", "overdrive"}) { juce::ValueTree row("BLOCK"); row.setProperty("id", type, nullptr); row.setProperty("type", type, nullptr); row.setProperty("automationSlot", 0, nullptr); row.setProperty("lane", "pre", nullptr); row.setProperty("deleted", 0, nullptr); board.addChild(row, -1, nullptr); }
    ordered.addChild(board, -1, nullptr); SerialPedalboard a(ordered, p.apvts, {48000, 128, 2});
    auto reversed = ordered.createCopy(); reversed.getChildWithName("PEDALBOARD").moveChild(0, 1, nullptr); SerialPedalboard b(reversed, p.apvts, {48000, 128, 2});
    juce::AudioBuffer<float> left(1, 128), right(1, 128); NamWrapper* captures[] {nullptr, nullptr}; double difference = 0;
    for (int block = 0; block < 64; ++block) { for (int i = 0; i < 128; ++i) left.setSample(0, i, .12f * std::sin((block * 128 + i) * .07f)); right.makeCopyOf(left); a.process(left, true, 120, captures); b.process(right, true, 120, captures); for (int i = 0; i < 128; ++i) { require(std::isfinite(left.getSample(0, i)) && std::isfinite(right.getSample(0, i)), "Serial audio must remain finite"); difference += std::abs(left.getSample(0, i) - right.getSample(0, i)); } }
    require(difference > .1, "Serial order must affect actual audio");
    // Bypass must restore dry audio exactly after its click-free ramp settles.
    set(p, "OD_ON", 0); set(p, "EQ_ON", 0);
    for (int block = 0; block < 20; ++block) { left.clear(); a.process(left, true, 120, captures); }
    left.clear(); left.setSample(0, 3, .123f); a.process(left, true, 120, captures);
    require(left.getSample(0, 3) == .123f && left.getMagnitude(0, 0, 3) == 0, "Settled serial bypass must return exact dry audio");
    require(p.boardCommand("replace", args({{"id", second}, {"type", "chorus"}})).isEmpty(), "Replacement must allocate a new kind-qualified slot"); settle(p);
    require(static_cast<int>(state(p).getChildWithName("PEDALBOARD").getChildWithProperty("id", second)["deleted"]) == 1 && id(p, "chorus", 1).isNotEmpty(), "Replacement must reserve the old identity and assign a fresh one");
    require(p.boardCommand("undo", {}).isEmpty(), "Replacement must undo"); settle(p);
    require(p.boardCommand("moveTo", args({{"id", second}, {"beforeId", first}, {"lane", "post"}})).isEmpty(), "Drag placement must use stable target identity"); settle(p);
    const auto dragged = state(p).getChildWithName("PEDALBOARD");
    require(dragged.indexOf(dragged.getChildWithProperty("id", second)) < dragged.indexOf(dragged.getChildWithProperty("id", first)), "Drag placement must publish target order");
    // Undo conversion cannot revive a frozen fixed-chain echo.
    { AmpSuiteAudioProcessor returning(false); set(returning, "AMP_SOURCE", 4); set(returning, "GATE_ON", 0); set(returning, "REVERB_MIX", 0); set(returning, "DELAY_TIME", 40); set(returning, "DELAY_MIX", 100); set(returning, "DELAY_FEEDBACK", 0); returning.prepareToPlay(48000, 128);
      juce::AudioBuffer<float> signal(2, 128); signal.clear(); signal.setSample(0, 0, .2f); returning.renderGuitarOffline(signal, 128);
      require(returning.boardCommand("convert", {}).isEmpty(), "Tail test must convert"); settle(returning);
      for (int i = 0; i < 120; ++i) { signal.clear(); returning.renderGuitarOffline(signal, 128); }
      require(returning.boardCommand("undo", {}).isEmpty(), "Conversion must undo to the fixed path"); settle(returning);
      float peak = 0; for (int i = 0; i < 120; ++i) { signal.clear(); returning.renderGuitarOffline(signal, 128); peak = juce::jmax(peak, signal.getMagnitude(0, 128)); }
      require(peak < 1e-5f && !PedalboardState::serial(state(returning)), "Undo conversion must not replay frozen fixed effect tails");
      require(returning.boardCommand("convert", {}).isEmpty(), "Session test must convert again"); settle(returning);
      juce::MemoryBlock fixedSession; AmpSuiteAudioProcessor fixed(false); fixed.getStateInformation(fixedSession);
      returning.setStateInformation(fixedSession.getData(), static_cast<int>(fixedSession.getSize())); settle(returning);
      require(!PedalboardState::serial(state(returning)), "A complete fixed native session must replace a serial graph through prepared recall"); }

    // Same captured content must survive independent slots, scene/session and
    // deduplicated portable export/import, without aliasing the block identity.
    juce::TemporaryFile marker(".xml"); const auto root = marker.getFile().getSiblingFile(marker.getFile().getFileNameWithoutExtension() + "-library");
    { AmpSuiteAudioProcessor managed(true, root); managed.prepareToPlay(48000, 128); managed.importAssets({fixture}, "pedal");
      for (int i = 0; i < 600 && managed.getLibrary()["assets"].size() == 0; ++i) juce::Thread::sleep(10);
      const auto assetId = managed.getLibrary()["assets"][0]["id"];
      require(managed.boardCommand("convert", {}).isEmpty(), "Managed board must convert"); settle(managed);
      require(managed.boardCommand("capture", args({{"id", id(managed, "neural-pedal")}, {"assetId", assetId}})).isEmpty(), "Capture must assign to its slot"); settle(managed);
      set(managed, "PEDAL_ON", 1);
      require(managed.boardCommand("duplicate", args({{"id", id(managed, "neural-pedal")}})).isEmpty(), "Captured pedal must duplicate with independent model state"); settle(managed);
      require(state(managed)["pedalId"] == state(managed)["pedal1Id"], "Duplicated capture content must retain its hash");
      juce::TemporaryFile pack(".zip"); require(managed.exportRigPack(pack.getFile()).isEmpty(), "Serial capture pack must export");
      juce::ZipFile zip(pack.getFile()); require(zip.getNumEntries() == 2, "Repeated capture content must package once");
      AmpSuiteAudioProcessor imported(true, root); imported.prepareToPlay(44100, 128); require(imported.importRigPack(pack.getFile()).isEmpty(), "Serial capture pack must import");
      const auto rigId = imported.getLibrary()["rigs"][0]["id"].toString(); require(imported.loadRig(rigId).isEmpty(), "Imported serial graph must recall"); settle(imported);
      require(PedalboardState::equal(state(managed), state(imported)), "Portable recall must preserve both block identities");
      juce::AudioBuffer<float> signal(2, 128); const auto start = juce::Time::getMillisecondCounterHiRes();
      for (int block = 0; block < 400; ++block) { signal.clear(); for (int i = 0; i < 128; ++i) signal.setSample(0, i, .03f * std::sin((block * 128 + i) * .08f)); imported.renderGuitarOffline(signal, 128); require(std::isfinite(signal.getMagnitude(0, 128)), "Two neural pedals must render finite audio"); }
      std::cout << "Two-capture serial graph: " << (juce::Time::getMillisecondCounterHiRes() - start) / 400 << " ms / 128-frame block at 44.1 kHz\n";
      // Library Use must target a surviving serial slot rather than the deleted
      // legacy slot. The graph's fade must also leave bypassed captures dry.
      set(imported, "PEDAL_ON", 0); set(imported, "BOARD_NEURAL_PEDAL_1_PEDAL_ON", 0);
      auto bypassState = state(imported); auto captureBoard = PedalboardState::emptySerial();
      for (const auto& row : bypassState.getChildWithName("PEDALBOARD")) if (PedalboardState::kind(row) == 2) captureBoard.addChild(row.createCopy(), -1, nullptr);
      bypassState.removeChild(bypassState.getChildWithName("PEDALBOARD"), nullptr); bypassState.addChild(captureBoard, -1, nullptr);
      SerialPedalboard bypass(bypassState, imported.apvts, {44100, 128, 2});
      NamWrapper firstCapture(fixture), secondCapture(fixture); firstCapture.prepare(44100, 128); secondCapture.prepare(44100, 128);
      NamWrapper* models[] {&firstCapture, &secondCapture};
      for (int block = 0; block < 25; ++block) { signal.clear(); signal.setSample(0, 3, .123f); bypass.process(signal, true, 120, models); require(signal.getSample(0, 3) == .123f && signal.getMagnitude(0, 0, 3) == 0, "Bypassed captured pedals must preserve exact dry audio"); }
      set(imported, "BOARD_NEURAL_PEDAL_1_PEDAL_ON", 1);
      for (int block = 0; block < 25; ++block) { signal.clear(); signal.setSample(0, 3, .123f); bypass.process(signal, true, 120, models); require(std::isfinite(signal.getMagnitude(0, 128)), "Captured pedal must re-enable with finite audio"); }
      require(imported.boardCommand("remove", args({{"id", id(imported, "neural-pedal")}})).isEmpty(), "First capture slot must remove"); settle(imported);
      set(imported, "BOARD_NEURAL_PEDAL_1_PEDAL_ON", 0);
      require(imported.selectAsset(assetId.toString()), "Library Use must select the surviving capture slot"); settle(imported);
      require(imported.apvts.getRawParameterValue("BOARD_NEURAL_PEDAL_1_PEDAL_ON")->load() == 1 && imported.apvts.getRawParameterValue("PEDAL_ON")->load() == 0, "Library Use must enable only the surviving slot");
    }
    require(root.isAChildOf(marker.getFile().getParentDirectory()) && root.deleteRecursively(), "Temporary serial library must clean up");
    const auto actualRoot = juce::SystemStats::getEnvironmentVariable("CASSIAN_ACTUAL_SOUND_LIBRARY", "");
    if (actualRoot.isNotEmpty()) {
        LibraryStore actualStore {juce::File(actualRoot)}; const auto catalog = actualStore.load();
        const auto find = [&](const char* kind, const char* pack, const char* name) { for (const auto& asset : catalog) if (asset["kind"].toString() == kind && asset["pack"].toString().containsIgnoreCase(pack)
            && (asset["name"].toString().containsIgnoreCase(name) || (juce::String(kind) == "ambience" && static_cast<double>(asset["duration"]) >= 19))) return asset; return juce::ValueTree(); };
        const auto amp = find("amp", "Marshall", "Jcm800"), drive = find("pedal", "Classic Drive", "Klon"), ambient = find("ambience", "Tone Charm", "20");
        require(amp.isValid() && drive.isValid() && ambient.isValid(), "Actual benchmark requires the supplied head, pedal and 20-second ambience");
        AmpSuiteAudioProcessor heavy(false); heavy.prepareToPlay(48000, 128);
        auto saved = state(heavy); saved.removeChild(saved.getChildWithName("PEDALBOARD"), nullptr); auto graph = PedalboardState::emptySerial();
        for (int kind = 0; kind < 9; ++kind) { juce::ValueTree row("BLOCK"); row.setProperty("id", "actual." + BoardParams::types[kind], nullptr); row.setProperty("type", BoardParams::types[kind], nullptr); row.setProperty("automationSlot", 0, nullptr); row.setProperty("lane", kind < 3 ? "pre" : "post", nullptr); row.setProperty("deleted", 0, nullptr); graph.addChild(row, -1, nullptr); }
        auto extra = graph.getChild(2).createCopy(); extra.setProperty("id", "actual.pedal2", nullptr); extra.setProperty("automationSlot", 1, nullptr); graph.addChild(extra, 3, nullptr); saved.addChild(graph, -1, nullptr);
        for (const auto& pair : {std::pair<juce::String, juce::ValueTree>{"model", amp}, {"pedal", drive}, {"pedal1", drive}, {"ambience", ambient}}) { saved.setProperty(pair.first + "Path", pair.second["path"], nullptr); saved.setProperty(pair.first + "Id", pair.second["id"], nullptr); }
        saved.removeChild(saved.getChildWithName("LIBRARY"), nullptr); saved.addChild(catalog.createCopy(), -1, nullptr);
        for (const auto& pair : {std::pair<juce::String, float>{"AMP_SOURCE", 3}, {"CAPTURE_KIND", 3}, {"GATE_ON", 0}, {"PEDAL_ON", 1}, {"BOARD_NEURAL_PEDAL_1_PEDAL_ON", 1}, {"EQ_ON", 1}, {"MOD_ON", 1}, {"MOD_MIX", 15}, {"CHORUS_MIX", 10}, {"DELAY_MIX", 10}, {"REVERB_MIX", 8}, {"BOARD_AMBIENCE_0_ON", 1}}) saved.getChildWithProperty("id", pair.first).setProperty("value", pair.second, nullptr);
        require(heavy.applyRig(args({{"schema", 3}, {"state", saved.toXmlString()}})).isEmpty(), "Actual heavy graph must queue"); settle(heavy);
        for (const int frames : {128, 256, 512}) {
            heavy.prepareToPlay(48000, frames); juce::AudioBuffer<float> signal(2, frames); double total = 0, maximum = 0; float peak = 0; int overBudget = 0;
            const double budget = frames * 1000.0 / 48000;
            for (int block = 0; block < 800; ++block) {
                signal.clear(); for (int i = 0; i < frames; ++i) signal.setSample(0, i, .03f * std::sin((block * frames + i) * .04f));
                const auto start = juce::Time::getMillisecondCounterHiRes(); heavy.renderGuitarOffline(signal, frames); const auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;
                if (block >= 50) { total += elapsed; maximum = juce::jmax(maximum, elapsed); if (elapsed > budget) ++overBudget; }
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < frames; ++i) require(std::isfinite(signal.getSample(ch, i)), "Actual serial board must render finite audio");
                peak = juce::jmax(peak, signal.getMagnitude(0, frames));
            }
            require(peak > .00001f, "Actual captures must produce audible-range output");
            std::cout << "Actual JCM800 + two Klon engines + 20-second ambience + effects, offline guitar 48 kHz/" << frames << ": mean " << total / 750 << " ms, max " << maximum << " ms; block budget " << budget << " ms; over budget " << overBudget << "/750; peak " << peak << '\n';
        }
    }
    std::cout << "Serial independent controls, routing, bypass, slot reservation, undo/redo, scenes, native recall and capture packs passed\n";
}
