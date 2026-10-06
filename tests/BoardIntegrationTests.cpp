#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
juce::ValueTree unwrap(const juce::var& rig) {
    const auto xml = juce::XmlDocument::parse(rig["state"].toString());
    require(xml != nullptr, "Board snapshot must contain XML"); return juce::ValueTree::fromXml(*xml);
}
juce::var wrap(const juce::ValueTree& state, int schema = 2) {
    auto object = std::make_unique<juce::DynamicObject>(); object->setProperty("schema", schema);
    object->setProperty("state", state.toXmlString()); return juce::var(object.release());
}
void settle(AmpSuiteAudioProcessor& p) {
    for (int n = 0; n < 1600; ++n) {
        if (!p.getRig().hasProperty("error") && !p.status()["message"].toString().contains("Restoring")) return;
        juce::Thread::sleep(5);
    }
    require(false, "Board integration asset preparation timed out");
}
void stripBoard(juce::ValueTree& state) { state.removeChild(state.getChildWithName("PEDALBOARD"), nullptr); }
void pack(const juce::File& file, const juce::var& rig) {
    juce::TemporaryFile document(".json"); require(document.getFile().replaceWithText(juce::JSON::toString(rig)), "Pack test document must write");
    juce::ZipFile::Builder builder; builder.addFile(document.getFile(), 6, "rig.cassian.json");
    auto stream = file.createOutputStream(); require(stream && builder.writeToStream(*stream, nullptr), "Board test pack must write");
}
void renderEqual(const juce::var& legacy, const juce::var& current, double rate) {
    AmpSuiteAudioProcessor a(false), b(false);
    a.setNonRealtime(true); b.setNonRealtime(true);
    require(a.applyRig(legacy, false).isEmpty() && b.applyRig(current, false).isEmpty(), "Legacy and current rig must both prepare");
    settle(a); settle(b); a.prepareToPlay(rate, 128); b.prepareToPlay(rate, 128);
    juce::AudioBuffer<float> left(2, 128), right(2, 128); double energy = 0, difference = 0;
    for (int block = 0; block < 120; ++block) {
        left.clear();
        for (int i = 0; i < 128; ++i) {
            const auto t = static_cast<float>(block * 128 + i) / static_cast<float>(rate);
            left.setSample(0, i, .03f * std::sin(juce::MathConstants<float>::twoPi * 220.f * t)
                                 + .015f * std::sin(juce::MathConstants<float>::twoPi * 73.f * t));
        }
        right.makeCopyOf(left); a.renderGuitarOffline(left, 128); b.renderGuitarOffline(right, 128);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) {
            require(std::isfinite(left.getSample(ch, i)) && std::isfinite(right.getSample(ch, i)), "Migrated audio must remain finite");
            difference = std::max(difference, static_cast<double>(std::abs(left.getSample(ch, i) - right.getSample(ch, i))));
            energy += left.getSample(ch, i) * left.getSample(ch, i);
        }
    }
    require(energy > .0001 && difference == 0, "State migration must preserve actual clean/driven/effect output exactly");
}
}

