#include "../Source/PluginProcessor.h"
#include <iostream>
#include <limits>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
template <typename Predicate> void waitFor(Predicate ready) {
    for (int n = 0; n < 2000; ++n) { if (ready()) return; juce::Thread::sleep(5); }
    require(false, "Take library worker timed out");
}
void set(AmpSuiteAudioProcessor& p, const char* id, float value) { auto* param = p.apvts.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(value)); }
void write(const juce::File& file, int channels, int frames, double rate, float value, bool step = false) {
    juce::WavAudioFormat format; auto stream = file.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), rate, static_cast<unsigned>(channels), 32, {}, 0));
    require(writer != nullptr, "Take fixture writer must open");
    juce::AudioBuffer<float> audio(channels, 1024);
    for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < 1024; ++i) audio.setSample(ch, i, value);
    for (int offset = 0; offset < frames; offset += 1024) {
        if (step) for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < 1024; ++i) audio.setSample(ch,i,offset+i < frames/2 ? value * .4f : value);
        require(writer->writeFromAudioSampleBuffer(audio, 0, juce::jmin(1024, frames - offset)), "Take fixture must write");
    }
}
void makeTake(const juce::File& folder, int frames = 4096, double rate = 48000) {
    require(folder.createDirectory().wasOk(), "Take fixture directory must create");
    write(folder.getChildFile("Guitar dry.wav"), 1, frames, rate, .125f); write(folder.getChildFile("Guitar processed.wav"), 2, frames, rate, .8f);
}
juce::var first(TakeLibrary& library) { auto list = library.list(); require(list.isArray() && list.size() > 0, "Take library must contain an entry"); return list[0]; }
void imported(TakeLibrary& library, const juce::File& folder, int count = 1) {
    library.importFolder(folder); waitFor([&] { return library.list().size() == count; });
}
juce::var rig() {
    AmpSuiteAudioProcessor p(false); set(p, "AMP_SOURCE", 4); set(p, "CAB_MODE", 3); set(p, "INPUT_GAIN", 6);
    set(p, "GATE_ON", 0); set(p, "REVERB_MIX", 0); set(p, "DELAY_MIX", 0); set(p, "MASTER_VOL", -48);
    set(p, "GUITAR_MIX_LEVEL", 12); set(p, "GUITAR_MIX_FOCUS", 100); return p.getRig();
}
juce::var recover(TakeLibrary& library, const juce::String& id, const juce::String& version) {
    juce::var result; std::atomic<bool> finished {false}; int calls = 0;
    const auto caller = juce::Thread::getCurrentThreadId(); bool worker = false;
    const auto error = library.readRigSnapshot(id, version, [&](juce::var value) {
        result = std::move(value); worker = caller != juce::Thread::getCurrentThreadId(); ++calls; finished.store(true);
    });
    require(error.isEmpty(), "Snapshot read must queue"); waitFor([&] { return finished.load(); });
    require(worker && calls == 1, "Snapshot I/O must complete exactly once on the worker"); return result;
}
}
void runTakeChecks()
{
    const auto base = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root = base.getNonexistentChildFile("Cassian-take-tests-" + juce::Uuid().toString(), "", false);
    require(root.createDirectory().wasOk(), "Take test directory must create");
    struct Cleanup { juce::File folder, base; ~Cleanup() { if (folder.isAChildOf(base)) folder.deleteRecursively(); } } cleanup {root, base};
    const auto folder = root.getChildFile("Original take"), catalog = root.getChildFile("takes.xml"); makeTake(folder);
    // Review sections persist independently of backing sections and audio. A
    // queued save captures the range at click time; stale generations cancel.
    const auto sectionFolder = root.getChildFile("Section take"), sectionCatalog = root.getChildFile("sections-catalog.xml"), sectionStore = root.getChildFile("take-sections");
    makeTake(sectionFolder, 48000); sectionFolder.getChildFile("Original rig.json").replaceWithText(juce::JSON::toString(rig()));
    const auto sectionAudioHash = juce::SHA256(sectionFolder.getChildFile("Guitar processed.wav")).toHexString();
    juce::String sectionTakeId, savedSectionId;
    {
        PracticeEngine review(262144, sectionStore); review.prepare(48000); TakeLibrary library(sectionCatalog, review); imported(library, sectionFolder);
        sectionTakeId = first(library)["id"].toString();
        require(library.reviewSection(sectionTakeId,"processed","save","Solo",{}).isNotEmpty(), "Unloaded section edits must reject");
        library.preview(sectionTakeId,"processed"); waitFor([&]{return library.status()["reviewId"].toString()==sectionTakeId;});
        juce::AudioBuffer<float> output(2,128), dry(1,128); output.clear(); dry.clear(); review.process(output,dry.getReadPointer(0));
        library.reviewControl(sectionTakeId,"processed","a",.1); library.reviewControl(sectionTakeId,"processed","b",.6);
        juce::WaitableEvent entered, release, finished;
        library.readRigSnapshot(sectionTakeId,"processed",[&](juce::var) { entered.signal(); release.wait(3000); finished.signal(); });
        require(entered.wait(3000),"Section fixture must pause the worker");
        require(library.reviewSection(sectionTakeId,"processed","save","Solo",{}).isEmpty(), "Loaded section save must queue");
        library.reviewControl(sectionTakeId,"processed","a",.2); library.reviewControl(sectionTakeId,"processed","b",.9);
        release.signal(); require(finished.wait(3000),"Section fixture callback must complete");
        waitFor([&]{return review.status()["sections"].size()==1;}); const auto row=review.status()["sections"][0]; savedSectionId=row["id"].toString();
        require(std::abs(static_cast<double>(row["a"])-.1)<1.e-6 && std::abs(static_cast<double>(row["b"])-.6)<1.e-6,"Section save must preserve its queued range");
        require(library.reviewSection(sectionTakeId,"dry","remove",{},savedSectionId).isNotEmpty() && library.reviewSection(sectionTakeId,"processed","unknown",{},savedSectionId).isNotEmpty(),"Wrong version and command must reject");
        const auto revision=static_cast<int>(review.status()["sectionRevision"]);
        require(library.reviewSection(sectionTakeId,"processed","recall",{},savedSectionId).isEmpty(),"Section recall must queue");
        waitFor([&]{return static_cast<int>(review.status()["sectionRevision"])>revision;});
        require(!review.isPlaying() && static_cast<bool>(review.status()["loop"]) && std::abs(static_cast<double>(review.status()["a"])-.1)<1.e-6,"Recall must pause and restore the saved loop");
        juce::WaitableEvent enteredAgain, releaseAgain;
        library.readRigSnapshot(sectionTakeId,"processed",[&](juce::var) { enteredAgain.signal(); releaseAgain.wait(3000); });
        require(enteredAgain.wait(3000),"Stale section fixture must pause worker");
        library.reviewSection(sectionTakeId,"processed","save","Cancelled section",{}); library.stopReview(); releaseAgain.signal();
        library.preview(sectionTakeId,"dry"); waitFor([&]{return library.status()["reviewVersion"].toString()=="dry";});
        require(review.status()["sections"].size()==0,"Dry DI must not inherit processed version sections");
    }
    {
        PracticeEngine review(262144,sectionStore); review.prepare(48000); TakeLibrary library(sectionCatalog,review); waitFor([&]{return library.list().size()==1;});
        library.preview(sectionTakeId,"processed"); waitFor([&]{return library.status()["reviewVersion"].toString()=="processed";});
        juce::AudioBuffer<float> output(2,128), dry(1,128); output.clear(); dry.clear(); review.process(output,dry.getReadPointer(0));
        require(review.status()["sections"].size()==1 && review.status()["sections"][0]["id"].toString()==savedSectionId,"Sections must survive reopening and stale saves must not persist");
        require(library.reviewSection(sectionTakeId,"processed","remove",{},savedSectionId).isEmpty(),"Saved section deletion must queue");
        waitFor([&]{return review.status()["sections"].size()==0;});
        require(juce::SHA256(sectionFolder.getChildFile("Guitar processed.wav")).toHexString()==sectionAudioHash,"Section edits must preserve original audio");
    }
    {
        // A valid nearly-full catalog must not become unreadable after saving
        // an otherwise valid annotation. Count serialized UTF-8, including XML.
        const auto limited = root.getChildFile("limited-catalog.xml"); juce::ValueTree tree("TAKES"), take("TAKE");
        take.setProperty("id","capacity",nullptr); take.setProperty("name","Kept",nullptr); take.setProperty("notes","",nullptr); take.setProperty("padding","",nullptr); tree.addChild(take,-1,nullptr);
        const auto remaining = 8 * 1024 * 1024 - tree.toXmlString().getNumBytesAsUTF8() - 50;
        take.setProperty("padding",juce::String::repeatedString("x",remaining),nullptr);
        require(limited.replaceWithText(tree.toXmlString()) && limited.getSize() <= 8 * 1024 * 1024,"Nearly-full catalog must be readable");
        const auto before = juce::SHA256(limited).toHexString(); PracticeEngine review; TakeLibrary library(limited,review); waitFor([&]{return library.list().size()==1;});
        require(library.annotate("capacity",juce::String::repeatedString("&",100)).isEmpty(),"Valid-size note must queue before serialized capacity checking");
        waitFor([&]{return library.status()["error"].toString().contains("exceed 8 MiB");});
        require(first(library)["notes"].toString().isEmpty() && juce::SHA256(limited).toHexString()==before,"Rejected oversized catalog must preserve disk and in-memory metadata");
    }
    // Video exports work at common interface rates and keep backing separate
    // until the export stage. No live Master/Play Along controls enter this mix.
    for (double rate : {44100., 48000., 96000.}) {
        const auto videoFolder = root.getChildFile("Video take " + juce::String(rate)); makeTake(videoFolder, static_cast<int>(rate / 10), rate);
        require(videoFolder.getChildFile("Guitar processed.wav").deleteFile(), "Replace temporary video fixture");
        write(videoFolder.getChildFile("Guitar processed.wav"),2,static_cast<int>(rate / 10),rate,.25f,true);
        write(videoFolder.getChildFile("Backing track.wav"),2,static_cast<int>(rate / 10),rate,.125f);
        PracticeEngine review; TakeLibrary video({},review); video.importFolder(videoFolder);
        waitFor([&] {return video.list().size()==1;}); const auto id=video.list()[0]["id"].toString();
        const auto destination=root.getChildFile("Video " + juce::String(rate) + ".wav");
        require(video.videoExport(id,"processed",destination,true,0,-6).isEmpty(), "Video mix must queue");
        waitFor([&] {return !static_cast<bool>(video.status()["exporting"]);}); require(video.status()["error"].toString().isEmpty(), "Video mix must finish");
        juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(destination));
        require(reader && reader->sampleRate==48000 && reader->bitsPerSample==24 && reader->numChannels==2 && !reader->usesFloatingPointData && reader->lengthInSamples==4800,"Video WAV must be 48k stereo 24-bit PCM with preserved duration");
        juce::AudioBuffer<float> audio(2,4800);require(reader->read(&audio,0,4800,0,true,true),"Video WAV must decode");
        const auto expected=.25f+.125f*juce::Decibels::decibelsToGain(-6.f);
        require(std::abs(audio.getSample(0,3000)-expected)<.002,"Export must mix the requested processed guitar and backing balance");
        const auto report = video.status()["lastExportReport"];
        require(report["takeId"].toString()==id && report["version"].toString()=="processed" && static_cast<bool>(report["backing"]) && static_cast<juce::int64>(report["frames"])==4800 && std::abs(static_cast<double>(report["duration"])-.1)<1.e-6 && static_cast<double>(report["attenuationDb"])==0,"Successful export must report its actual duration, source and absence of attenuation");
        for (const auto& version : {juce::String("processed"),juce::String("dry")}) {
            const auto guitarOnly=root.getChildFile("Quick guitar " + version + " " + juce::String(rate) + ".wav");
            require(video.videoExport(id,version,guitarOnly,false,0,0,0,.1,.01).isEmpty(),"Quick guitar export must queue the full selected version");
            waitFor([&]{return !static_cast<bool>(video.status()["exporting"]);});
            reader.reset(formats.createReaderFor(guitarOnly));
            require(reader && reader->sampleRate==48000 && reader->bitsPerSample==24 && reader->numChannels==2 && reader->lengthInSamples==4800 && reader->read(&audio,0,4800,0,true,true),"Quick guitar WAV must have the video-friendly format and full duration");
            const auto guitarExpected=version=="dry"?.125f:.25f;
            require(std::abs(audio.getSample(0,3000)-guitarExpected)<.002 && std::abs(audio.getSample(1,3000)-guitarExpected)<.002,"Quick export must exclude backing and retain the requested dry/processed guitar");
            require(std::abs(audio.getSample(0,0))<1.e-6 && std::abs(audio.getSample(0,4799))<1.e-6,"Quick export must fade its edges without trimming duration");
        }
        const auto loud=root.getChildFile("Video loud " + juce::String(rate) + ".wav");
        require(video.videoExport(id,"processed",loud,true,12,12).isEmpty(),"Hot export must queue");waitFor([&]{return !static_cast<bool>(video.status()["exporting"]);});
        reader.reset(formats.createReaderFor(loud));require(reader && reader->read(&audio,0,4800,0,true,true),"Protected export must decode");
        require(audio.getMagnitude(0,4800)<.892f && audio.getMagnitude(0,4800)>.88f,"Hot mixed soundtrack must retain -1 dBFS peak headroom");
        const auto measuredReduction = juce::Decibels::gainToDecibels((.25f+.125f)*juce::Decibels::decibelsToGain(12.f) / audio.getSample(0,3000));
        require(std::abs(static_cast<double>(video.status()["lastExportReport"]["attenuationDb"])-measuredReduction)<.02,"Export report must describe the attenuation actually written");
        const auto hash=juce::SHA256(destination).toHexString();require(video.videoExport(id,"processed",destination,false,0,0).isNotEmpty() && juce::SHA256(destination).toHexString()==hash,"Video export must never overwrite an existing file");
        require(video.videoExport(id,"missing",root.getChildFile("no.wav"),false,0,0).isNotEmpty(),"Unknown video take version must reject");
        const auto trimmed=root.getChildFile("Trimmed " + juce::String(rate) + ".wav");
        require(video.videoExport(id,"processed",trimmed,true,0,-6,.02,.08,.01).isEmpty(),"Trimmed mix must queue");
        waitFor([&]{return !static_cast<bool>(video.status()["exporting"]);});
        reader.reset(formats.createReaderFor(trimmed));
        require(reader && reader->lengthInSamples==2880,"Trimmed mix duration must be exact at every source rate");
        require(reader->read(&audio,0,2880,0,true,true),"Trimmed mix must decode");
        require(std::abs(audio.getSample(0,0))<1.e-6 && std::abs(audio.getSample(0,2879))<1.e-6,"Fades must silence both boundary samples");
        require(std::abs(audio.getSample(0,2000)-expected)<.002,"Trim must seek both passes and preserve interior balance");
        require(std::abs(audio.getSample(0,1000)-(.1f+.125f*juce::Decibels::decibelsToGain(-6.f)))<.002,"Trimmed content must retain its original timeline");
        const auto trimmedReport = juce::JSON::toString(video.status()["lastExportReport"]);
        require(std::abs(static_cast<double>(video.status()["lastExportReport"]["duration"])-.06)<1.e-6 && std::abs(static_cast<double>(video.status()["lastExportReport"]["start"])-.02)<1.e-6,"Trim report must retain source timeline and output duration");
        for (const auto range : {std::pair<double,double>{-.1,.08}, {.08,.02}, {0,.2}})
            require(video.videoExport(id,"processed",root.getChildFile("invalid.wav"),false,0,0,range.first,range.second,.01).isNotEmpty(),"Invalid export bounds must reject before queuing");
        require(video.videoExport(id,"processed",root.getChildFile("invalid.wav"),false,0,0,0,-1,.101).isNotEmpty(),"Oversized fades must reject");
        const auto cancelledFile=root.getChildFile("Cancelled " + juce::String(rate) + ".wav");
        video.videoExport(id,"processed",cancelledFile,true,0,0); video.cancelExport();
        waitFor([&]{return !static_cast<bool>(video.status()["exporting"]);});
        require(!cancelledFile.exists(),"Cancelled video mix must discard partial output");
        require(juce::JSON::toString(video.status()["lastExportReport"])==trimmedReport && video.status()["lastExportPath"].toString()==trimmed.getFullPathName(),"Cancelled exports must retain the last successful report and path");
    }
    // Original snapshots from before board metadata are migrated in the isolated
    // renderer. The original take and its reference document remain untouched.
    const auto oldSnapshot = rig(); auto oldXml = juce::XmlDocument::parse(oldSnapshot["state"].toString());
    require(oldXml != nullptr, "Legacy take snapshot must contain XML"); auto oldState = juce::ValueTree::fromXml(*oldXml);
    oldState.removeChild(oldState.getChildWithName("PEDALBOARD"), nullptr);
    oldSnapshot.getDynamicObject()->setProperty("schema", 1); oldSnapshot.getDynamicObject()->setProperty("state", oldState.toXmlString());
    const auto originalRigFile = folder.getChildFile("Original rig.json");
    require(originalRigFile.replaceWithText(juce::JSON::toString(oldSnapshot)), "Legacy original snapshot must write");
    const auto originalRigHash = juce::SHA256(originalRigFile).toHexString();
    const auto dryHash = juce::SHA256(folder.getChildFile("Guitar dry.wav")).toHexString(), wetHash = juce::SHA256(folder.getChildFile("Guitar processed.wav")).toHexString();
    // Persist names/favorites, deduplicate an imported folder, merge writes from
    // independent stores and reopen the resulting catalog.
    juce::String originalId;
    {
        PracticeEngine review; review.prepare(48000); TakeLibrary library(catalog, review); imported(library, folder);
        originalId = first(library)["id"].toString();
        const auto revision = static_cast<int>(library.status()["revision"]); library.importFolder(folder);
        waitFor([&] { return static_cast<int>(library.status()["revision"]) > revision; }); require(library.list().size() == 1, "Repeated folder imports must deduplicate");
        require(library.edit(originalId, "Neoclassical lead take", true).isEmpty(), "Take edit must queue");
        waitFor([&] { return catalog.loadFileAsString().contains("Neoclassical lead take"); });
        PracticeEngine otherReview; TakeLibrary other(catalog, otherReview);
        waitFor([&] { return other.list().size() == 1; });
        const auto secondFolder = root.getChildFile("Second take"); makeTake(secondFolder); imported(other, secondFolder, 2);
        waitFor([&] { return catalog.loadFileAsString().contains("Second take"); });
        library.edit(originalId, "Favorite lead", true);
        waitFor([&] { return catalog.loadFileAsString().contains("Favorite lead"); });
        require(library.annotate(originalId, "  Drop D, 140 bpm\nCheck the alternate picking.  ").isEmpty(), "Take notes must queue");
        waitFor([&] { return catalog.loadFileAsString().contains("alternate picking"); });
        require(library.annotate(originalId, juce::String::repeatedString("x", 2001)).isNotEmpty() && library.annotate("missing", "note").isNotEmpty(), "Oversized notes and unknown takes must reject");
        library.edit(originalId, "Favorite lead", true);
        waitFor([&] { return !library.status()["error"].toString().isNotEmpty() && first(library)["notes"].toString().contains("alternate picking"); });
        require(catalog.loadFileAsString().contains("Second take"), "Catalog edits must preserve another instance's take");
    }
    {
        PracticeEngine review; review.prepare(48000); TakeLibrary library(catalog, review);
        waitFor([&] { return library.list().size() == 2; });
        auto entries = library.list(); bool found = false;
        for (const auto& take : *entries.getArray()) if (take["id"].toString() == originalId) found = take["name"].toString() == "Favorite lead" && static_cast<bool>(take["favorite"]);
        require(found, "Take names and favorites must survive restart");
        bool notesFound = false; for (const auto& take : *entries.getArray()) if (take["id"].toString() == originalId) notesFound = take["notes"].toString() == "Drop D, 140 bpm\nCheck the alternate picking.";
        require(notesFound, "Multiline notes must survive restart and name edits");
        // Review processed audio bypasses all guitar effects, while stop cancels
        // queued previews instead of letting a stale request start playback.
        require(library.preview(originalId, "processed").isEmpty(), "Processed take preview must queue");
        waitFor([&] { return review.transportActive(); });
        juce::AudioBuffer<float> audio(2, 128), dry(1, 128); audio.clear(); dry.clear();
        review.process(audio, dry.getReadPointer(0)); require(audio.getMagnitude(0, 0, 128) > .19f, "Processed review must preserve stored audio at review gain");
        require(library.status()["reviewId"].toString() == originalId && library.status()["reviewVersion"].toString() == "processed" && !static_cast<bool>(library.status()["reviewLoading"]), "Review identity must publish only after preparation");
        const auto waveform = library.reviewWaveform(originalId,"processed");
        require(waveform["takeId"].toString()==originalId && waveform["version"].toString()=="processed" && waveform["peaks"].isArray() && waveform["peaks"].size()==512 && static_cast<int>(waveform["revision"])==static_cast<int>(review.status()["waveRevision"]),"Loaded review waveform must publish a bounded envelope with its exact identity/revision");
        require(static_cast<double>(waveform["peaks"][200][1])>.79 && std::abs(static_cast<double>(waveform["duration"])-4096./48000)<1.e-6,"Review waveform must describe stored audio before monitoring gain");
        require(library.reviewWaveform("other","processed").hasProperty("error") && library.reviewWaveform(originalId,"dry").hasProperty("error"),"Other take/version requests must not expose the loaded waveform");
        require(library.reviewControl("other", "processed", "seek", .02).isNotEmpty() && library.reviewControl(originalId, "dry", "pause", 0).isNotEmpty(), "Controls for another take or version must not change playback");
        require(review.transportActive(), "Rejected stale pause must leave the loaded version playing");
        require(library.reviewControl(originalId,"processed","pause",0).isEmpty(), "Loaded review must pause");
        const auto paused = static_cast<double>(review.status()["position"]);
        audio.clear(); review.process(audio, dry.getReadPointer(0));
        require(!review.transportActive() && static_cast<double>(review.status()["position"]) == paused && audio.getMagnitude(0,128) == 0, "Pause must retain position and produce no review audio");
        require(library.reviewControl(originalId,"processed","a",.02).isEmpty() && library.reviewControl(originalId,"processed","b",.08).isEmpty(), "Review loop bounds must set on the loaded version");
        require(library.reviewControl(originalId,"processed","loop",1).isEmpty() && library.reviewControl(originalId,"processed","seek",.02).isEmpty(), "Valid review loop must enable and seek");
        require(library.reviewControl(originalId,"processed","play",0).isEmpty(), "Paused review must resume");
        float loopPeak = 0;
        for (int block = 0; block < 80; ++block) { audio.clear(); review.process(audio,dry.getReadPointer(0)); loopPeak = juce::jmax(loopPeak,audio.getMagnitude(0,128)); }
        const auto loopPosition = static_cast<double>(review.status()["position"]);
        require(review.transportActive() && loopPosition >= .02 && loopPosition <= .08 + 1./48000 && loopPeak > .19f, "Review loop must wrap through actual stored audio beyond its natural end");
        require(library.reviewControl(originalId,"processed","seek",.02).isEmpty(), "Review must seek to loop start");
        audio.clear(); review.process(audio,dry.getReadPointer(0));
        require(std::abs(audio.getSample(0,0)) < 1.e-6 && audio.getSample(0,127) > .1f, "Review loop must fade the boundary without silencing its interior");
        require(library.reviewControl(originalId,"processed","b",.03).isEmpty() && !static_cast<bool>(review.status()["loop"]) && library.reviewControl(originalId,"processed","loop",1).isNotEmpty(), "Too-short review loops must disable and reject enabling");
        require(library.reviewControl(originalId,"processed","record",0).isNotEmpty() && library.reviewControl(originalId,"processed","seek",std::numeric_limits<double>::quiet_NaN()).isNotEmpty(), "Unsupported and nonfinite review controls must reject");
        library.stopReview(); require(!review.transportActive(), "Review must stop without waiting for an audio callback");
        require(library.reviewWaveform(originalId,"processed").hasProperty("error"),"Stopped review must not expose a stale envelope");
        require(library.status()["reviewId"].toString().isEmpty() && !static_cast<bool>(library.status()["reviewLoading"]) && library.reviewControl(originalId,"processed","play",0).isNotEmpty(), "Stopped review must invalidate its identity and reject stale resume");
        library.preview(originalId, "processed"); library.stopReview(); juce::Thread::sleep(20); require(!review.transportActive(), "Cancelled preview must not restart playback");
        require(library.preview(originalId,"dry").isEmpty(), "A different review version must queue");
        waitFor([&] {return library.status()["reviewVersion"].toString()=="dry";});
        require(!static_cast<bool>(review.status()["loop"]) && static_cast<double>(review.status()["a"])==0 && std::abs(static_cast<double>(review.status()["b"])-4096./48000)<1.e-6, "Loading another version must reset loop bounds to its whole duration");
        audio.clear(); review.process(audio,dry.getReadPointer(0));
        require(std::abs(audio.getSample(0,64)-.125f*juce::Decibels::decibelsToGain(-12.f))<1.e-5 && audio.getSample(0,64)==audio.getSample(1,64), "Dry review must play its own audio identically in both output channels");
        library.stopReview();
        require(library.preview(originalId,"processed").isEmpty() && library.preview(originalId,"dry").isEmpty(), "Rapid review changes must queue");
        waitFor([&] {return library.status()["reviewVersion"].toString()=="dry";});
        require(library.reviewControl(originalId,"processed","pause",0).isNotEmpty(), "Superseded version controls must stay invalid after the latest preview loads");
        library.stopReview();
        const auto snapshot = rig(); require(library.reamp(originalId, snapshot).isEmpty(), "Offline reamp must queue");
        waitFor([&] { return !static_cast<bool>(library.status()["exporting"]); });
        require(library.status()["error"].toString().isEmpty(), "Offline reamp must complete without errors");
        entries = library.list(); juce::var version;
        for (const auto& take : *entries.getArray()) if (take["id"].toString() == originalId) { require(take["versions"].size() == 1, "A successful reamp must add a version"); version = take["versions"][0]; }
        const juce::File output(version["path"].toString()); juce::AudioFormatManager formats; formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(output));
        require(reader && reader->numChannels == 2 && reader->lengthInSamples == 4096 && reader->sampleRate == 48000 && reader->usesFloatingPointData, "Reamp must retain length/rate and export stereo float WAV");
        juce::AudioBuffer<float> rendered(2, 4096); require(reader->read(&rendered, 0, 4096, 0, true, true), "Reamp must decode");
        require(std::abs(rendered.getSample(0, 3000) - .125f * 2 * juce::Decibels::decibelsToGain(6.f)) < .0001f, "Reamp must include current Input gain and guitar chain while excluding Master");
        const auto saved = juce::JSON::parse(juce::File(version["rigPath"].toString()).loadFileAsString());
        require(saved["state"].toString() == snapshot["state"].toString(), "Reamp version must save its exact rig snapshot");
        const auto reampHash = juce::SHA256(juce::File(version["rigPath"].toString())).toHexString();
        const auto audioHash = juce::SHA256(output).toHexString();
        require(library.renameVersion(originalId,version["id"].toString(),"  Singing lead variation  ").isEmpty(),"A reamp version label must queue");
        waitFor([&] {return catalog.loadFileAsString().contains("Singing lead variation");});
        require(juce::SHA256(output).toHexString()==audioHash && juce::SHA256(juce::File(version["rigPath"].toString())).toHexString()==reampHash,"Version renaming must not rename or modify audio/snapshot files");
        require(library.renameVersion(originalId,"processed","Original").isNotEmpty() && library.renameVersion(originalId,"missing","Unknown").isNotEmpty(),"Original and unknown versions cannot be renamed");
        require(library.renameVersion(originalId,version["id"].toString()," ").isNotEmpty() && library.renameVersion(originalId,version["id"].toString(),juce::String::repeatedString("x",81)).isNotEmpty(),"Invalid version labels must reject");
        { PracticeEngine anotherReview; TakeLibrary reopenedVersions(catalog,anotherReview); waitFor([&] {return reopenedVersions.list().size()==2;});
          bool renamed=false; const auto list=reopenedVersions.list(); for(const auto& take:*list.getArray()) if(take["id"].toString()==originalId) renamed=take["versions"][0]["name"].toString()=="Singing lead variation";
          require(renamed,"Reamp version labels must survive reopening the catalog"); }
        const auto recovered = recover(library, originalId, version["id"].toString());
        require(recovered["state"].toString() == saved["state"].toString(), "Recovery must read the chosen reamp, not the current rig");
        for (const auto& source : {juce::String("processed"), juce::String("dry")}) {
            const auto original = recover(library, originalId, source);
            require(static_cast<int>(original["schema"]) == 1 && original["state"].toString() == oldSnapshot["state"].toString(), "Both original versions must recover their exact legacy snapshot");
            AmpSuiteAudioProcessor live(false); live.prepareToPlay(48000,128);
            set(live,"INPUT_GAIN",-3); set(live,"MASTER_VOL",-21); set(live,"METRO_ON",1); set(live,"METRO_BPM",95);
            set(live,"GUITAR_MIX_LEVEL",5); set(live,"GUITAR_MIX_FOCUS",32);
            require(live.applyRig(original,true).isEmpty(), "Recovered legacy rig must enter prepared recall");
            juce::AudioBuffer<float> block(2,128); juce::MidiBuffer midi;
            waitFor([&] { block.clear(); live.processBlock(block,midi); return !static_cast<bool>(live.status()["rigLoading"]); });
            require(!live.status()["message"].toString().startsWith("Load failed:") && live.apvts.getRawParameterValue("AMP_SOURCE")->load() == 4, "Saved amp identity must become active");
            for (const auto& pair : {std::pair<const char*,float>{"INPUT_GAIN",-3}, {"MASTER_VOL",-21}, {"METRO_ON",1}, {"METRO_BPM",95}, {"GUITAR_MIX_LEVEL",5}, {"GUITAR_MIX_FOCUS",32}})
                require(live.apvts.getRawParameterValue(pair.first)->load() == pair.second, "Take recall must preserve calibration, listening and click settings");
            auto xml = juce::XmlDocument::parse(recovered["state"].toString()); auto missing = juce::ValueTree::fromXml(*xml);
            missing.setProperty("modelPath",root.getChildFile("missing.nam").getFullPathName(),nullptr);
            auto invalid = juce::JSON::parse(juce::JSON::toString(recovered)); invalid.getDynamicObject()->setProperty("state",missing.toXmlString());
            const auto before = live.getRig()["state"].toString();
            require(live.applyRig(invalid,true).isNotEmpty() && live.getRig()["state"].toString() == before, "Missing recovered assets must reject without changing the current rig");
        }
        require(juce::SHA256(juce::File(version["rigPath"].toString())).toHexString() == reampHash, "Recovery must never rewrite the reamp snapshot");
        require(library.readRigSnapshot(originalId,"missing",[](juce::var) {}).isNotEmpty(), "Unknown snapshot version must reject before queuing");
        require(library.readRigSnapshot("missing","processed",[](juce::var) {}).isNotEmpty(), "Unknown take must reject before queuing");
        require(library.reamp(originalId, juce::JSON::parse(originalRigFile.loadFileAsString())).isEmpty(), "Original schema-1 take snapshot must reamp");
        require(library.renameVersion(originalId,version["id"].toString(),"Busy").isNotEmpty(),"Version rename must reject during export");
        waitFor([&] { return !static_cast<bool>(library.status()["exporting"]); });
        require(library.status()["error"].toString().isEmpty(), "Legacy take reamp must complete");
        juce::var oldVersion;
        const auto updatedEntries = library.list();
        for (const auto& take : *updatedEntries.getArray()) if (take["id"].toString() == originalId) {
            require(take["versions"].size() == 2, "Legacy reamp must add a separate version"); oldVersion = take["versions"][1];
        }
        std::unique_ptr<juce::AudioFormatReader> oldReader(formats.createReaderFor(juce::File(oldVersion["path"].toString())));
        juce::AudioBuffer<float> oldRendered(2, 4096);
        require(oldReader && oldReader->read(&oldRendered, 0, 4096, 0, true, true), "Legacy reamp must decode");
        for (int channel = 0; channel < 2; ++channel) for (int sample = 0; sample < 4096; ++sample)
            require(oldRendered.getSample(channel, sample) == rendered.getSample(channel, sample), "Legacy and new take snapshots must render identical audio");
        require(juce::SHA256(originalRigFile).toHexString() == originalRigHash, "Reamping must never rewrite a legacy Original rig.json");
        require(juce::SHA256(folder.getChildFile("Guitar dry.wav")).toHexString() == dryHash && juce::SHA256(folder.getChildFile("Guitar processed.wav")).toHexString() == wetHash, "Reamping must leave both originals byte-identical");
    }
    // Bad/missing snapshots report an error without touching audio or the catalog.
    {
        const auto badFolder = root.getChildFile("Broken snapshot"); makeTake(badFolder);
        PracticeEngine review; TakeLibrary library({},review); imported(library,badFolder);
        const auto id=first(library)["id"].toString(), before=juce::JSON::toString(library.list());
        const auto file=badFolder.getChildFile("Original rig.json");
        require(recover(library,id,"processed").hasProperty("error"), "Missing snapshot must fail safely");
        for (const auto& text : {juce::String("not json"), juce::String("{}"), juce::String("{\"schema\":3,\"state\":12}")}) {
            require(file.replaceWithText(text), "Invalid snapshot fixture must write");
            require(recover(library,id,"dry").hasProperty("error"), "Malformed snapshot must fail safely");
        }
        require(file.replaceWithText(juce::String::repeatedString("x",4*1024*1024+1)), "Oversized fixture must write");
        require(recover(library,id,"processed").hasProperty("error"), "Oversized snapshot must fail before parsing");
        require(file.replaceWithText(juce::JSON::toString(rig())), "Valid snapshot fixture must write");
        require(!recover(library,id,"processed").hasProperty("error") && library.status()["error"].toString().isEmpty(), "A valid recovery must clear previous errors");
        require(juce::JSON::toString(library.list()) == before, "Snapshot reads must not edit the catalog");
    }
    // Catalog references cannot redirect recovery to a document outside a take.
    {
        const auto guardedCatalog=root.getChildFile("guarded.xml");
        juce::ValueTree tree("TAKES"), take("TAKE"), version("REAMP");
        take.setProperty("id","guarded",nullptr); take.setProperty("path",folder.getFullPathName(),nullptr);
        version.setProperty("id","outside",nullptr); version.setProperty("rigPath",root.getChildFile("outside.json").getFullPathName(),nullptr);
        require(root.getChildFile("outside.json").replaceWithText(juce::JSON::toString(rig())),"Outside fixture must write");
        take.addChild(version,-1,nullptr); tree.addChild(take,-1,nullptr);
        require(guardedCatalog.replaceWithText(tree.toXmlString()),"Guarded catalog must write");
        PracticeEngine review; TakeLibrary library(guardedCatalog,review); waitFor([&]{return library.list().size()==1;});
        require(recover(library,"guarded","outside").hasProperty("error"),"An outside reamp snapshot path must reject");
        juce::WaitableEvent entered, release, finished;
        require(library.readRigSnapshot("guarded","processed",[&](juce::var) { entered.signal(); release.wait(3000); finished.signal(); }).isEmpty(),"Read must queue without waiting for its callback");
        require(entered.wait(3000),"Callback must run independently of caller");
        std::atomic<bool> secondDone {false};
        require(library.readRigSnapshot("guarded","dry",[&](juce::var) {secondDone.store(true);}).isEmpty(),"One pending read may wait behind worker completion");
        require(library.readRigSnapshot("guarded","dry",[](juce::var) {}).isNotEmpty(),"Duplicate pending reads must reject");
        release.signal(); require(finished.wait(3000),"Blocked callback must complete"); waitFor([&]{return secondDone.load();});
        require(library.renameVersion("guarded","outside","Kept label").isEmpty(),"Metadata edits do not require the version snapshot to load");
        waitFor([&]{return guardedCatalog.loadFileAsString().contains("Kept label");});
        require(guardedCatalog.replaceWithText("broken catalog"),"Failed version rename fixture must write");
        require(library.renameVersion("guarded","outside","Failed label").isEmpty(),"A failing metadata write must reach the worker");
        waitFor([&]{return library.status()["error"].toString().contains("catalog");});
        require(first(library)["versions"][0]["name"].toString()=="Kept label" && guardedCatalog.loadFileAsString()=="broken catalog","Failed version labels must roll back without overwriting the catalog");
        const auto metadataBefore = juce::JSON::toString(library.list());
        const auto beforeNotesRevision = static_cast<int>(library.status()["revision"]);
        require(library.annotate(first(library)["id"].toString(), "Must roll back").isEmpty(), "Notes must queue for disk validation");
        waitFor([&]{return static_cast<int>(library.status()["revision"]) >= beforeNotesRevision + 2 && library.status()["error"].toString().contains("catalog");});
        require(juce::JSON::toString(library.list()) == metadataBefore, "Failed notes must restore the previous in-memory entry");
        require(guardedCatalog.loadFileAsString()=="broken catalog", "Failed notes must not overwrite an unreadable catalog");
    }
    {
        const auto incomplete=root.getChildFile("Incomplete"); makeTake(incomplete);
        require(incomplete.getChildFile("Cassian take.json").replaceWithText("{\"incomplete\":true}"),"Incomplete metadata must write");
        PracticeEngine review; TakeLibrary library({},review); imported(library,incomplete);
        require(library.readRigSnapshot(first(library)["id"].toString(),"processed",[](juce::var) {}).isNotEmpty(),"Interrupted recordings must reject recovery");
    }
    // Reamp tails preserve effects, align the original backing and survive reopening.
    {
        const auto tailFolder = root.getChildFile("Tail take"), tailCatalog = root.getChildFile("tail.xml"); makeTake(tailFolder);
        write(tailFolder.getChildFile("Backing track.wav"),2,4096,48000,.125f);
        const auto originalHash = juce::SHA256(tailFolder.getChildFile("Guitar dry.wav")).toHexString();
        juce::String versionId;
        {
            PracticeEngine review; TakeLibrary library(tailCatalog,review); imported(library,tailFolder);
            const auto id=first(library)["id"].toString();
            AmpSuiteAudioProcessor p(false); set(p,"AMP_SOURCE",4); set(p,"CAB_MODE",3); set(p,"GATE_ON",0);
            set(p,"DELAY_TIME",40); set(p,"DELAY_MIX",50); set(p,"DELAY_FEEDBACK",50);
            const auto snapshot=p.getRig();
            for (double invalid : {-1.,31.,std::numeric_limits<double>::quiet_NaN()})
                require(library.reamp(id,snapshot,invalid).isNotEmpty(),"Invalid tails must reject before queuing");
            require(library.reamp(id,snapshot,.25).isEmpty(),"Reamp with tail must queue");
            waitFor([&]{return !static_cast<bool>(library.status()["exporting"]);});
            require(library.status()["error"].toString().isEmpty(),"Tail render must complete");
            const auto version=first(library)["versions"][0]; versionId=version["id"].toString();
            require(static_cast<juce::int64>(version["frames"])==16096 && static_cast<double>(version["tailSeconds"])==.25,"Tail duration must be cataloged exactly");
            juce::AudioFormatManager formats; formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(version["path"].toString())));
            juce::AudioBuffer<float> audio(2,16096); require(reader && reader->lengthInSamples==16096 && reader->read(&audio,0,16096,0,true,true),"Extended reamp must decode");
            require(audio.getMagnitude(4096,4000)>.001f,"Delay must ring into appended silence");
            const auto guitarOnly=root.getChildFile("Tail guitar.wav"), withBacking=root.getChildFile("Tail mix.wav");
            require(library.videoExport(id,versionId,guitarOnly,false,0,0).isEmpty(),"Extended guitar export must queue");
            waitFor([&]{return !static_cast<bool>(library.status()["exporting"]);});
            require(library.videoExport(id,versionId,withBacking,true,0,0).isEmpty(),"Original backing must mix with extended reamp");
            waitFor([&]{return !static_cast<bool>(library.status()["exporting"]);});
            require(library.status()["error"].toString().isEmpty(),"Extended backing export must succeed");
            reader.reset(formats.createReaderFor(guitarOnly)); juce::AudioBuffer<float> guitar(2,16096);
            require(reader && reader->read(&guitar,0,16096,0,true,true),"Tail guitar soundtrack must decode");
            reader.reset(formats.createReaderFor(withBacking)); require(reader && reader->lengthInSamples==16096 && reader->read(&audio,0,16096,0,true,true),"Tail mix must retain duration");
            require(std::abs(audio.getSample(0,2000)-guitar.getSample(0,2000)-.125f)<.002f,"Backing must stay aligned before its end");
            require(std::abs(audio.getSample(0,8000)-guitar.getSample(0,8000))<.00001f,"Backing must be silent throughout the appended tail");
            const auto tailOnly=root.getChildFile("Tail only.wav");
            require(library.videoExport(id,versionId,tailOnly,true,0,0,.15,.25,.1).isEmpty(),"A short selection entirely in the tail must queue");
            waitFor([&]{return !static_cast<bool>(library.status()["exporting"]);});
            require(library.status()["error"].toString().isEmpty(),"Tail selection beyond backing end must export");
            reader.reset(formats.createReaderFor(tailOnly)); require(reader && reader->lengthInSamples==4800 && reader->read(&audio,0,4800,0,true,true),"Short selection must clamp fades and preserve duration");
            require(std::abs(audio.getSample(0,0))<1.e-6 && std::abs(audio.getSample(0,4799))<1.e-6,"Clamped fades must silence boundaries");
            require(juce::SHA256(tailFolder.getChildFile("Guitar dry.wav")).toHexString()==originalHash,"Tail rendering must preserve the dry recording");
        }
        PracticeEngine review; TakeLibrary reopened(tailCatalog,review); waitFor([&]{return reopened.list().size()==1;});
        require(first(reopened)["versions"][0]["id"].toString()==versionId && static_cast<juce::int64>(first(reopened)["versions"][0]["frames"])==16096,"Tail metadata must survive restart");
    }
    // New processor recordings automatically enter the catalog with their rig.
    {
        AmpSuiteAudioProcessor p(false); p.prepareToPlay(48000, 128); p.practice.setCountIn(0, 120, 4);
        const auto snapshot = p.getRig(); p.practice.record(root, snapshot);
        waitFor([&] { return static_cast<int>(p.practice.status()["recordMode"]) == 2; });
        juce::AudioBuffer<float> audio(2, 128); juce::MidiBuffer midi; audio.clear(); audio.setSample(0, 3, .2f); p.processBlock(audio, midi);
        p.practice.command("pause"); waitFor([&] { return p.takes.list().size() == 1; });
        const auto take = first(p.takes); require(static_cast<bool>(take["originalRig"]) && static_cast<juce::int64>(take["frames"]) == 128, "Finished recordings must auto-register with their rig and exact frame count");
        const juce::File takeFolder(take["path"].toString()); const auto saved = juce::JSON::parse(takeFolder.getChildFile("Original rig.json").loadFileAsString());
        require(saved["state"].toString() == snapshot["state"].toString() && takeFolder.getChildFile("Cassian take.json").existsAsFile(), "Recording must persist the original rig and take metadata");
    }
    // Failed imports and failed/cancelled exports do not register successful versions.
    {
        PracticeEngine review; TakeLibrary library({}, review);
        library.importFolder(root.getChildFile("missing")); waitFor([&] { return library.status()["error"].toString().isNotEmpty(); });
        require(library.list().size() == 0, "Invalid take pair must not enter the catalog");
        const auto longTake = root.getChildFile("Cancellation take"); makeTake(longTake, 960000); imported(library, longTake);
        require(library.status()["error"].toString().isEmpty(), "A successful import must clear an earlier import error");
        const auto id = first(library)["id"].toString(); library.reamp(id, rig()); library.cancelExport();
        waitFor([&] { return !static_cast<bool>(library.status()["exporting"]); });
        require(first(library)["versions"].size() == 0 && longTake.findChildFiles(juce::File::findFiles, false, "Reamp*.wav").size() == 0, "Cancelled reamp must discard its partial output");
        auto invalidRig = std::make_unique<juce::DynamicObject>(); invalidRig->setProperty("schema", 77); invalidRig->setProperty("state", "bad");
        library.reamp(id, juce::var(invalidRig.release())); waitFor([&] { return !static_cast<bool>(library.status()["exporting"]); });
        require(library.status()["error"].toString().isNotEmpty() && first(library)["versions"].size() == 0, "Invalid rig must produce an export error without a version");
        require(longTake.getChildFile("Guitar processed.wav").deleteFile(), "Remove only the temporary fixture's wet audio");
        require(library.preview(id,"processed").isEmpty(), "A catalog entry with moved audio must queue for worker validation");
        waitFor([&] {return !static_cast<bool>(library.status()["reviewLoading"]);});
        require(library.status()["error"].toString().contains("Take audio is missing") && library.status()["reviewId"].toString().isEmpty() && library.reviewControl(id,"processed","play",0).isNotEmpty(), "Missing review audio must clear preparation state, report failure and reject resume");
    }
    std::cout << "Take library, review and offline reamping checks passed\n";
}
