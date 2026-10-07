#include "../Source/PluginProcessor.h"
#include <iostream>

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
}
void runTakeChecks()
{
    const auto base = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root = base.getNonexistentChildFile("Cassian-take-tests-" + juce::Uuid().toString(), "", false);
    require(root.createDirectory().wasOk(), "Take test directory must create");
    struct Cleanup { juce::File folder, base; ~Cleanup() { if (folder.isAChildOf(base)) folder.deleteRecursively(); } } cleanup {root, base};
    const auto folder = root.getChildFile("Original take"), catalog = root.getChildFile("takes.xml"); makeTake(folder);
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
        const auto loud=root.getChildFile("Video loud " + juce::String(rate) + ".wav");
        require(video.videoExport(id,"processed",loud,true,12,12).isEmpty(),"Hot export must queue");waitFor([&]{return !static_cast<bool>(video.status()["exporting"]);});
        reader.reset(formats.createReaderFor(loud));require(reader && reader->read(&audio,0,4800,0,true,true),"Protected export must decode");
        require(audio.getMagnitude(0,4800)<.892f && audio.getMagnitude(0,4800)>.88f,"Hot mixed soundtrack must retain -1 dBFS peak headroom");
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
        for (const auto range : {std::pair<double,double>{-.1,.08}, {.08,.02}, {0,.2}})
            require(video.videoExport(id,"processed",root.getChildFile("invalid.wav"),false,0,0,range.first,range.second,.01).isNotEmpty(),"Invalid export bounds must reject before queuing");
        require(video.videoExport(id,"processed",root.getChildFile("invalid.wav"),false,0,0,0,-1,.101).isNotEmpty(),"Oversized fades must reject");
        const auto cancelledFile=root.getChildFile("Cancelled " + juce::String(rate) + ".wav");
        video.videoExport(id,"processed",cancelledFile,true,0,0); video.cancelExport();
        waitFor([&]{return !static_cast<bool>(video.status()["exporting"]);});
        require(!cancelledFile.exists(),"Cancelled video mix must discard partial output");
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
        require(catalog.loadFileAsString().contains("Second take"), "Catalog edits must preserve another instance's take");
    }
    {
        PracticeEngine review; review.prepare(48000); TakeLibrary library(catalog, review);
        waitFor([&] { return library.list().size() == 2; });
        auto entries = library.list(); bool found = false;
        for (const auto& take : *entries.getArray()) if (take["id"].toString() == originalId) found = take["name"].toString() == "Favorite lead" && static_cast<bool>(take["favorite"]);
        require(found, "Take names and favorites must survive restart");
        // Review processed audio bypasses all guitar effects, while stop cancels
        // queued previews instead of letting a stale request start playback.
        require(library.preview(originalId, "processed").isEmpty(), "Processed take preview must queue");
        waitFor([&] { return review.transportActive(); });
        juce::AudioBuffer<float> audio(2, 128), dry(1, 128); audio.clear(); dry.clear();
        review.process(audio, dry.getReadPointer(0)); require(audio.getMagnitude(0, 0, 128) > .19f, "Processed review must preserve stored audio at review gain");
        library.stopReview(); require(!review.transportActive(), "Review must stop without waiting for an audio callback");
        library.preview(originalId, "processed"); library.stopReview(); juce::Thread::sleep(20); require(!review.transportActive(), "Cancelled preview must not restart playback");
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
        require(library.reamp(originalId, juce::JSON::parse(originalRigFile.loadFileAsString())).isEmpty(), "Original schema-1 take snapshot must reamp");
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
    }
    std::cout << "Take library, review and offline reamping checks passed\n";
}
