#include "../Source/PracticeEngine.h"
#include <iostream>

namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<typename Predicate> void waitFor(Predicate ready) {
    for (int i = 0; i < 1500; ++i) { if (ready()) return; juce::Thread::sleep(5); }
    require(false, "Practice sections timed out");
}
void load(PracticeEngine& e, const juce::File& file) { e.load(file); waitFor([&] { return !static_cast<bool>(e.status()["loading"]); }); require(e.status()["error"].toString().isEmpty(), "Section fixture must load"); }
void wav(const juce::File& file, int channels = 2, int frames = 96000, float level = .4f) {
    juce::AudioBuffer<float> audio(channels, frames);
    for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < frames; ++i) audio.setSample(ch, i, ch == 0 ? level : -level);
    juce::WavAudioFormat format; auto stream = file.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, static_cast<unsigned>(channels), 32, {}, 0));
    require(writer && writer->writeFromAudioSampleBuffer(audio, 0, frames), "Waveform fixture must write");
}
juce::AudioBuffer<float> render(PracticeEngine& e, int frames) { juce::AudioBuffer<float> audio(2, frames), dry(1, frames); audio.clear(); dry.clear(); e.process(audio, dry.getReadPointer(0)); return audio; }
template<typename Action> void rejects(Action action) { bool failed = false; try { action(); } catch (const std::exception&) { failed = true; } require(failed, "Invalid section storage request must reject"); }
}
void runPracticeSectionChecks()
{
    const auto base = juce::File::getSpecialLocation(juce::File::tempDirectory), folder = base.getNonexistentChildFile("Cassian-sections", "", false);
    require(folder.createDirectory().wasOk(), "Section test folder must create");
    struct Cleanup { juce::File folder, base; ~Cleanup() { if (folder.isAChildOf(base)) folder.deleteRecursively(); } } cleanup {folder, base};
    const auto original = folder.getChildFile("Backing.wav"), renamed = folder.getChildFile("Renamed.wav"), other = folder.getChildFile("Other.wav"), storage = folder.getChildFile("sections");
    wav(original); require(original.copyFileTo(renamed), "Renamed backing copy must create"); wav(other, 1, 96000, .2f);
    PracticeEngine first(262144, storage); first.prepare(48000); first.setCountIn(0, 120, 4); first.command("level", 0);
    require(!first.saveSection("Solo").isEmpty() && !first.recallSection("missing").isEmpty(), "Sections must require a loaded track");
    load(first, original); render(first, 1);
    const auto wave = first.waveform(); const auto key = wave["trackId"].toString();
    require(key == juce::SHA256(original).toHexString() && wave["peaks"].size() == 512 && static_cast<double>(wave["duration"]) == 2, "Waveform must have bounded original-time bins and content identity");
    require(!first.status().hasProperty("peaks"), "Status polling must not repeatedly transmit waveform data");
    for (const auto& bin : *wave["peaks"].getArray()) require(static_cast<double>(bin[0]) < -.39 && static_cast<double>(bin[1]) > .39, "Opposite-phase stereo must remain visible without summing cancellation");
    first.command("a", .3); first.command("b", .6); require(first.saveSection("  Fast run  ").isEmpty(), "Named A-B section must save");
    auto rows = first.status()["sections"]; const auto id = rows[0]["id"].toString();
    require(rows.size() == 1 && rows[0]["name"].toString() == "Fast run", "Section name must trim and receive an ID");
    require(!first.saveSection("   ").isEmpty() && !first.saveSection(juce::String::repeatedString("x", 49)).isEmpty(), "Section names must validate");
    require(first.saveSection("Legato", id).isEmpty() && first.status()["sections"][0]["id"].toString() == id, "Replacing a section must retain its ID");
    first.command("a", .8); first.command("b", .9); first.command("loop", 0); first.command("play"); render(first, 4096);
    require(first.recallSection(id).isEmpty(), "Saved section must recall"); render(first, 1);
    require(!first.isPlaying() && static_cast<bool>(first.status()["loop"]) && std::abs(static_cast<double>(first.status()["position"]) - .3) < 1e-6 && static_cast<double>(first.status()["b"]) == .6, "Recall must pause at A, restore both points, enable looping and never autoplay");
    first.command("speed", .5); waitFor([&] { return !static_cast<bool>(first.status()["loading"]); });
    require(first.status()["sections"].size() == 1 && juce::JSON::toString(first.waveform()["peaks"]) == juce::JSON::toString(wave["peaks"]), "Speed changes must retain original-time sections and waveform envelope");
    first.recallSection(id); first.command("fade", 0); first.command("play"); render(first, 48000);
    require(std::abs(static_cast<double>(first.status()["position"]) - .5) < 1e-5, "Saved sections must loop correctly at slowed playback speed"); first.command("pause");
    load(first, renamed); require(first.status()["sections"].size() == 1 && first.waveform()["trackId"].toString() == key, "Renamed copies must find the same saved sections");
    first.prepare(44100); waitFor([&] { return !static_cast<bool>(first.status()["loading"]); }); require(first.status()["sections"].size() == 1 && first.recallSection(id).isEmpty(), "Sample-rate changes must retain valid section times");
    load(first, other); require(first.status()["sections"].size() == 0 && first.waveform()["trackId"].toString() != key, "Different backing content must have a separate section bank");
    const auto monoPeaks = first.waveform()["peaks"];
    for (const auto& bin : *monoPeaks.getArray()) require(static_cast<double>(bin[0]) == 0 && static_cast<double>(bin[1]) > .19, "Mono waveforms must preserve their visible peak envelope");
    load(first, original);

    // Reopen and stale writers merge changes to individual rows rather than
    // replacing a stale list, including deletion and missing-target errors.
    PracticeEngine second(262144, storage); second.prepare(48000); second.setCountIn(0, 120, 4); load(second, original);
    require(second.status()["sections"].size() == 1, "Saved sections must survive a new engine instance");
    first.command("a", .1); first.command("b", .2); first.saveSection("Intro"); second.command("a", 1); second.command("b", 1.5); require(second.saveSection("Solo").isEmpty() && second.status()["sections"].size() == 3, "Stale section writers must preserve another instance's additions");
    require(first.removeSection(id).isEmpty(), "Section must delete"); require(!second.recallSection(id).isEmpty() && second.status()["sections"].size() == 2, "Deleted targets must report failure and refresh without returning stale tones");
    require(second.saveSection("Outro").isEmpty() && second.status()["sections"].size() == 3, "Subsequent stale saves must not resurrect a deletion");
    second.setCountIn(1, 120, 4); second.command("play"); require(!second.saveSection("Blocked").isEmpty() && !second.recallSection(second.status()["sections"][0]["id"].toString()).isEmpty(), "Count-in must lock section editing and recall"); second.command("stop");
    require(second.record(folder).isEmpty(), "Section fixture take must arm"); require(!second.saveSection("Blocked").isEmpty() && !second.removeSection(second.status()["sections"][0]["id"].toString()).isEmpty(), "Recording must lock section editing"); second.command("pause"); waitFor([&] { return static_cast<int>(second.status()["recordMode"]) == 0; });

    // Corrupt metadata must not stop backing audio or be silently overwritten.
    const auto document = storage.getChildFile(key + ".json"); const auto saved = document.loadFileAsString(); require(document.replaceWithText("{broken"), "Malformed section fixture must write");
    require(!second.saveSection("Never overwrite").isEmpty() && document.loadFileAsString() == "{broken", "Failed metadata reads must preserve the existing document");
    load(second, original); require(second.status()["sectionError"].toString().isNotEmpty() && second.status()["sections"].size() == 0 && second.waveform()["peaks"].size() == 512, "Corrupt sections must report separately while backing and waveform load");
    second.setCountIn(0, 120, 4); second.command("play"); require(render(second, 4096).getMagnitude(0, 0, 4096) > .01f, "Backing must remain audible despite corrupt section metadata"); second.command("pause"); require(document.replaceWithText(saved), "Valid section fixture must restore"); load(second, original);
    const auto before = juce::JSON::toString(second.waveform()); const auto beforeRows = juce::JSON::toString(second.status()["sections"]);
    second.load(folder.getChildFile("missing.wav")); waitFor([&] { return !static_cast<bool>(second.status()["loading"]); });
    require(juce::JSON::toString(second.waveform()) == before && juce::JSON::toString(second.status()["sections"]) == beforeRows, "Failed track import must preserve waveform and sections");
    second.command("speed", .75); second.command("cancelLoad"); const auto revision = second.waveform()["revision"]; juce::Thread::sleep(30);
    require(second.waveform()["revision"] == revision, "Cancelled preparation must not publish a stale waveform");

    PracticeSections store(storage); rejects([&] { store.load("../escape", 2); });
    rejects([&] { store.change(key, 2, {}, "Bad range", 1.5, 1, false); });
    rejects([&] { store.change(key, 2, {}, "Too long", 0, 3, false); });
    rejects([&] { store.change(key, 2, {}, "NaN", 0, std::numeric_limits<double>::quiet_NaN(), false); });
    rejects([&] { store.change(key, 2, "missing", "Missing", .2, .5, false); });
    auto latest = store.load(key, 2);
    for (int i = latest.size(); i < 32; ++i) store.change(key, 2, {}, "Part " + juce::String(i), .2, .5, false);
    rejects([&] { store.change(key, 2, {}, "Too many", .2, .5, false); }); require(store.load(key, 2).size() == 32, "Section storage must enforce a bounded per-track list");
    const auto tiny = folder.getChildFile("tiny.wav"); wav(tiny, 1, 20); load(second, tiny); require(second.waveform()["peaks"].size() == 20, "Short tracks must use one bin per frame without empty buckets");
    std::cout << "Practice waveform, section identity/persistence, stale writers, looping and rollback checks passed\n";
}
