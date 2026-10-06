#include "../Source/PluginProcessor.h"
#include "../Source/BundledSoundBank.h"
#include <iostream>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void set(AmpSuiteAudioProcessor& p, const char* id, float value) { auto* param = p.apvts.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(value)); }
void ready(AmpSuiteAudioProcessor& p) {
    for (int i = 0; i < 600; ++i) {
        const auto status = p.status();
        if (!static_cast<bool>(status["rigLoading"])) { require(!status["message"].toString().startsWith("Load failed:"), "Starter rig must prepare"); return; }
        juce::Thread::sleep(10);
    }
    require(false, "Starter recall timed out");
}
juce::ValueTree state(AmpSuiteAudioProcessor& p) { return juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString())); }
}

void runStartingRigChecks(const juce::File& fixture)
{
    const auto catalog = AmpSuiteAudioProcessor::startingRigCatalog();
    require(static_cast<int>(catalog["version"]) == 1 && catalog["rigs"].isArray() && catalog["rigs"].size() >= 12, "Embedded starter catalog must be complete");
    AmpSuiteAudioProcessor p(false); p.prepareToPlay(48000, 256);
    p.requestFile(true, fixture); p.requestPedal(fixture); ready(p);
    require(p.boardCommand("convert", {}).isEmpty(), "Contaminated rig must convert"); ready(p);
    set(p, "EQ_ON", 1); set(p, "EQ_FOCUS", 8); set(p, "PEDAL_ON", 1);
    auto duplicate = std::make_unique<juce::DynamicObject>(); duplicate->setProperty("id", state(p).getChildWithName("PEDALBOARD").getChildWithProperty("type", "neural-pedal")["id"]);
    require(p.boardCommand("duplicate", juce::var(duplicate.release())).isEmpty(), "Previous rig must include a second captured pedal"); ready(p);
    require(p.storeScene(0, "Previous rig").isEmpty(), "Previous scene must store");
    set(p, "INPUT_GAIN", -3); set(p, "MASTER_VOL", -21); set(p, "METRO_ON", 1); set(p, "METRO_BPM", 95); set(p, "GUITAR_MIX_LEVEL", 5); set(p, "GUITAR_MIX_FOCUS", 32);
    const auto intact = p.getRig()["state"].toString();
    require(p.loadStartingRig("factory.missing").isNotEmpty() && p.getRig()["state"].toString() == intact, "Unknown starter must leave the current complete rig unchanged");
    juce::StringArray ids; double quietest = 10, loudest = -120;
    for (const auto& rig : *catalog["rigs"].getArray()) {
        const auto id = rig["id"].toString(); require(!ids.contains(id), "Starter identities must be unique"); ids.add(id);
        require(p.loadStartingRig(id).isEmpty(), "Every starter must load on a library-free installation"); ready(p);
        const auto saved = state(p); const auto status = p.status();
        require(p.validateRigDocument(p.getRig()).isEmpty() && PedalboardState::serial(saved), "Starter must be a complete validated serial rig");
        for (const auto* stage : {"model", "ir", "irB", "pedal", "pedal1", "ambience", "ambience1"}) {
            require(saved[juce::String(stage) + "Path"].toString().isEmpty() && saved[juce::String(stage) + "Id"].toString().isEmpty(), "Starters cannot inherit any external asset references");
        }
        require(saved.getChildWithName("PEDALBOARD").getNumChildren() == rig["board"].size(), "Starter must replace the previous pedal topology");
        require(status["scenes"]["active"].toString() == "-1" && !static_cast<bool>(status["scenes"]["slots"][0]["stored"]), "Starter must clear the previous scene bank");
        require(status["activeRigId"].toString() == id && static_cast<bool>(status["activeRigStarter"]) && !static_cast<bool>(status["activeRigSaved"]) && !static_cast<bool>(status["activeRigEdited"]), "Fresh starter must have its own unedited factory identity");
        for (const auto& pair : {std::pair<const char*, float>{"INPUT_GAIN", -3}, {"MASTER_VOL", -21}, {"METRO_ON", 1}, {"METRO_BPM", 95}, {"GUITAR_MIX_LEVEL", 5}, {"GUITAR_MIX_FOCUS", 32}})
            require(p.apvts.getRawParameterValue(pair.first)->load() == pair.second, "Starter must preserve calibration, metronome and listening controls");
        // Repeatable plucked harmonic signal checks headroom and gross level
        // differences; this is not a real guitar audition or a LUFS measurement.
        juce::AudioBuffer<float> audio(2, 256); double energy = 0; int samples = 0; float peak = 0;
        for (int block = 0; block < 320; ++block) {
            audio.clear(); for (int i = 0; i < 256; ++i) {
                const auto t = (block * 256 + i) / 48000.0; const auto pluck = std::fmod(t, .25);
                const auto body = std::sin(juce::MathConstants<double>::twoPi * 220 * t) + .5 * std::sin(juce::MathConstants<double>::twoPi * 440 * t) + .2 * std::sin(juce::MathConstants<double>::twoPi * 880 * t);
                audio.setSample(0, i, static_cast<float>((.012 + .15 * std::exp(-pluck * 16)) * body));
            }
            p.renderGuitarOffline(audio, 256);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) { const auto x = audio.getSample(ch, i); require(std::isfinite(x), "Every starter must render finite stereo audio"); peak = juce::jmax(peak, std::abs(x)); if (block >= 30) { energy += x * x; ++samples; } }
        }
        const auto db = 20 * std::log10(std::sqrt(energy / samples)); quietest = juce::jmin(quietest, db); loudest = juce::jmax(loudest, db);
        require(db > -60 && peak < .9f, "Starter must produce audible output with guitar-path headroom");
        std::cout << "Starter " << rig["name"].toString() << ": synthetic guitar RMS " << db << " dBFS; peak " << peak << '\n';
    }
    std::cout << "Starter synthetic level spread: " << loudest - quietest << " dB\n";
    require(loudest - quietest < 6, "Built-in starter levels must stay within six dB on the reference signal");
    const auto intactAfterBuiltins = p.getRig()["state"].toString();
    require(p.loadStartingRig("factory.capture-red2-tight").startsWith("Missing sound:") && p.getRig()["state"].toString() == intactAfterBuiltins, "Missing capture recipe must reject atomically without replacing it by an old voice");
    set(p, "AMP_MID", 3); require(static_cast<bool>(p.status()["activeRigEdited"]), "Editing a factory rig must mark it edited");
    require(p.saveRig("My ambient clean").isEmpty() && static_cast<bool>(p.status()["activeRigSaved"]) && !static_cast<bool>(p.status()["activeRigStarter"]), "Saving a starter must create a user rig");
    juce::MemoryBlock snapshot; p.getStateInformation(snapshot);
    AmpSuiteAudioProcessor restored(false); restored.prepareToPlay(44100, 257); restored.setStateInformation(snapshot.getData(), static_cast<int>(snapshot.getSize())); ready(restored);
    require(restored.status()["activeRigName"].toString() == "My ambient clean" && PedalboardState::equal(state(p), state(restored)), "Starter-derived user rig must recall through a native session");
    juce::TemporaryFile pack(".zip"); require(p.exportRigPack(pack.getFile()).isEmpty(), "Starter must export as a portable rig"); juce::ZipFile zip(pack.getFile()); require(zip.getNumEntries() == 1, "Built-in starter pack must need no external assets");
    const auto assetId = p.getLibrary()["assets"][0]["id"].toString();
    auto changes = std::make_unique<juce::DynamicObject>(); changes->setProperty("styles", "jazz, blues"); changes->setProperty("speaker", "V30"); changes->setProperty("gain", "breakup");
    require(p.editAsset(assetId, juce::var(changes.release())), "Structured sound metadata must save");
    const auto before = juce::JSON::toString(p.getLibrary());
    changes = std::make_unique<juce::DynamicObject>(); changes->setProperty("name", "Should not commit"); changes->setProperty("gain", "invented");
    require(!p.editAsset(assetId, juce::var(changes.release())) && juce::JSON::toString(p.getLibrary()) == before, "Invalid gain metadata must reject without partial edits");
    // Optional real bank exercise uses a different empty user directory. No
    // developer Download paths can leak into the recall or exported pack.
    const auto bankPath = juce::SystemStats::getEnvironmentVariable("CASSIAN_TEST_SOUND_BANK", "");
    if (bankPath.isNotEmpty()) {
        const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Cassian-bank-test", "", false);
        struct Cleanup { juce::File folder; ~Cleanup() { folder.deleteRecursively(); } } cleanup {root};
        LibraryStore store(root); AssetLibrary library;
        const auto count = BundledSoundBank::install(juce::File(bankPath), store, library);
        require(count > 0 && BundledSoundBank::install(juce::File(bankPath), store, library) == 0, "Relocated bank must import idempotently");
        AmpSuiteAudioProcessor player(true, root); player.prepareToPlay(48000, 256);
        double lowestCapture = 10, highestCapture = -120;
        for (const auto& rig : *catalog["captureRigs"].getArray()) {
            require(player.loadStartingRig(rig["id"].toString()).isEmpty(), "Every capture recipe must resolve on a new library"); ready(player);
            const auto recalled = state(player);
            for (const auto& ref : rig["assets"].getDynamicObject()->getProperties()) {
                const auto stage = ref.name.toString();
                require(recalled[stage + "Id"].toString() == ref.value["id"].toString(), "Recipe must retain each exact capture variant");
                require(juce::File(recalled[stage + "Path"].toString()).isAChildOf(root), "Recipe must use the new PC's managed storage");
            }
            require(!static_cast<bool>(player.status()["activeRigEdited"]), "Capture recipe baseline must include all assets");
            player.prepareToPlay(48000,256);
            juce::AudioBuffer<float> audio(2,256); float peak = 0; double energy = 0;
            for (int block = 0; block < 180; ++block) {
                audio.clear(); for (int i = 0; i < 256; ++i) { const auto t = (block*256+i)/48000.; audio.setSample(0,i,static_cast<float>(.06*std::sin(t*220*juce::MathConstants<double>::twoPi))); }
                player.renderGuitarOffline(audio,256); for (int ch=0;ch<2;++ch) for (int i=0;i<256;++i) {const auto x=audio.getSample(ch,i);require(std::isfinite(x), "Real recipe must render finite audio"); peak=juce::jmax(peak,std::abs(x));energy+=x*x;}
            }
            require(peak > .0001f && peak < .9f, "Real recipe must produce bounded audible output with guitar-path headroom");
            const auto db = 10*std::log10(energy/(180*256*2)); lowestCapture = juce::jmin(lowestCapture,db); highestCapture = juce::jmax(highestCapture,db);
            std::cout << "Capture recipe " << rig["name"].toString() << ": RMS " << db << " dBFS; peak " << peak << '\n';
        }
        require(highestCapture-lowestCapture < 6, "Capture recipe levels must stay within six dB on the reference signal");
        std::cout << "Capture recipe synthetic level spread: " << highestCapture-lowestCapture << " dB\n";
        std::cout << count << " bank assets and " << catalog["captureRigs"].size() << " exact capture recipes verified on relocated empty storage\n";
    }
    std::cout << "Starter identity, complete recall, global isolation, native restore, pack and metadata checks passed\n";
}
