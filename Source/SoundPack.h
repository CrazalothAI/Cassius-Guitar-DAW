#pragma once
#include "AssetLibrary.h"
#include "dsp/NamWrapper.h"

// Archives are read as bounded streams into flat temporary files. Entry paths
// never become filesystem destinations. Called only on control/loader threads.
class SoundPack {
public:
    struct Result { int imported = 0, skipped = 0; juce::StringArray errors; };
    template<typename Import> static Result read(const juce::File& file, Import import) {
        Result result;
        try {
            if (!file.existsAsFile() || file.getSize() > 512 * 1024 * 1024) throw std::runtime_error("Sound pack is missing or too large");
            juce::ZipFile archive(file); const int count = archive.getNumEntries(); juce::int64 total = 0;
            if (count < 1 || count > 1024) throw std::runtime_error("Sound pack must contain 1–1024 entries");
            // Check the entire expansion budget before importing any entry.
            juce::StringArray names;
            for (int i = 0; i < count; ++i) {
                const auto* entry = archive.getEntry(i); const auto name = entry->filename.replaceCharacter('\\', '/');
                juce::StringArray parts; parts.addTokens(name, "/", "");
                if (name.startsWithChar('/') || name.containsChar(':') || parts.contains("..") || names.contains(name)
                    || entry->uncompressedSize < 0 || entry->uncompressedSize > 64 * 1024 * 1024)
                    throw std::runtime_error("Invalid or oversized sound pack entry");
                names.add(name); total += entry->uncompressedSize;
                if (total > 512 * 1024 * 1024) throw std::runtime_error("Expanded sound pack is too large");
            }
            juce::AudioFormatManager formats; formats.registerBasicFormats();
            for (int i = 0; i < count; ++i) {
                const auto* entry = archive.getEntry(i); const auto name = names[i];
                const bool nam = name.endsWithIgnoreCase(".nam"), wav = name.endsWithIgnoreCase(".wav");
                if (!nam && !wav) { ++result.skipped; continue; }
                try {
                    juce::TemporaryFile temporary(nam ? ".nam" : ".wav");
                    std::unique_ptr<juce::InputStream> input(archive.createStreamForEntry(i));
                    { auto output = temporary.getFile().createOutputStream();
                      if (!input || !output || output->writeFromInputStream(*input, entry->uncompressedSize) != entry->uncompressedSize || output->getStatus().failed())
                          throw std::runtime_error("Could not read archive entry"); }
                    const auto sourceName = name.fromLastOccurrenceOf("/", false, false).dropLastCharacters(4);
                    juce::String kind, reason;
                    if (nam) {
                        NamWrapper validation(temporary.getFile(), sourceName);
                        const auto metadata = juce::JSON::parse(temporary.getFile().loadFileAsString())["metadata"];
                        const auto gear = metadata["gear_type"].toString().toLowerCase();
                        const bool echo = file.getFileName().containsIgnoreCase("Space Echo");
                        if (gear == "pedal" || (gear == "studio" && echo)) kind = "pedal";
                        else if (gear == "amp" || gear == "amp_cab" || gear.contains("preamp")) kind = "amp";
                        else throw std::runtime_error("Unclear capture type; extract and import as amp or pedal explicitly");
                        reason = "Classified from NAM gear_type: " + gear;
                        if (echo) reason += ". Static preamp/circuit capture; use Cassian delay for adjustable repeats.";
                    } else {
                        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(temporary.getFile()));
                        if (!reader || reader->numChannels < 1 || reader->numChannels > 2 || reader->sampleRate <= 0 || reader->lengthInSamples <= 0 || reader->lengthInSamples > reader->sampleRate * 30)
                            throw std::runtime_error("Choose a mono/stereo response up to 30 seconds");
                        const auto hint = (file.getFileName() + " " + name).toLowerCase();
                        const bool ambience = reader->lengthInSamples > reader->sampleRate || hint.contains("reverb") || hint.contains("ambient") || hint.contains("shoegaze") || hint.contains("flint") || hint.contains("rv-6");
                        kind = ambience ? "ambience" : "cab";
                        reason = "Classified from pack name and response length. " + juce::String(ambience ? "Recorded ambience; time/decay are captured." : "Cabinet response.");
                    }
                    auto asset = AssetLibrary::describe(temporary.getFile(), kind);
                    asset.setProperty("name", sourceName, nullptr); asset.setProperty("sourceName", sourceName, nullptr);
                    asset.setProperty("pack", file.getFileName(), nullptr); asset.setProperty("notes", reason, nullptr);
                    // describe() cannot use the randomized temporary filename to
                    // infer a cabinet. Metadata remains the classification source.
                    import(asset); ++result.imported;
                } catch (const std::exception& e) { result.errors.add(name + ": " + juce::String(e.what())); }
            }
        } catch (const std::exception& e) { result.errors.add(e.what()); }
        return result;
    }
};
