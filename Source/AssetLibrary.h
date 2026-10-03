#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

// Called on the asset loader/control threads, never in the audio callback.
class AssetLibrary
{
public:
    juce::ValueTree tree {"LIBRARY"};
    int revision = 0;

    static juce::ValueTree describe(const juce::File& file, const juce::String& kind)
    {
        juce::ValueTree asset("ASSET");
        asset.setProperty("id", kind + ":" + juce::SHA256(file).toHexString(), nullptr);
        asset.setProperty("kind", kind, nullptr);
        asset.setProperty("path", file.getFullPathName(), nullptr);
        asset.setProperty("name", file.getFileNameWithoutExtension(), nullptr);
        asset.setProperty("ownership", "User", nullptr);
        asset.setProperty("rights", "Unverified — not cleared for factory redistribution", nullptr);
        asset.setProperty("favorite", false, nullptr);
        if (file.hasFileExtension("nam"))
        {
            const auto config = juce::JSON::parse(file.loadFileAsString());
            const auto metadata = config["metadata"];
            asset.setProperty("sampleRate", config["sample_rate"], nullptr);
            asset.setProperty("creator", metadata["modeled_by"].toString(), nullptr);
            asset.setProperty("gear", metadata["gear_make"].toString() + " " + metadata["gear_model"].toString(), nullptr);
            asset.setProperty("inputLevelDbu", metadata["input_level_dbu"], nullptr);
            asset.setProperty("metadata", juce::JSON::toString(metadata), nullptr);
            const auto gear = metadata["gear_type"].toString().toLowerCase();
            const auto name = file.getFileNameWithoutExtension().toLowerCase().removeCharacters(" -_");
            const int mode = gear.contains("cab") || gear == "studio" || name.contains("fullrig") ? 3
                           : gear.contains("preamp") ? 2 : gear == "amp" ? 1 : 0;
            asset.setProperty("captureKind", mode, nullptr);
        }
        else
        {
            juce::AudioFormatManager formats; formats.registerBasicFormats();
            if (std::unique_ptr<juce::AudioFormatReader> reader {formats.createReaderFor(file)})
            {
                asset.setProperty("sampleRate", reader->sampleRate, nullptr);
                asset.setProperty("channels", static_cast<int>(reader->numChannels), nullptr);
                asset.setProperty("duration", reader->lengthInSamples / reader->sampleRate, nullptr);
            }
        }
        return asset;
    }

    juce::ValueTree find(const juce::String& id) const { return tree.getChildWithProperty("id", id); }
    static bool exists(const juce::String& path) { return juce::File::isAbsolutePath(path) && juce::File(path).existsAsFile(); }
    static void rememberPath(juce::ValueTree asset, const juce::String& path)
    {
        juce::Array<juce::var> aliases;
        if (const auto previous = juce::JSON::parse(asset["aliases"].toString()); previous.isArray()) aliases = *previous.getArray();
        const auto old = asset["path"].toString();
        if (old.isNotEmpty() && !aliases.contains(old)) aliases.add(old);
        if (path.isNotEmpty() && !aliases.contains(path)) aliases.add(path);
        asset.setProperty("aliases", juce::JSON::toString(aliases, true), nullptr);
        asset.setProperty("path", path, nullptr);
    }
    juce::String idForPath(const juce::String& kind, const juce::String& path) const
    {
        for (const auto& asset : tree)
            if (asset.hasType("ASSET") && asset["kind"].toString() == kind)
            {
                if (asset["path"].toString() == path) return asset["id"].toString();
                const auto aliases = juce::JSON::parse(asset["aliases"].toString());
                if (aliases.isArray() && aliases.getArray()->contains(path)) return asset["id"].toString();
            }
        return {};
    }
    void upsert(const juce::ValueTree& asset)
    {
        auto existing = find(asset["id"].toString());
        if (existing.isValid()) rememberPath(existing, asset["path"].toString());
        else tree.addChild(asset.createCopy(), -1, nullptr);
        ++revision;
    }
    void merge(const juce::ValueTree& incoming)
    {
        if (!incoming.hasType("LIBRARY")) return;
        for (const auto& child : incoming)
        {
            if (!child.hasType("ASSET") && !child.hasType("RIG")) continue;
            if (child["id"].toString().isEmpty()) continue;
            if (child.hasType("ASSET")) {
                const auto kind = child["kind"].toString(), id = child["id"].toString();
                if ((kind != "amp" && kind != "pedal" && kind != "cab") || !id.startsWith(kind + ":")
                    || id.length() != kind.length() + 65
                    || id.substring(kind.length() + 1).removeCharacters("0123456789abcdef").isNotEmpty()) continue;
            }
            if (!find(child["id"].toString()).isValid()) {
                auto copy = child.createCopy();
                if (copy.hasType("ASSET")) { copy.setProperty("ownership", "User", nullptr); copy.setProperty("rights", "Unverified — not cleared for factory redistribution", nullptr); }
                tree.addChild(copy, -1, nullptr);
            }
            else if (child.hasType("ASSET"))
            {
                auto current = find(child["id"].toString());
                if (!exists(current["path"].toString()) && exists(child["path"].toString())) rememberPath(current, child["path"].toString());
            }
        }
        ++revision;
    }
    juce::var list() const
    {
        juce::Array<juce::var> assets, rigs;
        for (const auto& child : tree)
        {
            auto row = std::make_unique<juce::DynamicObject>();
            for (int i = 0; i < child.getNumProperties(); ++i)
            {
                const auto key = child.getPropertyName(i);
                if (key != juce::Identifier("state")) row->setProperty(key, child[key]);
            }
            if (child.hasType("ASSET"))
            {
                row->setProperty("missing", !exists(child["path"].toString()));
                assets.add(juce::var(row.release()));
            }
            else if (child.hasType("RIG")) rigs.add(juce::var(row.release()));
        }
        auto result = std::make_unique<juce::DynamicObject>();
        result->setProperty("assets", assets); result->setProperty("rigs", rigs); result->setProperty("revision", revision);
        return juce::var(result.release());
    }
};
