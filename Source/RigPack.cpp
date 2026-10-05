#include "PluginProcessor.h"

namespace {
juce::String assetKind(const juce::String& stage) { return stage == "model" ? "amp" : (stage == "ir" || stage == "irB") ? "cab" : "pedal"; }
juce::String entryName(const juce::String& id, const juce::String& kind) {
    const auto hash = id.fromFirstOccurrenceOf(":", false, false);
    if (!id.startsWith(kind + ":") || hash.length() != 64 || hash.removeCharacters("0123456789abcdef").isNotEmpty())
        throw std::runtime_error("Invalid rig asset identity");
    return "assets/" + kind + "/" + hash + (kind == "cab" ? ".wav" : ".nam");
}
}

juce::String AmpSuiteAudioProcessor::exportRigPack(const juce::File& destination, const juce::var& snapshot)
{
    try {
        const auto rig = snapshot.isVoid() ? getRig() : snapshot;
        if (rig.hasProperty("error")) return rig["error"].toString();
        juce::ValueTree state;
        if (const auto failure = migrateRigDocument(rig, state); failure.isNotEmpty()) return failure;
        juce::ZipFile::Builder builder; juce::StringArray ids;
        for (const auto* label : {"model", "ir", "pedal", "irB"}) {
            const juce::String stage(label), path = state[stage + "Path"].toString();
            if (path.isEmpty()) continue;
            const auto id = state[stage + "Id"].toString(), kind = assetKind(stage), entry = entryName(id, kind);
            const juce::File file(path);
            if (!file.existsAsFile() || file.getSize() > 64 * 1024 * 1024 || id != kind + ":" + juce::SHA256(file).toHexString())
                return "Rig asset is missing, changed, or too large to package";
            if (!ids.contains(id)) { builder.addFile(file, 6, entry); ids.add(id); } state.setProperty(stage + "Path", entry, nullptr);
        }
        auto libraryTree = state.getChildWithName("LIBRARY");
        for (int i = libraryTree.getNumChildren(); --i >= 0;) {
            auto item = libraryTree.getChild(i);
            if (!ids.contains(item["id"].toString())) libraryTree.removeChild(i, nullptr);
            else {
                item.setProperty("path", entryName(item["id"].toString(), item["kind"].toString()), nullptr);
                item.removeProperty("aliases", nullptr);
            }
        }
        auto object = std::make_unique<juce::DynamicObject>(); object->setProperty("schema", 2); object->setProperty("state", state.createXml()->toString());
        juce::TemporaryFile document(".json");
        if (!document.getFile().replaceWithText(juce::JSON::toString(juce::var(object.release())))) return "Could not write the pack document";
        builder.addFile(document.getFile(), 6, "rig.cassian.json");
        juce::TemporaryFile temporary(destination);
        {
            auto output = temporary.getFile().createOutputStream();
            if (!output || !builder.writeToStream(*output, nullptr) || output->getStatus().failed()) return "Could not write the rig pack";
            output->flush(); if (output->getStatus().failed()) return "Could not finish the rig pack";
        }
        return temporary.overwriteTargetFileWithTemporary() ? juce::String() : "Could not replace the rig pack file";
    } catch (const std::exception& e) { return e.what(); }
}

