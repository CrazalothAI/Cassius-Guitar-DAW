#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void wave(const juce::File& file, int channels) {
    juce::WavAudioFormat format; auto stream = file.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, static_cast<unsigned>(channels), 32, {}, 0));
    require(writer != nullptr, "Backup fixture WAV writer must open"); juce::AudioBuffer<float> audio(channels, 4800);
    for (int c = 0; c < channels; ++c) for (int i = 0; i < 4800; ++i) audio.setSample(c, i, .1f * std::sin(static_cast<float>(i) * .05f));
    require(writer->writeFromAudioSampleBuffer(audio, 0, 4800), "Backup fixture audio must write");
}
template<typename F> void rejected(F run) { bool failed = false; try { run(); } catch (const std::exception&) { failed = true; } require(failed, "Invalid/cancelled backup must reject"); }
juce::ValueTree read(const juce::File& file) { auto xml = juce::XmlDocument::parse(file); require(xml != nullptr, "Recovery catalog must parse"); return juce::ValueTree::fromXml(*xml); }
juce::ValueTree readTreeFromXML(const juce::String& text) { auto xml = juce::XmlDocument::parse(text); require(xml != nullptr, "Fixture/recovered state must parse"); return juce::ValueTree::fromXml(*xml); }
void mutateArchive(const juce::File& original, const juce::File& output, const juce::String& replaceName, const juce::String& newName, bool corrupt = false) {
    juce::ZipFile zip(original); juce::ZipFile::Builder builder;
    for (int i = 0; i < zip.getNumEntries(); ++i) {
        const auto name = zip.getEntry(i)->filename; std::unique_ptr<juce::InputStream> input(zip.createStreamForEntry(i));
        juce::MemoryBlock bytes; input->readIntoMemoryBlock(bytes);
        if (name == replaceName && corrupt && bytes.getSize() > 0) static_cast<char*>(bytes.getData())[0] ^= 1;
        builder.addEntry(new juce::MemoryInputStream(bytes, true), 0, name == replaceName ? newName : name, juce::Time::getCurrentTime());
    }
    auto stream = output.createOutputStream(); require(stream && builder.writeToStream(*stream, nullptr), "Malformed backup fixture must write");
}
}
void runBackupChecks(const juce::File& model)
{
    const auto parent = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto base = parent.getChildFile("Cassian-backup-tests-" + juce::Uuid().toString()); require(base.createDirectory().wasOk(), "Backup fixture root must create");
    struct Cleanup {juce::File folder, parent; ~Cleanup() {if (folder.isAChildOf(parent)) folder.deleteRecursively();}} cleanup {base, parent};
    const auto source = base.getChildFile("Source library"), target = base.getChildFile("Target library"), originals = base.getChildFile("External take");
    require(source.createDirectory().wasOk() && target.createDirectory().wasOk() && originals.createDirectory().wasOk(), "Fixture folders must create");
    auto validator = std::make_unique<AmpSuiteAudioProcessor>(false); const auto validate = [&](const juce::var& doc) {return validator->validateRigDocument(doc);};
    require(validator->storeScene(0,"Clean variation").isEmpty(), "Backup fixture scene must store");
    auto rig = validator->getRig(); auto state = readTreeFromXML(rig["state"].toString());
    auto asset = AssetLibrary::describe(model, "amp"); LibraryStore store(source); store.manage(asset);
    state.setProperty("modelPath", model.getFullPathName(), nullptr); state.setProperty("modelId", asset["id"], nullptr);
    auto assets = state.getChildWithName("LIBRARY"); if (!assets.isValid()) { assets = juce::ValueTree("LIBRARY"); state.addChild(assets, -1, nullptr); } assets.addChild(asset.createCopy(), -1, nullptr);
    rig.getDynamicObject()->setProperty("state", state.toXmlString());
    juce::ValueTree library("LIBRARY"), saved("RIG"); saved.setProperty("id", "original-rig", nullptr); saved.setProperty("name", "Lead <notes>", nullptr); saved.setProperty("schema", 3, nullptr); saved.setProperty("notes", "Keep / my notes", nullptr); saved.setProperty("favorite", true, nullptr); saved.setProperty("state", rig["state"], nullptr);
    library.addChild(asset.createCopy(), -1, nullptr); library.addChild(saved, -1, nullptr); store.save(library);
    wave(originals.getChildFile("Guitar dry.wav"), 1); wave(originals.getChildFile("Guitar processed.wav"), 2); wave(originals.getChildFile("Backing track.wav"), 2); wave(originals.getChildFile("Reamp.wav"), 2);
    originals.getChildFile("Original rig.json").replaceWithText(juce::JSON::toString(rig)); originals.getChildFile("Reamp.json").replaceWithText(juce::JSON::toString(rig));
    juce::ValueTree takes("TAKES"), take("TAKE"), version("REAMP"); take.setProperty("id", "old-take", nullptr); take.setProperty("name", "Practice", nullptr); take.setProperty("notes", "Best solo", nullptr); take.setProperty("favorite", true, nullptr); take.setProperty("path", originals.getFullPathName(), nullptr);
    version.setProperty("id", "reamp-1", nullptr); version.setProperty("name", "Tighter lead", nullptr); version.setProperty("path", originals.getChildFile("Reamp.wav").getFullPathName(), nullptr); version.setProperty("rigPath", originals.getChildFile("Reamp.json").getFullPathName(), nullptr); take.addChild(version, -1, nullptr); takes.addChild(take, -1, nullptr);
    source.getChildFile("takes.xml").replaceWithText(takes.toXmlString());
    const auto section = juce::SHA256(originals.getChildFile("Guitar processed.wav")).toHexString() + ".json";
    source.getChildFile("take-sections").createDirectory(); source.getChildFile("take-sections").getChildFile(section).replaceWithText("{\"version\":1,\"sections\":[]}");
    const auto destination = base.getChildFile("Personal.cassian-backup.zip"); std::atomic<bool> cancelled {false};
    const auto sourceHash = juce::SHA256(source.getChildFile("library.xml")).toHexString(), audioHash = juce::SHA256(originals.getChildFile("Guitar dry.wav")).toHexString();
    const auto report = LibraryBackup::create(source, destination, rig, cancelled);
    require(report.takes == 1 && report.files >= 9 && destination.existsAsFile(), "Backup must include external recordings, snapshots, reamps and sound files");
    rejected([&] {LibraryBackup::create(source, originals.getChildFile("Guitar dry.wav"), rig, cancelled);});
    require(juce::SHA256(originals.getChildFile("Guitar dry.wav")).toHexString() == audioHash, "Backup destination must never replace a referenced external recording");
    juce::ZipFile archive(destination); require(archive.getIndexOfFileName("backup.json") >= 0, "Backup must include its integrity manifest");
    // Archive is portable: no original machine paths in the library metadata.
    std::unique_ptr<juce::InputStream> portable(archive.createStreamForEntry(archive.getIndexOfFileName("library.xml")));
    const auto portableText = portable->readEntireStreamAsString(); require(!portableText.contains(source.getFullPathName()) && !portableText.contains(model.getFullPathName()), "Portable catalog must rewrite paths, not keep machine references");
    juce::ValueTree current("LIBRARY"), kept("RIG"); kept.setProperty("id", "original-rig", nullptr); kept.setProperty("name", "Existing rig", nullptr); current.addChild(kept, -1, nullptr);
    target.getChildFile("library.xml").replaceWithText(current.toXmlString()); target.getChildFile("takes.xml").replaceWithText("<TAKES/>");
    target.getChildFile("take-sections").createDirectory(); target.getChildFile("take-sections").getChildFile(section).replaceWithText("existing sections");
    const auto recovered = LibraryBackup::restore(destination, target, cancelled, validate);
    const auto restored = read(target.getChildFile("library.xml")), restoredTakes = read(target.getChildFile("takes.xml"));
    require(recovered.rigs == 2 && recovered.takes == 1 && recovered.location.isDirectory(), "Restore must add saved and current-tone copies");
    require(restored.getChildWithProperty("id", "original-rig")["name"].toString() == "Existing rig", "Restore must preserve conflicting saved rig identity");
    const auto restoredTake = restoredTakes.getChild(0); require(restoredTake["id"].toString() != "old-take" && restoredTake["notes"].toString() == "Best solo" && static_cast<bool>(restoredTake["favorite"]), "Restore must preserve take metadata with a fresh identity");
    const juce::File folder(restoredTake["path"].toString()); require(juce::SHA256(folder.getChildFile("Guitar dry.wav")).toHexString() == audioHash && folder.isAChildOf(recovered.location), "Recovered audio must match exactly and be isolated");
    require(juce::File(restoredTake.getChild(0)["rigPath"].toString()).isAChildOf(folder), "Reamp snapshots must relocate with their audio");
    require(target.getChildFile("take-sections").getChildFile(section).loadFileAsString() == "existing sections", "Recovery must preserve current review section conflicts");
    auto recoveredRig = juce::JSON::parse(folder.getChildFile("Original rig.json").loadFileAsString()); require(validate(recoveredRig).isEmpty(), "Recovered original snapshot must remain a valid rig");
    const auto recoveredState = readTreeFromXML(recoveredRig["state"].toString()); require(juce::File(recoveredState["modelPath"].toString()).existsAsFile() && juce::File(recoveredState["modelPath"].toString()).isAChildOf(recovered.location), "Take snapshots must use recovered sound assets");
    const auto sceneBank = juce::JSON::parse(recoveredState.getChildWithName("SCENES")["json"].toString());
    require(sceneBank["slots"][0]["name"].toString() == "Clean variation" && sceneBank["slots"][1].getDynamicObject() == nullptr, "Populated and empty scene slots must survive backup/recovery");
    require(juce::SHA256(source.getChildFile("library.xml")).toHexString() == sourceHash && juce::SHA256(originals.getChildFile("Guitar dry.wav")).toHexString() == audioHash, "Backup and recovery must leave all originals intact");
    const auto committedHash = juce::SHA256(target.getChildFile("library.xml")).toHexString();
    const auto bad = base.getChildFile("Bad.zip");
    mutateArchive(destination, bad, "library.xml", "../escape.xml"); rejected([&] {LibraryBackup::restore(bad, target, cancelled, validate);});
    bad.deleteFile(); mutateArchive(destination, bad, "library.xml", "library.xml", true); rejected([&] {LibraryBackup::restore(bad, target, cancelled, validate);});
    bad.deleteFile(); mutateArchive(destination, bad, "library.xml", "LIBRARY.XML"); rejected([&] {LibraryBackup::restore(bad, target, cancelled, validate);});
    require(juce::SHA256(target.getChildFile("library.xml")).toHexString() == committedHash && !base.getChildFile("escape.xml").exists(), "Rejected archives must not modify the destination or escape it");
    cancelled.store(true); const auto priorBackup = juce::SHA256(destination).toHexString(); rejected([&] {LibraryBackup::create(source, destination, rig, cancelled);}); rejected([&] {LibraryBackup::restore(destination, target, cancelled, validate);});
    require(juce::SHA256(destination).toHexString() == priorBackup, "Cancellation must preserve the previous backup"); cancelled.store(false);
    rejected([&] {LibraryBackup::create(source, destination, rig, cancelled, [&](double p) {if (p > .15) cancelled.store(true);});}); cancelled.store(false);
    require(juce::SHA256(destination).toHexString() == priorBackup, "Mid-copy cancellation must preserve the previous backup");
    source.getChildFile("assets").deleteRecursively(); rejected([&] {LibraryBackup::create(source, destination, rig, cancelled);});
    require(juce::SHA256(destination).toHexString() == priorBackup, "Missing sound files must not replace a valid backup");
    // Exercise the public worker API and prove restore does not recall a tone.
    const auto liveRoot = base.getChildFile("Live"), liveArchive = base.getChildFile("Live.zip");
    auto live = std::make_unique<AmpSuiteAudioProcessor>(true, liveRoot);
    require(live->saveRig("My live clean").isEmpty(), "Live backup fixture must save");
    auto wait = [&] { for (int i = 0; i < 2000; ++i) {if (!static_cast<bool>(live->backupStatus()["busy"])) return; juce::Thread::sleep(5);} require(false,"Backup worker timed out"); };
    require(live->requestBackup(false,liveArchive).isEmpty(), "Backup must queue on its worker"); wait();
    require(live->backupStatus()["error"].toString().isEmpty() && liveArchive.existsAsFile(), "Worker must report a completed verified backup");
    auto* drive = live->apvts.getParameter("DRIVE_GAIN"); drive->setValueNotifyingHost(drive->convertTo0to1(7)); const auto beforeRestore = live->getRig()["state"].toString();
    require(live->requestBackup(true,liveArchive).isEmpty(), "Restore must queue on its worker"); wait();
    require(live->backupStatus()["error"].toString().isEmpty() && live->getLibrary()["rigs"].size() == 3, "Worker restore must refresh its library and retain existing rigs");
    require(live->apvts.getRawParameterValue("DRIVE_GAIN")->load() == 7 && live->getRig()["state"].toString() != juce::String(), "Restore must preserve the live parameter edit");
    require(live->getRig()["state"].toString().contains("My live clean") && beforeRestore.contains("My live clean"), "Recovery must preserve current rig identity");
    std::cout << "Personal backup/recovery checks passed\n";
}
