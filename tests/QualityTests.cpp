#include "../Source/PluginProcessor.h"
#include <iostream>
#include <thread>
#include <tuple>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void set(AmpSuiteAudioProcessor& p, const char* id, float x) { auto* parameter = p.apvts.getParameter(id); parameter->setValueNotifyingHost(parameter->convertTo0to1(x)); }
float get(AmpSuiteAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue(id)->load(); }
void settle(AmpSuiteAudioProcessor& p) {
    for (int i = 0; i < 600; ++i) {
        const auto status = p.status();
        if (status["message"].toString().startsWith("Load failed:")) throw std::runtime_error(status["message"].toString().toStdString());
        if (!p.getRig().hasProperty("error")) return;
        juce::Thread::sleep(10);
    }
    require(false, "Quality test rig did not settle");
}
void neutral(AmpSuiteAudioProcessor& p) { set(p, "AMP_SOURCE", 4); set(p, "CAB_MODE", 3); set(p, "GATE_ON", 0); set(p, "REVERB_MIX", 0); set(p, "MASTER_VOL", -24); }
void play(AmpSuiteAudioProcessor& p, int blocks = 400, double rate = 48000) {
    juce::AudioBuffer<float> block(2, 128); juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b) {
        block.clear();
        for (int i = 0; i < 128; ++i) block.setSample(0, i, .05f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi * 220 * (b * 128 + i) / rate)));
        p.processBlock(block, midi);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i)
            require(std::isfinite(block.getSample(ch, i)) && std::abs(block.getSample(ch, i)) <= 1, "Quality render must stay finite and bounded");
    }
}
struct Folder {
    juce::TemporaryFile token {".quality"};
    juce::File root = token.getFile();
    Folder() { require(root.createDirectory().wasOk(), "Temporary quality folder must open"); }
    ~Folder() { if (root.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory)) && root.getFileName().startsWith("temp_")) root.deleteRecursively(); }
};
juce::var wrap(const juce::ValueTree& state) { auto o = std::make_unique<juce::DynamicObject>(); o->setProperty("schema", 1); o->setProperty("state", state.createXml()->toString()); return juce::var(o.release()); }
juce::ValueTree stateOf(juce::var rig) { auto xml = juce::XmlDocument::parse(rig["state"].toString()); require(xml != nullptr, "Rig state must parse"); return juce::ValueTree::fromXml(*xml); }
}