juce::String AmpSuiteAudioProcessor::importRigPack(const juce::File& source)
{
    try {
        if (!sharedStore.enabled()) return "Portable packs require managed library storage";
        if (!source.existsAsFile() || source.getSize() > 272 * 1024 * 1024) return "Rig pack is missing or too large";
        juce::ZipFile archive(source); const int count = archive.getNumEntries();
        if (count < 1 || count > 5) return "Choose a Cassian rig pack containing one rig and up to four assets";
        juce::StringArray entries; juce::int64 total = 0;
        for (int i = 0; i < count; ++i) {
            const auto* entry = archive.getEntry(i);
            if (entries.contains(entry->filename) || entry->uncompressedSize < 0 || entry->uncompressedSize > 64 * 1024 * 1024)
                return "Invalid or oversized rig pack entry";
            total += entry->uncompressedSize; entries.add(entry->filename);
        }
        if (total > 260 * 1024 * 1024) return "Expanded rig pack is too large";
        const int docIndex = entries.indexOf("rig.cassian.json");
        if (docIndex < 0 || archive.getEntry(docIndex)->uncompressedSize > 4 * 1024 * 1024) return "Rig pack has no valid document";
        std::unique_ptr<juce::InputStream> document(archive.createStreamForEntry(docIndex));
        if (!document) return "Could not read the rig pack document";
        const auto rig = juce::JSON::parse(document->readEntireStreamAsString()); juce::ValueTree state;
        // Parameters, scenes and the bounded board are validated and migrated
        // together before any pack asset is written into managed storage.
        if (const auto failure = migrateRigDocument(rig, state); failure.isNotEmpty()) return failure;
        juce::StringArray expected {"rig.cassian.json"};
        auto catalog = state.getChildWithName("LIBRARY"); if (!catalog.isValid()) { catalog = juce::ValueTree("LIBRARY"); state.addChild(catalog, -1, nullptr); }
        for (const auto* label : {"model", "ir", "pedal", "irB"}) {
            const juce::String stage(label); const auto path = state[stage + "Path"].toString(); if (path.isEmpty()) continue;
            const auto id = state[stage + "Id"].toString(), kind = assetKind(stage), name = entryName(id, kind);
            const int index = entries.indexOf(name);
            if (path != name || index < 0) return "Rig pack asset reference is missing or invalid";
            expected.addIfNotAlreadyThere(name);
        }
        if (expected.size() != entries.size()) return "Unexpected files in the rig pack";
        for (const auto* label : {"model", "ir", "pedal", "irB"}) {
            const juce::String stage(label); if (state[stage + "Path"].toString().isEmpty()) continue;
            const auto id = state[stage + "Id"].toString(), kind = assetKind(stage), name = entryName(id, kind);
            const auto target = sharedStore.root().getChildFile(name);
            if (target.getParentDirectory().createDirectory().failed()) return "Could not create managed pack storage";
            juce::TemporaryFile temporary(target);
            std::unique_ptr<juce::InputStream> input(archive.createStreamForEntry(entries.indexOf(name)));
            {
                auto output = temporary.getFile().createOutputStream();
                if (!input || !output || output->writeFromInputStream(*input, archive.getEntry(entries.indexOf(name))->uncompressedSize) != archive.getEntry(entries.indexOf(name))->uncompressedSize)
                    return "Could not extract the rig pack asset";
            }
            if (id != kind + ":" + juce::SHA256(temporary.getFile()).toHexString()) return "Rig pack asset checksum failed";
            // Confirm it is a supported capture/IR before committing the file.
            if (kind != "cab") { NamWrapper check(temporary.getFile()); }
            else {
                juce::AudioFormatManager formats; formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(temporary.getFile().createInputStream()));
                if (!reader || reader->numChannels == 0 || reader->numChannels > 2 || reader->lengthInSamples <= 0 || reader->lengthInSamples > reader->sampleRate * 10)
                    return "Rig pack contains an unsupported IR";
            }
            if (target.existsAsFile()) { if (juce::SHA256(target).toHexString() != id.fromFirstOccurrenceOf(":", false, false)) return "Managed asset has changed; relink it first"; }
            else if (!temporary.overwriteTargetFileWithTemporary()) return "Could not store the rig pack asset";
            state.setProperty(stage + "Path", target.getFullPathName(), nullptr);
            auto asset = catalog.getChildWithProperty("id", id);
            if (!asset.isValid()) { asset = AssetLibrary::describe(target, kind); catalog.addChild(asset, -1, nullptr); }
            AssetLibrary::rememberPath(asset, target.getFullPathName()); asset.setProperty("managed", true, nullptr);
        }
        auto object = std::make_unique<juce::DynamicObject>(); object->setProperty("schema", 2); object->setProperty("state", state.createXml()->toString());
        return importRig(source.getFileNameWithoutExtension().replace(".cassian", ""), juce::var(object.release()));
    } catch (const std::exception& e) { return e.what(); }
}

void AmpSuiteAudioProcessor::requestRigPack(bool save, const juce::File& file)
{
    const auto snapshot = save ? getRig() : juce::var();
    const juce::ScopedLock lock(requestLock);
    if (snapshot.hasProperty("error")) { message = "Load failed: " + snapshot["error"].toString(); return; }
    pendingPacks.push_back({file, save, snapshot}); message = save ? "Packing rig..." : "Importing rig pack..."; notify();
}
