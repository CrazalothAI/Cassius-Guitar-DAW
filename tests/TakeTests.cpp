#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
template <typename Predicate> void waitFor(Predicate ready) {
    for (int n = 0; n < 2000; ++n) { if (ready()) return; juce::Thread::sleep(5); }
    require(false, "Take library worker timed out");
}
void set(AmpSuiteAudioProcessor& p, const char* id, float value) { auto* param = p.apvts.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(value)); }
void write(const juce::File& file, int channels, int frames, double rate, float value) {
    juce::WavAudioFormat format; auto stream = file.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), rate, static_cast<unsigned>(channels), 32, {}, 0));
    require(writer != nullptr, "Take fixture writer must open");
    juce::AudioBuffer<float> audio(channels, 1024);
    for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < 1024; ++i) audio.setSample(ch, i, value);
    for (int offset = 0; offset < frames; offset += 1024) require(writer->writeFromAudioSampleBuffer(audio, 0, juce::jmin(1024, frames - offset)), "Take fixture must write");
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
    set(p, "GATE_ON", 0); set(p, "REVERB_MIX", 0); set(p, "DELAY_MIX", 0); set(p, "MASTER_VOL", -48); return p.getRig();
}
}
void runTakeChecks()
{
    const auto base = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root = base.getNonexistentChildFile("Cassian-take-tests-" + juce::Uuid().toString(), "", false);
    require(root.createDirectory().wasOk(), "Take test directory must create");
    struct Cleanup { juce::File folder, base; ~Cleanup() { if (folder.isAChildOf(base)) folder.deleteRecursively(); } } cleanup {root, base};
    const auto folder = root.getChildFile("Original take"), catalog = root.getChildFile("takes.xml"); makeTake(folder);
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
        require(juce::SHA256(folder.getChildFile("Guitar dry.wav")).toHexString() == dryHash && juce::SHA256(folder.getChildFile("Guitar processed.wav")).toHexString() == wetHash, "Reamping must leave both originals byte-identical");
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
