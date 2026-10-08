#pragma once
#include "LibraryStore.h"
#include <vector>

// A portable, content-addressed bank. It contains no settings, recordings or
// owner-specific paths. Only explicit packaged banks are imported at startup.
namespace BundledSoundBank {
inline int install(const juce::File& folder, LibraryStore& store, AssetLibrary& library) {
    if (folder == juce::File()) return 0;
    const auto manifest = folder.getChildFile("manifest.json");
    if (!manifest.existsAsFile()) return 0;
    if (manifest.getSize() > 2 * 1024 * 1024) throw std::runtime_error("Sound bank manifest is too large");
    const auto document = juce::JSON::parse(manifest.loadFileAsString());
    if (static_cast<int>(document["schema"]) != 1 || !document["assets"].isArray() || document["assets"].size() > 1024)
        throw std::runtime_error("Invalid sound bank manifest");
    std::vector<juce::ValueTree> incoming; juce::StringArray seen;
    for (const auto& entry : *document["assets"].getArray()) {
        const auto id = entry["id"].toString(), kind = entry["kind"].toString();
        const auto hash = id.fromFirstOccurrenceOf(":", false, false);
        if ((kind != "amp" && kind != "pedal" && kind != "cab" && kind != "ambience") || id != kind + ":" + hash
            || hash.length() != 64 || hash.removeCharacters("0123456789abcdef").isNotEmpty() || seen.contains(id))
            throw std::runtime_error("Invalid or duplicate sound bank identity");
        seen.add(id);
        const auto file = folder.getChildFile("assets").getChildFile(kind).getChildFile(hash + ((kind == "amp" || kind == "pedal") ? ".nam" : ".wav"));
        if (!file.existsAsFile() || file.getSize() > 64 * 1024 * 1024 || juce::SHA256(file).toHexString() != hash)
            throw std::runtime_error("Missing or changed sound bank file: " + entry["name"].toString().toStdString());
        auto existing = library.find(id);
        if (existing.isValid() && AssetLibrary::exists(existing["path"].toString())) continue;
        auto asset = AssetLibrary::describe(file, kind);
        for (const auto* key : {"name", "sourceName", "creator", "pack", "notes", "styles", "gain", "speaker", "captureKind"})
            if (entry.hasProperty(key)) asset.setProperty(key, entry[key], nullptr);
        if (asset["sourceName"].toString().isEmpty()) asset.setProperty("sourceName", entry["name"], nullptr);
        incoming.push_back(asset);
    }
    // Verify the whole bank before writing any files or library metadata.
    for (auto asset : incoming) { store.manage(asset); asset.setProperty("aliases", "[]", nullptr); library.upsert(asset); }
    if (!incoming.empty()) store.save(library.tree);
    return static_cast<int>(incoming.size());
}
inline juce::File location() {
    // Validator processes must not import the owner's installed sound bank.
    if (juce::SystemStats::getEnvironmentVariable("CASSIAN_VALIDATION_ROOT", {}).isNotEmpty()) return {};
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const auto beside = executable.getParentDirectory().getChildFile("Sounds");
    if (beside.getChildFile("manifest.json").existsAsFile()) return beside;
    // The standalone installer and VST3 share a per-user installed bank.
   #if JUCE_WINDOWS
    const auto local = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", "");
    if (juce::File::isAbsolutePath(local)) return juce::File(local).getChildFile("Programs/Cassian/Sounds");
   #endif
    return {};
}
}