void runQualityChecks(const juce::File& fixture)
{
    // Smooth tone automation preserves the first sample and reaches its intended gain.
    {
        ToneStack tone; tone.prepare(48000); juce::AudioBuffer<float> audio(2, 128);
        auto block = [&] { for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) audio.setSample(ch, i, .05f); tone.process(audio); };
        for (int b = 0; b < 30; ++b) block(); const float previous = audio.getSample(0, 127);
        tone.update(12, 0, 0, 0); block();
        require(std::abs(audio.getSample(0, 0) - previous) < 1e-6, "Tone automation must not jump on its first sample");
        for (int b = 0; b < 150; ++b) block();
        require(std::abs(20 * std::log10(audio.getSample(0, 127) / .05f) - 12) < .2f, "Smoothed bass must reach the requested shelf gain");
    }
    // Publication can overlap rendering; only process/prepare call JUCE convolution APIs.
    for (const double rate : {44100., 48000., 96000.}) {
        IrLoader ir; ir.prepare({rate, 128, 2}); std::atomic<bool> done {false}; std::exception_ptr error;
        std::thread loader([&] { try {
            for (int k = 0; k < 32; ++k) { juce::AudioBuffer<float> impulse(2, 96); impulse.clear(); impulse.setSample(0, k % 24, .8f); impulse.setSample(1, k % 24 + 4, -.6f); ir.load(std::move(impulse), rate); juce::Thread::sleep(1); }
        } catch (...) { error = std::current_exception(); } done.store(true); });
        juce::AudioBuffer<float> audio(2, 128); bool finite = true;
        for (int b = 0; b < 1200 || !done.load(); ++b) {
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) audio.setSample(ch, i, .02f * std::sin(static_cast<float>(i + b * 128) * .07f));
            juce::dsp::AudioBlock<float> block(audio); juce::dsp::ProcessContextReplacing<float> context(block); ir.process(context);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) finite = finite && std::isfinite(audio.getSample(ch, i)) && std::abs(audio.getSample(ch, i)) < 1;
            if (b % 40 == 0) juce::Thread::sleep(1);
        }
        loader.join(); if (error) std::rethrow_exception(error);
        require(finite && ir.isLoaded(), "Repeated concurrent IR loads must remain bounded at every supported rate");
    }
    // A mono input gets genuine stereo modulation; bypass is sample-exact.
    for (const double rate : {44100., 48000., 96000.}) {
        StereoChorus chorus; chorus.prepare({rate, 128, 2}, {0, .8f, 60});
        juce::AudioBuffer<float> audio(2, 128); double difference = 0;
        for (int b = 0; b < 400; ++b) {
            if (b == 40) chorus.configure({65, .8f, 60});
            for (int i = 0; i < 128; ++i) { const float x = .04f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi * 440 * (b * 128 + i) / rate)); audio.setSample(0, i, x); audio.setSample(1, i, x); }
            const float dry = audio.getSample(0, 19);
            juce::dsp::AudioBlock<float> block(audio); juce::dsp::ProcessContextReplacing<float> context(block); chorus.process(context);
            if (b < 40) require(audio.getSample(0, 19) == dry && audio.getSample(1, 19) == dry, "Chorus at zero mix must preserve the dry samples exactly");
            if (b > 100) for (int i = 0; i < 128; ++i) { const float d = audio.getSample(0, i) - audio.getSample(1, i); difference += d * d; }
        }
        require(difference > .01, "Chorus must create stereo width from a mono input");
    }
    // Tempo sync and adjustable feedback are measured from actual impulse echoes.
    for (const double rate : {44100., 48000., 96000.}) {
        AmpSuiteAudioProcessor p(false); neutral(p); set(p, "DELAY_SYNC", 1); set(p, "DELAY_DIVISION", 2); set(p, "METRO_BPM", 120); set(p, "DELAY_MIX", 100); set(p, "DELAY_FEEDBACK", 0);
        p.prepareToPlay(rate, 128); juce::AudioBuffer<float> audio(2, 128); juce::MidiBuffer midi;
        for (int b = 0; b < 80; ++b) { audio.clear(); p.processBlock(audio, midi); }
        const int size = static_cast<int>(rate * 1.2); float peak = 0, second = 0; int peakAt = 0;
        for (int start = 0; start < size; start += 128) {
            audio.clear(); if (start == 0) audio.setSample(0, 0, .1f); p.processBlock(audio, midi);
            for (int i = 0; i < 128; ++i) { const int sample = start + i; const float x = std::abs(audio.getSample(0, i));
                if (sample > 256 && sample < rate * .6 && x > peak) { peak = x; peakAt = sample; }
                if (sample > rate * .65) second = std::max(second, x);
            }
        }
        require(peak > .001f && std::abs(peakAt - rate * .375) <= 1, "Dotted eighth at 120 BPM must repeat at 375 ms");
        require(second < 1e-6f, "Zero delay feedback must produce no second echo");
    }
    // Reverb pre-delay moves only the ambience; voicing changes its decay.
    {
        auto renderRoom = [](float preDelay, float voice) {
            AmpSuiteAudioProcessor p(false); neutral(p); set(p, "REVERB_MIX", 100); set(p, "REVERB_SIZE", 80);
            set(p, "REVERB_PREDELAY", preDelay); set(p, "REVERB_STYLE", voice); p.prepareToPlay(48000, 128);
            juce::AudioBuffer<float> audio(2, 128); juce::MidiBuffer midi;
            for (int b = 0; b < 80; ++b) { audio.clear(); p.processBlock(audio, midi); }
            double energy = 0; int first = -1; float dry = 0;
            for (int b = 0; b < 800; ++b) {
                audio.clear(); if (b == 0) audio.setSample(0, 0, .1f); p.processBlock(audio, midi);
                if (b == 0) dry = audio.getSample(0, 0);
                for (int i = 0; i < 128; ++i) { const int frame = b * 128 + i; const float x = audio.getSample(0, i);
                    if (frame > 128 && std::abs(x) > 1e-8f && first < 0) first = frame;
                    if (frame > 24000) energy += x * x;
                }
            }
            return std::tuple<int, double, float>{first, energy, dry};
        };
        const auto room = renderRoom(0, 0), delayed = renderRoom(100, 0), hall = renderRoom(0, 2);
        require(std::get<0>(room) > 128 && std::abs(std::get<0>(delayed) - std::get<0>(room) - 4800) <= 1, "100 ms reverb pre-delay must move the wet onset by 4800 samples");
        require(std::abs(std::get<2>(delayed) - std::get<2>(room)) < 1e-7f, "Reverb pre-delay must not delay the dry guitar");
        require(std::get<1>(hall) > std::get<1>(room) * 1.1, "Hall voicing must extend the measured reverb decay");
    }
    // Metadata-guided input/output gain and clean compression remain controllable.
    {
        AmpSuiteAudioProcessor p(false); neutral(p); p.requestPedal(fixture); settle(p); set(p, "PEDAL_ON", 1); p.prepareToPlay(48000, 128);
        play(p); const double before = p.status()["postPedal"];
        set(p, "PEDAL_OUTPUT", -12); play(p); const double after = p.status()["postPedal"];
        require(before > 0 && std::abs(after / before - juce::Decibels::decibelsToGain(-12.0)) < .015, "Pedal output trim must change level without changing capture drive");
        set(p, "PEDAL_ON", 0); set(p, "AMP_SOURCE", 1); set(p, "CLEAN_COMP", 100); set(p, "COMP_THRESH", -35); set(p, "COMP_RATIO", 8); play(p);
        const float compressed = p.status()["postAmp"]; set(p, "COMP_THRESH", 0); set(p, "COMP_RATIO", 1); play(p);
        require(static_cast<float>(p.status()["postAmp"]) > compressed * 1.5f, "Adjustable clean compressor must change the measured dynamics");
        neutral(p); play(p, 1000); const auto reference = p.getRig(); set(p, "AMP_OUT", 6); play(p, 1000);
        require(p.applyRig(reference, true, true).isEmpty(), "Level-matched comparison must queue"); settle(p);
        require(std::abs(get(p, "AMP_OUT") - 6) < .2f && get(p, "MASTER_VOL") == -24, "Comparison matching must adjust amp output and preserve listening level");
        auto legacy = stateOf(reference); for (size_t i = 41; i < Params::definitions.size(); ++i) legacy.removeChild(legacy.getChildWithProperty("id", Params::definitions[i].id), nullptr);
        set(p, "CHORUS_MIX", 65); require(p.applyRig(wrap(legacy)).isEmpty(), "Existing 41-parameter rigs must still recall"); settle(p);
        require(get(p, "CHORUS_MIX") == 0 && get(p, "DELAY_FEEDBACK") == 35 && get(p, "COMP_RATIO") == 2.5f, "Legacy rigs must restore bypassed/legacy effect defaults");
    }
    // Selecting another slot must not cancel a model already preparing in this slot.
    {
        AmpSuiteAudioProcessor p(false); neutral(p); p.prepareToPlay(48000, 128);
        for (int attempt = 0; attempt < 12; ++attempt) {
            p.requestFile(true, fixture); play(p, 1); juce::Thread::sleep(1); p.requestPedal(fixture);
            for (int i = 0; i < 600 && p.getRig().hasProperty("error"); ++i) { play(p, 1); juce::Thread::sleep(1); }
            settle(p);
            const auto state = stateOf(p.getRig());
            require(state["modelPath"].toString() == fixture.getFullPathName() && state["pedalPath"].toString() == fixture.getFullPathName(), "Independent slot requests must both finish");
            p.requestFile(true, {}); p.requestPedal({}); settle(p);
        }
    }
    // Rig preparation overlaps real callbacks; malformed captures cannot partially commit.
    {
        Folder folder; AmpSuiteAudioProcessor p(false); neutral(p); p.prepareToPlay(48000, 128); play(p, 100);
        const auto clean = p.getRig(); auto capture = stateOf(clean);
        capture.setProperty("modelPath", fixture.getFullPathName(), nullptr);
        capture.setProperty("modelId", "amp:" + juce::SHA256(fixture).toHexString(), nullptr);
        capture.getChildWithProperty("id", "AMP_SOURCE").setProperty("value", 3, nullptr);
        require(p.applyRig(wrap(capture)).isEmpty(), "Complete capture rig must queue during playback");
        for (int i = 0; i < 600 && p.getRig().hasProperty("error"); ++i) { play(p, 4); juce::Thread::sleep(1); }
        settle(p); require(get(p, "AMP_SOURCE") == 3, "Prepared rig must activate while callbacks continue"); play(p, 80);
        const auto before = p.getRig(); const auto bad = folder.root.getChildFile("Invalid.nam"); require(bad.replaceWithText("{}"), "Malformed test capture must write");
        auto invalid = stateOf(before); invalid.setProperty("modelPath", bad.getFullPathName(), nullptr);
        invalid.setProperty("modelId", "amp:" + juce::SHA256(bad).toHexString(), nullptr);
        invalid.getChildWithProperty("id", "AMP_OUT").setProperty("value", -12, nullptr);
        require(p.applyRig(wrap(invalid)).isEmpty(), "Malformed capture reaches preparation after document validation");
        for (int i = 0; i < 600 && !p.status()["message"].toString().startsWith("Load failed:"); ++i) { play(p, 2); juce::Thread::sleep(1); }
        require(p.status()["message"].toString().startsWith("Load failed:"), "Malformed capture preparation must report failure");
        require(get(p, "AMP_OUT") == 0 && stateOf(p.getRig())["modelPath"] == stateOf(before)["modelPath"], "Failed preparation must retain both settings and active capture");
        require(p.applyRig(before).isEmpty() && p.applyRig(clean).isEmpty(), "Rapid replacement rig requests must queue");
        for (int i = 0; i < 600 && p.getRig().hasProperty("error"); ++i) { play(p, 2); juce::Thread::sleep(1); }
        settle(p); require(get(p, "AMP_SOURCE") == 4, "The newest rig request must win");
    }
    // Hash-based filenames must retain full-rig routing hints when metadata is absent.
    {
        Folder folder; const auto original = folder.root.getChildFile("Example FullRig.nam");
        auto config = juce::JSON::parse(fixture.loadFileAsString());
        if (auto* metadata = config["metadata"].getDynamicObject()) metadata->removeProperty("gear_type");
        require(original.replaceWithText(juce::JSON::toString(config)), "Metadata-free full-rig fixture must write");
        AmpSuiteAudioProcessor p(true, folder.root.getChildFile("Library")); neutral(p); p.requestFile(true, original); settle(p);
        require(static_cast<bool>(p.status()["ampHasCab"]), "Original filename must identify the cabinet in a metadata-free full rig");
        set(p, "AMP_SOURCE", 3); const auto rig = p.getRig();
        require(original.deleteFile(), "Only the temporary source capture may be removed");
        require(p.applyRig(rig).isEmpty(), "Managed full-rig capture must recall without its download"); settle(p);
        require(static_cast<bool>(p.status()["ampHasCab"]), "Managed copy must retain its original cabinet hint");
    }
    // Managed copies survive removal of original downloads and are shared across instances.
    {
        Folder folder; const auto root = folder.root.getChildFile("Library"), original = folder.root.getChildFile("Original.nam");
        require(fixture.copyFileTo(original), "Fixture copy must open");
        AmpSuiteAudioProcessor first(true, root), stale(true, root); neutral(first); neutral(stale);
        first.importAssets({original}, "amp");
        for (int i = 0; i < 400 && first.getLibrary()["assets"].size() == 0; ++i) juce::Thread::sleep(10);
        auto catalog = first.getLibrary(); require(catalog["assets"].size() == 1, "Managed batch import must finish");
        const auto id = catalog["assets"][0]["id"].toString(), managed = catalog["assets"][0]["path"].toString();
        require(juce::File(managed).isAChildOf(root) && juce::File(managed).existsAsFile(), "Imported assets must have an independent managed copy");
        const auto renamed = folder.root.getChildFile("Renamed.nam"); require(fixture.copyFileTo(renamed), "Renamed duplicate fixture must write");
        first.importAssets({renamed}, "amp"); bool remembered = false;
        for (int i = 0; i < 400 && !remembered; ++i) {
            const auto aliases = juce::JSON::parse(first.getLibrary()["assets"][0]["aliases"].toString());
            remembered = aliases.isArray() && aliases.getArray()->contains(renamed.getFullPathName()); if (!remembered) juce::Thread::sleep(10);
        }
        require(remembered && first.getLibrary()["assets"].size() == 1, "Managed duplicates must remember every original path without multiplying assets");
        require(first.saveRig("Shared A").isEmpty(), "Shared rig A must save");
        require(stale.saveRig("Shared B").isEmpty(), "Stale instance must add rather than overwrite rigs");
        auto shared = stale.getLibrary(); require(shared["assets"].size() == 1 && shared["rigs"].size() == 2, "Instances must merge shared assets and saved rigs");
        auto changes = std::make_unique<juce::DynamicObject>(); changes->setProperty("name", "Shared favorite"); changes->setProperty("favorite", true);
        require(first.editAsset(id, juce::var(changes.release())), "Shared metadata must save");
        require(stale.saveRig("Shared C").isEmpty(), "Stale rig save must preserve another instance's metadata");
        AmpSuiteAudioProcessor reopened(true, root); require(reopened.getLibrary()["assets"][0]["name"].toString() == "Shared favorite", "Stale writers must not overwrite edited metadata");
        const auto removed = first.getLibrary()["rigs"][0]["id"].toString(); require(first.removeRig(removed), "Shared rig must remove");
        require(stale.saveRig("Shared D").isEmpty(), "Subsequent writes must work after a rig deletion");
        auto after = stale.getLibrary(); for (const auto& rig : *after["rigs"].getArray()) require(rig["id"].toString() != removed, "Deleted shared rigs must not reappear from stale writers");
        require(original.deleteFile(), "Only the temporary original may be removed");
        require(reopened.selectAsset(id), "Managed copy must remain usable after the original is removed"); settle(reopened);
        set(reopened, "AMP_SOURCE", 3); set(reopened, "CAB_MODE", 3); reopened.prepareToPlay(48000, 128); play(reopened);
        const auto cabinet = folder.root.getChildFile("Cabinet.wav");
        { juce::WavAudioFormat format; auto output = cabinet.createOutputStream();
          std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(output.release(), 48000, 2, 16, {}, 0));
          juce::AudioBuffer<float> impulse(2, 96); impulse.clear(); impulse.setSample(0, 0, .8f); impulse.setSample(1, 4, .7f);
          require(writer && writer->writeFromAudioSampleBuffer(impulse, 0, 96), "Temporary cabinet fixture must write"); }
        reopened.requestFile(false, cabinet); settle(reopened); reopened.requestPedal(fixture); settle(reopened);
        set(reopened, "PEDAL_ON", 1); set(reopened, "CAB_MODE", 1); play(reopened);
        const auto portable = folder.root.getChildFile("Portable.cassian.zip"); require(reopened.exportRigPack(portable).isEmpty(), "Portable three-stage rig must export");
        juce::ZipFile exported(portable); require(exported.getNumEntries() == 4, "Portable pack must contain the document and all three stage files");
        AmpSuiteAudioProcessor destination(true, folder.root.getChildFile("OtherLibrary")); neutral(destination);
        require(destination.importRigPack(portable).isEmpty(), "Portable rig must import into separate managed storage");
        auto rigs = destination.getLibrary()["rigs"]; require(rigs.size() == 1 && get(destination, "AMP_SOURCE") == 4, "Pack import must add a rig without changing the active tone");
        require(destination.loadRig(rigs[0]["id"].toString()).isEmpty(), "Portable rig must recall without original paths"); settle(destination);
        require(get(destination, "AMP_SOURCE") == 3 && juce::File(stateOf(destination.getRig())["modelPath"].toString()).isAChildOf(folder.root.getChildFile("OtherLibrary")), "Portable recall must use the destination's asset copy");
        destination.prepareToPlay(48000, 128); play(destination);
        const auto corrupted = folder.root.getChildFile("Corrupted.zip"); juce::ZipFile::Builder damaged;
        for (int i = 0; i < exported.getNumEntries(); ++i) {
            const auto entry = exported.getEntry(i)->filename; const auto file = folder.root.getChildFile("entry" + juce::String(i));
            std::unique_ptr<juce::InputStream> input(exported.createStreamForEntry(i));
            { auto output = file.createOutputStream(); require(input && output && output->writeFromInputStream(*input, -1) > 0, "Pack test entry must write"); }
            if (entry.startsWith("assets/amp/")) require(file.replaceWithText("{}"), "Corrupted fixture must write");
            damaged.addFile(file, 1, entry);
        }
        { auto output = corrupted.createOutputStream(); require(output && damaged.writeToStream(*output, nullptr), "Corrupted pack must write"); }
        require(destination.importRigPack(corrupted).containsIgnoreCase("checksum"), "Tampered pack content must fail hash validation");
        // Reject unexpected archive paths rather than calling general ZIP extraction.
        const auto unsafe = folder.root.getChildFile("Unexpected.zip"), doc = folder.root.getChildFile("rig.json");
        require(doc.replaceWithText(juce::JSON::toString(first.getRig())), "Test pack document must write");
        juce::ZipFile::Builder builder; builder.addFile(doc, 1, "rig.cassian.json"); builder.addFile(fixture, 1, "../escape.nam");
        { auto stream = unsafe.createOutputStream(); require(stream && builder.writeToStream(*stream, nullptr), "Invalid pack fixture must write"); }
        require(!destination.importRigPack(unsafe).isEmpty() && !folder.root.getChildFile("escape.nam").existsAsFile(), "Unexpected pack paths must be rejected without extracting them");
        require(destination.getLibrary()["rigs"].size() == 1, "Rejected packs must not add rigs");
    }
    std::cout << "Audio quality, concurrent loading, effects, shared library, and portable pack checks passed\n";
}
