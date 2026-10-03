#include "../Source/PluginProcessor.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
void set(AmpSuiteAudioProcessor& p, const char* id, float value) {
    auto* parameter = p.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(AmpSuiteAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue(id)->load(); }
void settle(AmpSuiteAudioProcessor& p) {
    for (int i = 0; i < 600; ++i) {
        if (!p.getRig().hasProperty("error") && !p.status()["message"].toString().contains("Restoring")) return;
        juce::Thread::sleep(10);
    }
    require(false, "Rig assets must finish loading");
}
void load(AmpSuiteAudioProcessor& p, const juce::File& file, bool pedal = false) {
    if (pedal) p.requestPedal(file); else p.requestFile(true, file);
    settle(p);
    require(p.status()[pedal ? "pedal" : "model"].toString() == file.getFileName(), "NAM fixture must load for library checks");
}
juce::var wrap(const juce::ValueTree& state) {
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("schema", 1); object->setProperty("state", state.createXml()->toString());
    return juce::var(object.release());
}
juce::ValueTree unwrap(const juce::var& rig) {
    const auto xml = juce::XmlDocument::parse(rig["state"].toString());
    require(xml != nullptr, "Saved rig must contain XML state"); return juce::ValueTree::fromXml(*xml);
}
std::vector<float> render(AmpSuiteAudioProcessor& p, double rate = 48000, float frequency = 440) {
    p.prepareToPlay(rate, 128);
    juce::AudioBuffer<float> audio(2, 128); juce::MidiBuffer midi; std::vector<float> result;
    for (int block = 0; block < 160; ++block) {
        audio.clear();
        for (int i = 0; i < 128; ++i) audio.setSample(0, i, .025f * std::sin(juce::MathConstants<float>::twoPi * frequency * static_cast<float>(block * 128 + i) / static_cast<float>(rate)));
        p.processBlock(audio, midi);
        if (block >= 120) for (int i = 0; i < 128; ++i) {
            const float x = audio.getSample(0, i); require(std::isfinite(x) && std::abs(x) <= 1, "Universal amp output must be finite and bounded"); result.push_back(x);
        }
    }
    return result;
}
double difference(const std::vector<float>& a, const std::vector<float>& b) {
    require(a.size() == b.size(), "Render sizes must match"); double error = 0;
    for (size_t i = 0; i < a.size(); ++i) error = std::max(error, static_cast<double>(std::abs(a[i] - b[i])));
    return error;
}
double energy(const std::vector<float>& a) { double sum = 0; for (const auto x : a) sum += x * x; return std::sqrt(sum / a.size()); }
void dry(AmpSuiteAudioProcessor& p) {
    set(p, "GATE_ON", 0); set(p, "REVERB_MIX", 0); set(p, "MASTER_VOL", -12); set(p, "CLEAN_COMP", 0); set(p, "CAB_MODE", 3);
}
}
void runLibraryChecks(const juce::File& fixture)
{
    // Renaming and duplicate imports must not erase a user's metadata or favorite.
    juce::TemporaryFile original(".nam"), relocated(".nam"), wrong(".nam"), impulse(".wav");
    require(fixture.copyFileTo(original.getFile()) && fixture.copyFileTo(relocated.getFile()), "Temporary captures must copy");
    wrong.getFile().replaceWithText("different content");
    const auto descriptor = AssetLibrary::describe(original.getFile(), "amp");
    const auto renamed = AssetLibrary::describe(relocated.getFile(), "amp");
    require(descriptor["id"] == renamed["id"], "Renamed copies must have the same stable asset ID");
    require(descriptor["ownership"].toString() == "User", "User files must not become cleared factory assets");
    AssetLibrary catalog; catalog.upsert(descriptor);
    auto row = catalog.find(descriptor["id"]); row.setProperty("name", "Favorite clean amp", nullptr); row.setProperty("favorite", true, nullptr);
    catalog.upsert(renamed);
    require(catalog.tree.getNumChildren() == 1 && row["name"].toString() == "Favorite clean amp" && static_cast<bool>(row["favorite"]), "Duplicate imports must preserve names and favorites");
    require(catalog.idForPath("amp", original.getFile().getFullPathName()) == descriptor["id"].toString(), "Importing a renamed duplicate must not erase the playing rig's stable reference");

    // Batch importing is catalog-only; rejected files don't become library entries.
    AmpSuiteAudioProcessor batch; dry(batch);
    batch.importAssets({original.getFile(), relocated.getFile(), wrong.getFile()}, "amp");
    for (int i = 0; i < 600 && !batch.status()["message"].toString().startsWith("Load failed:"); ++i) juce::Thread::sleep(10);
    require(batch.getLibrary()["assets"].size() == 1 && batch.status()["model"].toString().isEmpty(), "Batch import must deduplicate valid assets without activating them");

    // NAM captures and pedals work in an explicit slot even with the old clean flag on.
    AmpSuiteAudioProcessor capture; dry(capture); load(capture, original.getFile());
    set(capture, "AMP_SOURCE", 3); set(capture, "AMP_CLEAN", 0);
    const auto metalFlag = render(capture); set(capture, "AMP_CLEAN", 1);
    require(difference(metalFlag, render(capture)) < 1e-5, "Old clean routing must not bypass an explicit NAM capture");
    load(capture, original.getFile(), true); set(capture, "PEDAL_ON", 1);
    require(difference(metalFlag, render(capture)) > .00001, "A pedal must affect a NAM capture with the clean flag set");
    set(capture, "AMP_SOURCE", 1); set(capture, "PEDAL_ON", 0); const auto clean = render(capture);
    set(capture, "PEDAL_ON", 1);
    require(difference(clean, render(capture)) > .00001, "Lumen's explicit amp slot must accept pedals");
    AmpSuiteAudioProcessor empty; dry(empty); set(empty, "AMP_SOURCE", 3);
    require(energy(render(empty)) == 0, "A missing explicit NAM must not silently substitute a different amp");
    // A live algorithm change must ramp down rather than chop the waveform.
    AmpSuiteAudioProcessor switching; dry(switching); set(switching, "AMP_SOURCE", 4); switching.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> transition(2, 128); juce::MidiBuffer midi;
    float previous = 0, largestStep = 0;
    for (int block = 0; block < 80; ++block) {
        if (block == 50) set(switching, "AMP_SOURCE", 3); // deliberately empty NAM slot
        transition.clear(); for (int i = 0; i < 128; ++i) transition.setSample(0, i, .02f);
        switching.processBlock(transition, midi);
        for (int i = 0; i < 128; ++i) {
            const auto x = transition.getSample(0, i);
            if (block >= 50) largestStep = std::max(largestStep, std::abs(x - previous));
            previous = x;
        }
    }
    require(largestStep < .0001f && transition.getMagnitude(0, 0, 128) < 1e-6f, "Amp slot changes must fade smoothly to the new algorithm");

    // Natural DI preserves the low end across rates; electric amp drive doesn't color it.
    AmpSuiteAudioProcessor natural; dry(natural); set(natural, "AMP_SOURCE", 4);
    for (const double rate : {44100., 48000., 96000.}) {
        set(natural, "DRIVE_GAIN", 0); const auto flat = render(natural, rate, 80);
        set(natural, "DRIVE_GAIN", 24); set(natural, "AMP_CLEAN", 1);
        require(difference(flat, render(natural, rate, 80)) < 1e-6, "Natural DI must ignore electric amp drive and channel routing");
        require(energy(flat) > .001, "Natural DI must preserve audible bass notes");
    }

    // A deterministic IR changes phase, making double-cabinet routing observable.
    {
        juce::WavAudioFormat format; auto stream = impulse.getFile().createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 1, 24, {}, 0));
        require(writer != nullptr, "Temporary IR writer must open");
        juce::AudioBuffer<float> ir(1, 96); ir.clear(); ir.setSample(0, 24, -1);
        require(writer->writeFromAudioSampleBuffer(ir, 0, 96), "Temporary IR must write");
    }
    set(capture, "AMP_SOURCE", 3); set(capture, "PEDAL_ON", 0); set(capture, "CAPTURE_KIND", 3); set(capture, "CAB_MODE", 3);
    const auto cabOff = render(capture);
    capture.requestFile(false, impulse.getFile()); settle(capture);
    set(capture, "CAB_MODE", 0);
    require(difference(cabOff, render(capture)) < 1e-5, "Auto must bypass an external IR for full-rig captures");
    set(capture, "CAB_MODE", 1);
    const auto forced = render(capture);
    require(difference(cabOff, forced) > .00001, "External IR override must intentionally process a full-rig capture");

    // Named rigs restore every tone parameter and all three assets. Calibration,
    // listening volume, and practice tempo remain under the player's control.
    set(capture, "EQ_ON", 1); set(capture, "EQ_FIZZ", -7); set(capture, "DELAY_TIME", 450);
    set(capture, "PEDAL_ON", 1); set(capture, "INPUT_GAIN", 3); set(capture, "MASTER_VOL", -18); set(capture, "METRO_BPM", 90);
    const auto saved = capture.getRig(); require(!saved.hasProperty("error"), "A ready rig must export");
    const auto snapshot = unwrap(saved);
    require(snapshot["modelId"].toString().isNotEmpty() && snapshot["pedalId"].toString().isNotEmpty() && snapshot["irId"].toString().isNotEmpty(), "Every loaded stage needs a stable reference");
    require(capture.saveRig("Complete lead").isEmpty(), "Named rig must save");
    const auto rigId = capture.getLibrary()["rigs"][0]["id"].toString();
    set(capture, "AMP_SOURCE", 4); set(capture, "EQ_FIZZ", 0); set(capture, "INPUT_GAIN", 7); set(capture, "MASTER_VOL", -24); set(capture, "METRO_BPM", 130);
    require(capture.loadRig(rigId).isEmpty(), "Named rig must recall"); settle(capture);
    require(get(capture, "AMP_SOURCE") == 3 && get(capture, "CAPTURE_KIND") == 3 && get(capture, "CAB_MODE") == 1 && get(capture, "EQ_FIZZ") == -7 && get(capture, "DELAY_TIME") == 450 && get(capture, "PEDAL_ON") == 1, "Rig recall must restore routing and effects");
    require(get(capture, "INPUT_GAIN") == 7 && get(capture, "MASTER_VOL") == -24 && get(capture, "METRO_BPM") == 130, "Rig recall must preserve calibration, master, and tempo");
    juce::MemoryBlock session; capture.getStateInformation(session);
    AmpSuiteAudioProcessor reopened; reopened.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(reopened);
    require(reopened.getLibrary()["rigs"].size() == 1 && reopened.status()["ir"].toString() == impulse.getFile().getFileName(), "Host state must retain the catalog, saved rigs, and cabinet");
    require(reopened.saveRig("Second rig").isEmpty(), "Second rig must save");
    const auto state = unwrap(reopened.getRig());
    require(!state.getChildWithName("LIBRARY").getChildWithName("RIG").isValid(), "Rig exports must not recursively include other saved rigs");

    // Reject corrupt documents before changing the current sound.
    for (const auto value : {"99", "1.5", "nan", "wrong"}) {
        auto invalid = snapshot.createCopy(); invalid.getChildWithProperty("id", "AMP_SOURCE").setProperty("value", value, nullptr);
        require(!reopened.applyRig(wrap(invalid)).isEmpty() && get(reopened, "AMP_SOURCE") == 3, "Invalid routing must leave the current rig untouched");
    }
    auto incomplete = snapshot.createCopy(); incomplete.removeChild(incomplete.getChildWithProperty("id", "CAB_MODE"), nullptr);
    require(!reopened.applyRig(wrap(incomplete)).isEmpty(), "Incomplete rig documents must be rejected");
    auto duplicate = snapshot.createCopy(); duplicate.addChild(duplicate.getChildWithProperty("id", "AMP_SOURCE").createCopy(), -1, nullptr);
    require(!reopened.applyRig(wrap(duplicate)).isEmpty(), "Duplicate rig parameters must be rejected");

    // Missing assets can be catalogued, relinked by content, and recalled by ID.
    auto missing = snapshot.createCopy(); missing.setProperty("modelPath", original.getFile().getSiblingFile("missing-capture.nam").getFullPathName(), nullptr);
    auto missingEntry = missing.getChildWithName("LIBRARY").getChildWithProperty("id", missing["modelId"]);
    missingEntry.setProperty("path", missing["modelPath"], nullptr);
    AmpSuiteAudioProcessor imported; dry(imported); set(imported, "AMP_SOURCE", 4);
    require(imported.importRig("Moved lead", wrap(missing)).isEmpty(), "Import must allow missing assets for later relinking");
    const auto importedRig = imported.getLibrary()["rigs"][0]["id"].toString();
    require(!imported.loadRig(importedRig).isEmpty() && get(imported, "AMP_SOURCE") == 4, "Missing assets must leave current parameters intact");
    require(!imported.relinkAsset(missing["modelId"], wrong.getFile()).isEmpty(), "Relink must reject unrelated files");
    require(imported.relinkAsset(missing["modelId"], relocated.getFile()).isEmpty(), "Relink must recognize renamed original content");
    require(imported.loadRig(importedRig).isEmpty(), "Relinked rig must recall"); settle(imported);
    require(imported.status()["model"].toString() == relocated.getFile().getFileName(), "Rig must resolve the relocated model by ID");

    auto old = juce::ValueTree::fromXml(R"(<AmpSuiteState><PARAM id="AMP_CLEAN" value="1"/></AmpSuiteState>)");
    juce::AudioProcessor::copyXmlToBinary(*old.createXml(), session);
    imported.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    require(get(imported, "AMP_SOURCE") == 0 && get(imported, "CAB_MODE") == 0 && get(imported, "CAPTURE_KIND") == 0, "Old host sessions must keep their original routing defaults");
    std::cout << "Library, universal amp, and complete rig checks passed\n";
}
