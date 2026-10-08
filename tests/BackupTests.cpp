#include "../Source/PluginProcessor.h"
#include <iostream>
#include <thread>

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
void zip64Checks(const juce::File& base) {
    const auto payload=base.getChildFile("zip-payload.txt"), archive=base.getChildFile("ZIP64 profile.zip"), broken=base.getChildFile("Broken ZIP64.zip");
    require(payload.replaceWithText("bounded archive payload"),"ZIP64 source must write"); std::atomic<bool> cancelled{false};
    const std::vector<BackupZip::Source> files {{payload,"one.txt",payload.getSize()},{payload,juce::String::fromUTF8("clean-\xc3\xa9.txt"),payload.getSize()}};
    require(!BackupZip::needsZip64(files),"Small archives must retain classic ZIP automatically");
    require(BackupZip::needsZip64({{payload,"large.wav",4LL*1024*1024*1024+7}}),"Large source lengths must trigger ZIP64 without narrowing to 32 bits");
    rejected([&]{BackupZip::write(archive,{{payload,"oversized.wav",BackupZip::archiveLimit+1}},cancelled);});
    BackupZip::write(archive,files,cancelled); require(!BackupZip::Reader(archive).isZip64(),"Default small writer must remain compatible with classic ZIP");
    archive.deleteFile(); BackupZip::write(archive,files,cancelled,{},true); BackupZip::Reader reader(archive);
    require(reader.isZip64() && reader.entries().size()==2 && reader.entries()[1].name==files[1].name,"Forced small ZIP64 must preserve UTF-8 names and entry counts");
    for(int i=0;i<2;++i) require(reader.open(i)->readEntireStreamAsString()==payload.loadFileAsString(),"Bounded ZIP64 data streams must read exactly their entry contents");
    auto stream=archive.createInputStream(); stream->setPosition(archive.getSize()-50); const auto central=stream->readInt64();
    // Classic end (22) + locator (20) + last ZIP64 end field (8).
    auto corrupt=[&](juce::int64 position, juce::int64 value, bool wide=false) {
        require(archive.copyFileTo(broken),"Corrupt ZIP64 fixture must copy"); auto output=broken.createOutputStream(); require(output && output->setPosition(position),"Corrupt ZIP64 fixture must seek");
        if(wide) output->writeInt64(value); else output->writeByte(static_cast<char>(value)); output->flush(); output.reset(); rejected([&]{BackupZip::Reader invalid(broken);});
    };
    corrupt(0,0); // Wrong local signature.
    corrupt(central+8,9); // Encryption flag.
    corrupt(central+10,8); // Compression method.
    corrupt(central+38,1); // Unsupported attributes (including symlinks).
    corrupt(central+46+files[0].name.getNumBytesAsUTF8()+20,1,true); // Displaced/overlapping local header.
    corrupt(archive.getSize()-34,4LL*1024*1024*1024+7,true); // Out-of-file 64-bit locator.
    corrupt(central+46+files[0].name.getNumBytesAsUTF8()+4,BackupZip::archiveLimit+1,true); // Oversized expanded payload.
    corrupt(50+files[0].name.getNumBytesAsUTF8()+payload.getSize()+4,(reader.entries()[0].crc&255)^1); // Descriptor CRC mismatch.
    stream.reset();
    // Optional retained fixture for independent Python/.NET interoperability.
    const auto outputPath=juce::SystemStats::getEnvironmentVariable("CASSIAN_TEST_ZIP64_OUTPUT",{});
    if(outputPath.isNotEmpty()) {require(juce::File::isAbsolutePath(outputPath),"ZIP64 fixture output must be absolute"); require(archive.copyFileTo(juce::File(outputPath)),"ZIP64 interoperability fixture must copy");}
}
}
void runBackupChecks(const juce::File& model)
{
    const auto parent = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto base = parent.getChildFile("Cassian-backup-tests-" + juce::Uuid().toString()); require(base.createDirectory().wasOk(), "Backup fixture root must create");
    struct Cleanup {juce::File folder, parent; ~Cleanup() {if (folder.isAChildOf(parent)) folder.deleteRecursively();}} cleanup {base, parent};
    zip64Checks(base);
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
    const auto extended=base.getChildFile("Personal ZIP64.zip");
    const auto extendedReport=LibraryBackup::create(source,extended,rig,cancelled,{},true,std::nullopt,true);
    require(extendedReport.takes==1 && BackupZip::Reader(extended).isZip64(),"Library backup must support explicit small ZIP64 fixtures");
    const auto extendedTarget=base.getChildFile("ZIP64 target");
    require(LibraryBackup::restore(extended,extendedTarget,cancelled,validate).takes==1,"ZIP64 recovery must preserve catalog and snapshot compatibility");
    const auto extendedTake=read(extendedTarget.getChildFile("takes.xml")).getChild(0);
    require(juce::SHA256(juce::File(extendedTake["path"].toString()).getChildFile("Guitar dry.wav")).toHexString()==audioHash,"ZIP64 recovered audio must be byte-identical");
    const auto beforeExtended=juce::SHA256(extendedTarget.getChildFile("library.xml")).toHexString();
    const auto corruptExtended=base.getChildFile("Corrupt ZIP64 payload.zip"); require(extended.copyFileTo(corruptExtended),"ZIP64 corruption fixture must copy");
    {auto input=extended.createInputStream(); input->setPosition(26); const int nameBytes=input->readShort(); auto output=corruptExtended.createOutputStream(); output->setPosition(50+nameBytes); output->writeByte('!'); output->flush();}
    rejected([&]{LibraryBackup::restore(corruptExtended,extendedTarget,cancelled,validate);});
    require(juce::SHA256(extendedTarget.getChildFile("library.xml")).toHexString()==beforeExtended,"Rejected ZIP64 payloads must leave existing catalogs intact");
    if(juce::SystemStats::getEnvironmentVariable("CASSIAN_TEST_LARGE_BACKUP",{})=="1") {
        // A valid short RIFF with synthetic trailing padding exercises archive
        // transport above 4 GiB, not four-gigabyte audio recording/RIFF support.
        const auto backing=originals.getChildFile("Backing track.wav"); juce::MemoryBlock originalBacking;
        require(backing.loadFileAsData(originalBacking),"Large fixture must preserve its original short WAV");
        constexpr juce::int64 paddedBytes=4LL*1024*1024*1024+1024;
        require(base.getBytesFreeOnVolume()>paddedBytes*4,"Large archive validation needs at least 17 GiB free scratch space");
        {auto output=backing.createOutputStream(); require(output && output->setPosition(paddedBytes-1) && output->writeByte(0),"Large synthetic WAV padding must write"); output->flush(); require(output->getStatus().wasOk(),"Large padding write must succeed");}
        const auto largeArchive=base.getChildFile("Large personal archive.zip");
        const auto largeReport=LibraryBackup::create(source,largeArchive,rig,cancelled);
        require(largeReport.bytes>4LL*1024*1024*1024 && largeArchive.getSize()>4LL*1024*1024*1024 && BackupZip::Reader(largeArchive).isZip64(),"Multi-gigabyte personal archive must automatically use ZIP64 without truncation");
        const auto largeTarget=base.getChildFile("Large target");
        require(LibraryBackup::restore(largeArchive,largeTarget,cancelled,validate).takes==1,"Large archive must restore as a complete take");
        const auto largeFolder=juce::File(read(largeTarget.getChildFile("takes.xml")).getChild(0)["path"].toString());
        auto bufferedHash=[](const juce::File& file){auto input=file.createInputStream(); juce::BufferedInputStream buffered(*input,65536); return juce::SHA256(buffered).toHexString();};
        require(largeFolder.getChildFile("Backing track.wav").getSize()==paddedBytes && bufferedHash(backing)==bufferedHash(largeFolder.getChildFile("Backing track.wav")),"Above-4-GiB recovered payload must match its source byte-for-byte");
        std::jthread cancelHash([&]{juce::Thread::sleep(50); cancelled.store(true);});
        rejected([&]{LibraryBackup::create(source,largeArchive,rig,cancelled);}); cancelHash.join(); cancelled.store(false);
        require(BackupZip::Reader(largeArchive).isZip64(),"Cancellation during checksum preparation must preserve the large valid archive");
        const auto outputPath=juce::SystemStats::getEnvironmentVariable("CASSIAN_TEST_LARGE_BACKUP_OUTPUT",{});
        if(outputPath.isNotEmpty()) {const juce::File output(outputPath); require(juce::File::isAbsolutePath(outputPath) && !output.exists() && largeArchive.copyFileTo(output),"Optional large interoperability output must create without replacing existing work");}
        require(backing.replaceWithData(originalBacking.getData(),originalBacking.getSize()),"Large fixture must restore its original short WAV");
        std::cout<<"ZIP64 above-4-GiB backup/recovery and checksum cancellation passed\n";
    }
    rejected([&] {LibraryBackup::create(source, originals.getChildFile("Guitar dry.wav"), rig, cancelled);});
    require(juce::SHA256(originals.getChildFile("Guitar dry.wav")).toHexString() == audioHash, "Backup destination must never replace a referenced external recording");
    const auto toneArchive = base.getChildFile("Tone library.zip");
    const auto toneReport = LibraryBackup::create(source,toneArchive,rig,cancelled,{},false);
    require(toneReport.takes == 0 && toneReport.rigs == 2,"Tone-library backup must retain saved/current rigs and exclude recorded takes");
    juce::ZipFile toneZip(toneArchive);
    for(int i=0;i<toneZip.getNumEntries();++i) require(!toneZip.getEntry(i)->filename.startsWith("takes/") && !toneZip.getEntry(i)->filename.startsWith("take-sections/"),"Tone-library archive must not include recording or take review payloads");
    const auto toneRecovered = LibraryBackup::restore(toneArchive,base.getChildFile("Tone target"),cancelled,validate);
    require(toneRecovered.rigs==2 && toneRecovered.takes==0,"Tone-library archive must restore as valid rig copies without recordings");
    const auto takeCatalogFile=source.getChildFile("takes.xml"); const auto takeCatalogText=takeCatalogFile.loadFileAsString();
    require(takeCatalogFile.replaceWithText("unreadable recording catalog"),"Damaged take fixture must write");
    require(LibraryBackup::create(source,toneArchive,rig,cancelled,{},false).takes==0,"Excluded take metadata must not prevent protecting the tone library");
    rejected([&]{LibraryBackup::create(source,base.getChildFile("Incomplete complete backup.zip"),rig,cancelled);});
    require(takeCatalogFile.replaceWithText(takeCatalogText),"Take fixture must restore before remaining complete-backup checks");
    // An excluded take can be missing entirely; only explicitly selected
    // identities and their media are archived. Catalog changes still abort.
    auto subsetCatalog = takes.createCopy(); juce::ValueTree omitted("TAKE");
    omitted.setProperty("id", "missing-take", nullptr); omitted.setProperty("name", "Excluded recording", nullptr);
    omitted.setProperty("path", base.getChildFile("Missing recording").getFullPathName(), nullptr); subsetCatalog.addChild(omitted, -1, nullptr);
    require(takeCatalogFile.replaceWithText(subsetCatalog.toXmlString()), "Selective fixture catalog must write");
    const auto unrelatedSection = juce::String::repeatedString("a",64)+".json";
    source.getChildFile("take-sections").getChildFile(unrelatedSection).replaceWithText("unrelated section metadata");
    const auto subsetArchive=base.getChildFile("Selected takes.zip"); const juce::StringArray selectedIds {"old-take"};
    const auto subsetReport=LibraryBackup::create(source,subsetArchive,rig,cancelled,{},true,selectedIds);
    require(subsetReport.takes==1 && subsetReport.rigs==2,"Selective backup must retain all tones and only chosen takes despite missing excluded audio");
    juce::ZipFile subsetZip(subsetArchive);
    require(subsetZip.getIndexOfFileName("take-sections/"+section)>=0 && subsetZip.getIndexOfFileName("take-sections/"+unrelatedSection)<0,"Selective backup must include matching review sections and exclude unrelated metadata");
    std::unique_ptr<juce::InputStream> subsetManifest(subsetZip.createStreamForEntry(subsetZip.getIndexOfFileName("backup.json")));
    const auto manifest=juce::JSON::parse(subsetManifest->readEntireStreamAsString());
    require(manifest["scope"].toString()=="selected-takes" && static_cast<int>(manifest["takeCount"])==1,"Selective archive must identify its scope and included take count");
    std::unique_ptr<juce::InputStream> subsetTakes(subsetZip.createStreamForEntry(subsetZip.getIndexOfFileName("takes.xml")));
    require(!subsetTakes->readEntireStreamAsString().contains("Excluded recording"),"Unselected take metadata must never enter the archive");
    const auto subsetTarget=base.getChildFile("Subset target");
    require(LibraryBackup::restore(subsetArchive,subsetTarget,cancelled,validate).takes==1,"Selected-takes archive must restore through the compatible archive reader");
    const auto subsetTake=read(subsetTarget.getChildFile("takes.xml")).getChild(0);
    require(subsetTake["notes"].toString()=="Best solo" && subsetTake.getNumChildren()==1 && juce::SHA256(juce::File(subsetTake["path"].toString()).getChildFile("Guitar dry.wav")).toHexString()==audioHash,"Selective recovery must preserve notes, reamps and exact audio");
    const auto subsetHash=juce::SHA256(subsetArchive).toHexString();
    rejected([&]{LibraryBackup::create(source,subsetArchive,rig,cancelled,{},true,juce::StringArray{});});
    rejected([&]{LibraryBackup::create(source,subsetArchive,rig,cancelled,{},true,juce::StringArray{"deleted-take"});});
    rejected([&]{LibraryBackup::create(source,subsetArchive,rig,cancelled,{},true,juce::StringArray{"old-take","old-take"});});
    rejected([&]{LibraryBackup::create(source,subsetArchive,rig,cancelled,{},false,selectedIds);});
    rejected([&]{LibraryBackup::create(source,subsetArchive,rig,cancelled,{},true,juce::StringArray{"missing-take"});});
    auto ambiguous=subsetCatalog.createCopy(); ambiguous.addChild(take.createCopy(),-1,nullptr);
    require(takeCatalogFile.replaceWithText(ambiguous.toXmlString()),"Ambiguous take fixture must write");
    rejected([&]{LibraryBackup::create(source,subsetArchive,rig,cancelled,{},true,selectedIds);});
    require(juce::SHA256(subsetArchive).toHexString()==subsetHash,"Invalid selections and missing selected audio must preserve the previous valid archive");
    require(takeCatalogFile.replaceWithText(takeCatalogText),"Take catalog must restore after subset checks");
    rejected([&]{LibraryBackup::create(source,subsetArchive,rig,cancelled,[&](double p){if(p>.15) takeCatalogFile.replaceWithText("changed while archiving");},true,selectedIds);});
    require(juce::SHA256(subsetArchive).toHexString()==subsetHash && takeCatalogFile.replaceWithText(takeCatalogText),"Catalog changes during selective backup must preserve output and permit retry");
    source.getChildFile("take-sections").getChildFile(unrelatedSection).deleteFile();
    juce::ZipFile archive(destination); require(archive.getIndexOfFileName("backup.json") >= 0, "Backup must include its integrity manifest");
    // Archive is portable: no original machine paths in the library metadata.
    std::unique_ptr<juce::InputStream> portable(archive.createStreamForEntry(archive.getIndexOfFileName("library.xml")));
    const auto portableText = portable->readEntireStreamAsString(); require(!portableText.contains(source.getFullPathName()) && !portableText.contains(model.getFullPathName()), "Portable catalog must rewrite paths, not keep machine references");
    juce::ValueTree current("LIBRARY"), kept("RIG"); kept.setProperty("id", "original-rig", nullptr); kept.setProperty("name", "Existing rig", nullptr); current.addChild(kept, -1, nullptr);
    current.setProperty("removedIds", asset["id"].toString()+",old-deleted-rig",nullptr);
    target.getChildFile("library.xml").replaceWithText(current.toXmlString()); target.getChildFile("takes.xml").replaceWithText("<TAKES/>");
    target.getChildFile("take-sections").createDirectory(); target.getChildFile("take-sections").getChildFile(section).replaceWithText("existing sections");
    const auto recovered = LibraryBackup::restore(destination, target, cancelled, validate);
    const auto restored = read(target.getChildFile("library.xml")), restoredTakes = read(target.getChildFile("takes.xml"));
    require(recovered.rigs == 2 && recovered.takes == 1 && recovered.location.isDirectory(), "Restore must add saved and current-tone copies");
    require(restored.getChildWithProperty("id", "original-rig")["name"].toString() == "Existing rig", "Restore must preserve conflicting saved rig identity");
    require(restored["removedIds"].toString()=="old-deleted-rig","Recovery must clear only restored sound deletion markers");
    LibraryStore recoveredStore(target); const auto recoveredCatalog=recoveredStore.load(); recoveredStore.save(recoveredCatalog);
    require(recoveredStore.load().getChildWithProperty("id",asset["id"]).hasType("ASSET"),"A subsequent normal library save must preserve restored sound identities");
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
    require(live->requestBackup(false,liveArchive,true,juce::StringArray{}).isNotEmpty() && !static_cast<bool>(live->backupStatus()["busy"]),"Empty public subset selection must reject before starting worker work");
    const auto liveTake=base.getChildFile("Live take"); liveTake.createDirectory();
    wave(liveTake.getChildFile("Guitar dry.wav"),1); wave(liveTake.getChildFile("Guitar processed.wav"),2);
    liveTake.getChildFile("Original rig.json").replaceWithText(juce::JSON::toString(live->getRig())); live->takes.importFolder(liveTake);
    for(int i=0;i<2000 && live->takes.list().size()==0;++i) juce::Thread::sleep(5);
    require(live->takes.list().size()==1,"Live selective fixture must import on the take worker");
    auto wait = [&] { for (int i = 0; i < 2000; ++i) {if (!static_cast<bool>(live->backupStatus()["busy"])) return; juce::Thread::sleep(5);} require(false,"Backup worker timed out"); };
    require(live->requestBackup(false,liveArchive).isEmpty(), "Backup must queue on its worker"); wait();
    require(live->backupStatus()["error"].toString().isEmpty() && liveArchive.existsAsFile(), "Worker must report a completed verified backup");
    const auto liveSubset=base.getChildFile("Live selected.zip");
    require(live->requestBackup(false,liveSubset,true,juce::StringArray{live->takes.list()[0]["id"].toString()}).isEmpty(),"Public selective request must queue stable identities"); wait();
    require(live->backupStatus()["error"].toString().isEmpty() && live->backupStatus()["summary"].toString().contains("selected-takes") && liveSubset.existsAsFile(),"Worker must report the verified selective scope");
    auto* drive = live->apvts.getParameter("DRIVE_GAIN"); drive->setValueNotifyingHost(drive->convertTo0to1(7)); const auto beforeRestore = live->getRig()["state"].toString();
    require(live->requestBackup(true,liveArchive).isEmpty(), "Restore must queue on its worker"); wait();
    require(live->backupStatus()["error"].toString().isEmpty() && live->getLibrary()["rigs"].size() == 3, "Worker restore must refresh its library and retain existing rigs");
    require(live->apvts.getRawParameterValue("DRIVE_GAIN")->load() == 7 && live->getRig()["state"].toString() != juce::String(), "Restore must preserve the live parameter edit");
    require(live->getRig()["state"].toString().contains("My live clean") && beforeRestore.contains("My live clean"), "Recovery must preserve current rig identity");
    std::cout << "Personal backup/recovery checks passed\n";
}
