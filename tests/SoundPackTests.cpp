#include "../Source/PluginProcessor.h"
#include "../Source/SoundPack.h"
#include <iostream>
namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void zip(const juce::File& target, const juce::File& source, const juce::StringArray& entries) { juce::ZipFile::Builder builder; for (const auto& name : entries) builder.addFile(source, 6, name); auto output = target.createOutputStream(); require(output != nullptr && builder.writeToStream(*output, nullptr), "Test ZIP must write"); }
}
void runSoundPackChecks(const juce::File& fixture)
{
    juce::TemporaryFile nam(".nam"), pack(".zip");
    auto config = juce::JSON::parse(fixture.loadFileAsString());
    auto metadata = std::make_unique<juce::DynamicObject>(); metadata->setProperty("gear_type", "pedal"); metadata->setProperty("gear_model", "Example test pedal");
    config.getDynamicObject()->setProperty("metadata", juce::var(metadata.release())); require(nam.getFile().replaceWithText(juce::JSON::toString(config)), "Test NAM must write");
    zip(pack.getFile(), nam.getFile(), {"nested/drive.nam", "docs/readme.txt"});
    int imported = 0; const auto result = SoundPack::read(pack.getFile(), [&](auto asset) { require(asset["kind"].toString() == "pedal" && asset["name"].toString() == "drive", "ZIP import must preserve type and original name"); ++imported; });
    require(imported == 1 && result.imported == 1 && result.skipped == 1 && result.errors.isEmpty(), "Supported entries must import and inert documents must skip");
    for (const auto& name : {"../outside.nam", "/outside.nam", "C:/outside.nam", "nested\\..\\outside.nam"}) {
        juce::TemporaryFile malicious(".zip"); zip(malicious.getFile(), nam.getFile(), {name});
        const auto rejected = SoundPack::read(malicious.getFile(), [&](auto) { require(false, "Unsafe ZIP cannot import any entries"); });
        require(rejected.imported == 0 && !rejected.errors.isEmpty(), "Unsafe paths must reject before writes");
    }
    juce::TemporaryFile duplicate(".zip"); zip(duplicate.getFile(), nam.getFile(), {"drive.nam", "drive.nam"});
    require(!SoundPack::read(duplicate.getFile(), [&](auto) { require(false, "Duplicate entries must reject before imports"); }).errors.isEmpty(), "Duplicate ZIP names must reject");
    // A real stereo response checks wet routing and preservation of recorded time.
    juce::TemporaryFile wav(".wav");
    { juce::WavAudioFormat format; auto stream = wav.getFile().createOutputStream(); std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 2, 24, {}, 0));
      require(writer != nullptr, "Test response writer must open"); juce::AudioBuffer<float> response(2, 96000); response.clear(); response.setSample(0, 2400, .5f); response.setSample(1, 4800, .25f); require(writer->writeFromAudioSampleBuffer(response, 0, response.getNumSamples()), "Response must write"); }
    juce::TemporaryFile ambiencePack(".zip"); zip(ambiencePack.getFile(), wav.getFile(), {"Hall.wav"});
    require(SoundPack::read(ambiencePack.getFile(), [&](auto asset) { require(asset["kind"].toString() == "ambience", "Long WAV must import as ambience instead of cabinet"); }).imported == 1, "Ambience ZIP must import");
    AmpSuiteAudioProcessor p(false); p.prepareToPlay(48000, 128); auto state = p.apvts.copyState(); state.removeChild(state.getChildWithName("PEDALBOARD"), nullptr);
    auto board = PedalboardState::emptySerial(); juce::ValueTree row("BLOCK"); row.setProperty("id", "ambience.test", nullptr); row.setProperty("type", "ambience", nullptr); row.setProperty("automationSlot", 0, nullptr); row.setProperty("lane", "post", nullptr); row.setProperty("deleted", 0, nullptr); board.addChild(row, -1, nullptr); state.addChild(board, -1, nullptr); state.setProperty("ambiencePath", wav.getFile().getFullPathName(), nullptr);
    for (const auto* id : {"BOARD_AMBIENCE_0_AMBIENCE_MIX", "BOARD_AMBIENCE_0_ON"}) { const float value = juce::String(id).endsWith("_ON") ? 1.f : 100.f; auto* param = p.apvts.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(value)); state.getChildWithProperty("id", id).setProperty("value", value, nullptr); }
    SerialPedalboard graph(state, p.apvts, {48000, 128, 2}); juce::AudioBuffer<float> audio(2, 128); NamWrapper* captures[] {nullptr, nullptr}; double left = 0, right = 0;
    for (int block = 0; block < 80; ++block) { audio.clear(); if (block == 0) { audio.setSample(0, 0, 1); audio.setSample(1, 0, 1); } graph.process(audio, false, 120, captures);
        for (int i = 0; i < 128; ++i) { if (block * 128 + i == 2400) left = audio.getSample(0, i); if (block * 128 + i == 4800) right = audio.getSample(1, i); } }
    require(std::abs(left - .5) < .001 && std::abs(right - .25) < .001, "Ambience must preserve stereo channels, gain and captured timing without trim/normalization");

    // Optional local inventory audit validates supplied packs without putting
    // third-party files into fixtures, source control or release packages.
    const auto manifest = juce::SystemStats::getEnvironmentVariable("CASSIAN_SOUND_PACK_MANIFEST", "");
    if (manifest.isNotEmpty()) {
        const auto paths = juce::JSON::parse(juce::File(manifest).loadFileAsString()); require(paths.isArray(), "Sound pack audit manifest must be an array");
        int total = 0; for (const auto& path : *paths.getArray()) { const auto checked = SoundPack::read(juce::File(path.toString()), [&](auto asset) { require(asset["rights"].toString().contains("Unverified"), "User pack rights remain unverified"); });
            std::cout << "Pack " << juce::File(path.toString()).getFileName() << ": " << checked.imported << " supported files, " << checked.errors.size() << " errors\n"; for (const auto& failure : checked.errors) std::cout << failure << '\n'; require(checked.errors.isEmpty(), "Supplied sound pack must validate"); total += checked.imported; }
        std::cout << "Supplied pack files validated: " << total << '\n';
    }
    std::cout << "Sound ZIP classification, path/duplicate rejection and stereo ambience timing passed\n";
}