void runBoardIntegrationChecks(const juce::File& fixture)
{
    AmpSuiteAudioProcessor p(false); p.prepareToPlay(48000, 128);
    const auto initial = p.getRig(); const auto state = unwrap(initial);
    require(static_cast<int>(initial["schema"]) == 3 && state.getChildWithName("PEDALBOARD").isValid(), "New snapshots must use rig schema 3 and contain a board");
    require(p.getParameters().size() == 87 + static_cast<int>(BoardParams::definitions().size()), "Independent controls must append after the 87 legacy host parameters");
    for (size_t i = 0; i < Params::definitions.size(); ++i) {
        auto* parameter = dynamic_cast<juce::AudioProcessorParameterWithID*>(p.getParameters()[static_cast<int>(i)]);
        require(parameter && parameter->paramID == Params::definitions[i].id, "Existing automation positions must retain their parameter IDs");
    }
    juce::ValueTree migrated;
    auto oldest = state.createCopy(); stripBoard(oldest);
    for (size_t i = 41; i < Params::definitions.size(); ++i) oldest.removeChild(oldest.getChildWithProperty("id", Params::definitions[i].id), nullptr);
    require(p.migrateRigDocument(wrap(oldest, 1), migrated).isEmpty(), "First-41-control rigs must migrate on an isolated tree");
    require(PedalboardState::equal(state, migrated), "Legacy rigs must gain deterministic board identities");
    for (size_t i = 41; i < Params::definitions.size(); ++i)
        require(static_cast<float>(migrated.getChildWithProperty("id", Params::definitions[i].id)["value"]) == Params::definitions[i].initial, "Newer controls must retain established legacy defaults");
    require(!p.validateRigDocument(wrap(oldest)).isEmpty(), "Schema-2 rigs cannot omit their board or controls");
    auto fractional = wrap(state); fractional.getDynamicObject()->setProperty("schema", 1.5);
    require(!p.validateRigDocument(fractional).isEmpty(), "Fractional schemas cannot silently select a legacy parser");
    auto wrapped = wrap(state); wrapped.getDynamicObject()->setProperty("schema", static_cast<juce::int64>(4294967297LL));
    require(!p.validateRigDocument(wrapped).isEmpty(), "Large schema values cannot wrap to a supported version");
    auto missing = state.createCopy(); missing.removeChild(missing.getChildWithProperty("id", "OD_ON"), nullptr);
    require(!p.validateRigDocument(wrap(missing)).isEmpty(), "Complete schema-2 rigs must contain all effect controls");
    auto routing = state.createCopy(); routing.getChildWithProperty("id", "MOD_TYPE").setProperty("value", .5, nullptr);
    require(!p.validateRigDocument(wrap(routing, 1)).isEmpty(), "Fractional modulation choices must reject legacy recalls too");

    // Valid custom identities survive A/B, saving and native session recall.
    auto identified = state.createCopy(); identified.getChildWithName("PEDALBOARD").getChild(1).setProperty("id", "user-overdrive-01", nullptr);
    require(p.applyRig(wrap(identified)).isEmpty(), "Valid block identities must recall"); settle(p);
    require(p.saveRig("Board identity").isEmpty(), "Rig must save stable block identities");
    const auto savedId = p.status()["activeRigId"].toString();
    require(savedId.isNotEmpty() && static_cast<bool>(p.status()["activeRigSaved"]) && p.status()["activeRigName"].toString() == "Board identity", "Saved rig must expose its actual identity and saved status");
    require(PedalboardState::equal(identified, unwrap(p.getRig())), "A/B snapshot must preserve custom block identities");
    juce::MemoryBlock session; p.getStateInformation(session); AmpSuiteAudioProcessor restored(false);
    restored.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(restored);
    require(PedalboardState::equal(identified, unwrap(restored.getRig())) && restored.status()["activeRigId"].toString() == savedId && !static_cast<bool>(restored.status()["activeRigEdited"]), "Native identity and saved baseline must round-trip without a false edit");

    const auto baseline = p.getRig(); const auto bank = p.scenes.save()["json"].toString();
    auto invalid = unwrap(baseline); invalid.getChildWithProperty("id", "DRIVE_GAIN").setProperty("value", 21, nullptr);
    invalid.getChildWithName("PEDALBOARD").setProperty("runtime", "serial-v1", nullptr);
    require(!p.applyRig(wrap(invalid)).isEmpty() && p.getRig()["state"].toString() == baseline["state"].toString(), "Unsupported boards must reject a complete recall atomically");
    juce::AudioProcessor::copyXmlToBinary(*invalid.createXml(), session); p.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    require(p.getRig()["state"].toString() == baseline["state"].toString() && p.status()["activeRigId"].toString() == savedId && p.scenes.save()["json"].toString() == bank, "Unsupported native boards must leave sound, identity and scenes unchanged");
    // The catalog retains each saved entry's format, including after native XML
    // round-trip, rather than treating a broken modern entry as a legacy rig.
    p.getStateInformation(session); const auto catalogXml = juce::AudioProcessor::getXmlFromBinary(session.getData(), static_cast<int>(session.getSize()));
    require(catalogXml != nullptr, "Saved catalog session must decode"); auto corruptCatalog = juce::ValueTree::fromXml(*catalogXml);
    auto entry = corruptCatalog.getChildWithName("LIBRARY").getChildWithProperty("id", savedId);
    require(entry.hasType("RIG") && entry["schema"].toString() == "3", "Saved entries must retain the strict rig format");
    auto incomplete = unwrap(baseline); stripBoard(incomplete); entry.setProperty("state", incomplete.toXmlString(), nullptr);
    juce::AudioProcessor::copyXmlToBinary(*corruptCatalog.createXml(), session);
    AmpSuiteAudioProcessor catalogReader(false); catalogReader.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(catalogReader);
    const auto intact = catalogReader.getRig();
    require(!static_cast<bool>(catalogReader.status()["activeRigEdited"]), "Invalid saved catalog metadata must not replace a valid session baseline");
    require(!catalogReader.loadRig(savedId).isEmpty() && catalogReader.getRig()["state"].toString() == intact["state"].toString(), "Broken schema-2 catalog entries must reject without legacy downgrade");
    juce::TemporaryFile destination(".cassian.zip"); require(destination.getFile().replaceWithText("existing pack sentinel"), "Pack sentinel must write");
    const auto hash = juce::SHA256(destination.getFile()).toHexString();
    require(!p.exportRigPack(destination.getFile(), wrap(invalid)).isEmpty() && juce::SHA256(destination.getFile()).toHexString() == hash, "Invalid snapshots must reject before overwriting a pack");

    // Sparse legacy host states use their historical defaults and gain a board.
    const auto sparse = juce::ValueTree::fromXml(R"(<AmpSuiteState><PARAM id="AMP_CLEAN" value="1"/></AmpSuiteState>)");
    juce::AudioProcessor::copyXmlToBinary(*sparse.createXml(), session); restored.setStateInformation(session.getData(), static_cast<int>(session.getSize())); settle(restored);
    require(restored.apvts.getRawParameterValue("AMP_CLEAN")->load() == 1 && restored.apvts.getRawParameterValue("OD_ON")->load() == 0 && PedalboardState::equal(sparse, unwrap(restored.getRig())), "Sparse host sessions must keep clean and bypass defaults with a migrated board");

    // Old packs migrate without activating a rig; unsupported packs write no assets.
    struct Folder {
        juce::File base = juce::File::getSpecialLocation(juce::File::tempDirectory);
        juce::File root = base.getNonexistentChildFile("CassianBoardTests", "", false);
        ~Folder() { if (root.isAChildOf(base)) root.deleteRecursively(); }
    } folder;
    AmpSuiteAudioProcessor importer(true, folder.root); importer.prepareToPlay(48000, 128);
    juce::TemporaryFile portable(".cassian.zip"); pack(portable.getFile(), wrap(oldest, 1));
    require(importer.importRigPack(portable.getFile()).isEmpty(), "A schema-1 pack must migrate into the library");
    const auto rigs = importer.getLibrary()["rigs"]; require(rigs.size() == 1, "Legacy pack must create one saved rig");
    require(importer.loadRig(rigs[0]["id"].toString()).isEmpty(), "Migrated pack must recall"); settle(importer);
    require(PedalboardState::equal(state, unwrap(importer.getRig())), "Imported old pack must acquire the deterministic board");
    require(importer.exportRigPack(destination.getFile()).isEmpty(), "Migrated packs must export with the new rig format");
    juce::ZipFile archive(destination.getFile()); std::unique_ptr<juce::InputStream> document(archive.createStreamForEntry(0));
    const auto exported = juce::JSON::parse(document->readEntireStreamAsString());
    require(static_cast<int>(exported["schema"]) == 3 && PedalboardState::equal(state, unwrap(exported)), "Pack document must preserve its versioned board");
    portable.getFile().deleteFile(); pack(portable.getFile(), wrap(invalid));
    const auto fileCount = folder.root.findChildFiles(juce::File::findFiles, true).size();
    require(!importer.importRigPack(portable.getFile()).isEmpty() && importer.getLibrary()["rigs"].size() == 1 && folder.root.findChildFiles(juce::File::findFiles, true).size() == fileCount, "Unsupported pack boards must not create files or rigs");

    // Test all compressor placements through the legacy and explicit amp paths.
    // Same DSP/input must be bit-identical after metadata-only migration.
    auto sound = state.createCopy();
    sound.setProperty("pedalPath", fixture.getFullPathName(), nullptr); sound.setProperty("pedalId", "", nullptr);
    sound.setProperty("modelPath", fixture.getFullPathName(), nullptr); sound.setProperty("modelId", "", nullptr);
    const auto assign = [&](const char* id, float x) { sound.getChildWithProperty("id", id).setProperty("value", x, nullptr); };
    for (const auto* id : {"OD_ON", "PEDAL_ON", "EQ_ON", "MOD_ON"}) assign(id, 1);
    assign("GATE_ON", 0); assign("CAB_MODE", 3); assign("CAPTURE_MATCH", 0);
    assign("CHORUS_MIX", 18); assign("DELAY_TIME", 40); assign("DELAY_MIX", 20); assign("REVERB_MIX", 12);
    assign("EQ_MUD", -3); assign("DRIVE_GAIN", 4); assign("CLEAN_COMP", 45); assign("MICRO_DELAY", .25);
    for (int source = 0; source <= 4; ++source) for (int compressor = 0; compressor <= 3; ++compressor) {
        assign("AMP_SOURCE", static_cast<float>(source)); assign("AMP_CLEAN", source == 0 ? 1.f : 0.f); assign("COMP_MODE", static_cast<float>(compressor));
        auto old = sound.createCopy(); stripBoard(old); renderEqual(wrap(old, 1), wrap(sound), 48000);
    }
    assign("AMP_SOURCE", 1); assign("COMP_MODE", 2);
    for (const double rate : {44100., 96000.}) { auto old = sound.createCopy(); stripBoard(old); renderEqual(wrap(old, 1), wrap(sound), rate); }
    std::cout << "Board integration, atomic rejection, legacy/session/pack migrations and exact audio compatibility checks passed\n";
}
