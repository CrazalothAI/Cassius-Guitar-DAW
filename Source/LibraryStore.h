#pragma once
#include "AssetLibrary.h"
#include <stdexcept>

// Shared, reference-preserving storage. All methods run on control/loader threads.
// Writers merge under an interprocess lock and atomically replace the manifest.
class LibraryStore
{
public:
    static juce::File defaultRoot() {
        // Opt-in scratch storage for host validation; ordinary installs keep
        // their existing library path. Invalid test configuration fails closed.
        const auto validationRoot = juce::SystemStats::getEnvironmentVariable("CASSIAN_VALIDATION_ROOT", {});
        if (validationRoot.isNotEmpty()) {
            if (!juce::File::isAbsolutePath(validationRoot)) throw std::runtime_error("Validation storage must use an absolute path.");
            return juce::File(validationRoot);
        }
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Cassian/Library");
    }
    explicit LibraryStore(juce::File folder = {}) : directory(std::move(folder)), mutex("CassianLibrary-" + juce::SHA256(directory.getFullPathName().toRawUTF8(), static_cast<size_t>(directory.getFullPathName().getNumBytesAsUTF8())).toHexString()) {}
    bool enabled() const { return directory != juce::File(); }
    juce::File root() const { return directory; }
    juce::ValueTree load()
    {
        const juce::ScopedLock threadLock(localMutex);
        if (!enabled()) return {};
        Guard guard(mutex); const auto file = directory.getChildFile("library.xml");
        if (!file.existsAsFile()) return {};
        const auto digest = juce::SHA256(file).toHexString();
        if (digest == lastDigest) return {};
        auto tree = read(file); lastDigest = digest; lastView = tree.createCopy(); return tree;
    }
    void save(const juce::ValueTree& local, const juce::StringArray& removed = {})
    {
        const juce::ScopedLock threadLock(localMutex);
        if (!enabled()) return;
        Guard guard(mutex);
        if (auto result = directory.createDirectory(); result.failed()) throw std::runtime_error(result.getErrorMessage().toStdString());
        const auto file = directory.getChildFile("library.xml");
        auto tree = file.existsAsFile() ? read(file) : juce::ValueTree("LIBRARY");
        juce::StringArray tombstones; tombstones.addTokens(tree["removedIds"].toString(), ",", ""); tombstones.addArray(removed); tombstones.removeDuplicates(false);
        for (const auto& id : tombstones) { const auto child = tree.getChildWithProperty("id", id); if (child.isValid()) tree.removeChild(child, nullptr); }
        tree.setProperty("removedIds", tombstones.joinIntoString(","), nullptr);
        for (const auto& child : local) {
            const auto id = child["id"].toString(); if (id.isEmpty()) continue;
            if (tombstones.contains(id) || child.isEquivalentTo(lastView.getChildWithProperty("id", id))) continue;
            const auto existing = tree.getChildWithProperty("id", id);
            if (existing.isValid()) tree.removeChild(existing, nullptr);
            tree.addChild(child.createCopy(), -1, nullptr);
        }
        juce::TemporaryFile temporary(file);
        if (!temporary.getFile().replaceWithText(tree.createXml()->toString()) || !temporary.overwriteTargetFileWithTemporary())
            throw std::runtime_error("Could not save the shared library");
        lastDigest.clear(); lastView = local.createCopy();
    }
    void manage(juce::ValueTree asset)
    {
        const juce::ScopedLock threadLock(localMutex);
        if (!enabled()) return;
        const auto source = juce::File(asset["path"].toString());
        const auto id = asset["id"].toString(), kind = asset["kind"].toString();
        const auto hash = id.fromFirstOccurrenceOf(":", false, false);
        if ((kind != "amp" && kind != "pedal" && kind != "cab" && kind != "ambience") || hash.length() != 64 || hash.removeCharacters("0123456789abcdef").isNotEmpty())
            throw std::runtime_error("Invalid managed asset identity");
        Guard guard(mutex);
        const auto folder = directory.getChildFile("assets").getChildFile(kind);
        if (folder.createDirectory().failed()) throw std::runtime_error("Could not create managed asset storage");
        const auto target = folder.getChildFile(hash + ((kind == "cab" || kind == "ambience") ? ".wav" : ".nam"));
        if (target.existsAsFile()) {
            if (juce::SHA256(target).toHexString() != hash) throw std::runtime_error("Managed asset content has changed; relink the original file");
        } else {
            juce::TemporaryFile temporary(target);
            if (!source.copyFileTo(temporary.getFile()) || juce::SHA256(temporary.getFile()).toHexString() != hash || !temporary.overwriteTargetFileWithTemporary())
                throw std::runtime_error("Could not copy the asset into the shared library");
        }
        AssetLibrary::rememberPath(asset, target.getFullPathName());
        asset.setProperty("managed", true, nullptr);
    }
private:
    struct Guard {
        explicit Guard(juce::InterProcessLock& m) : mutex(m) { if (!mutex.enter(3000)) throw std::runtime_error("Shared library is busy; try again"); }
        ~Guard() { mutex.exit(); }
        juce::InterProcessLock& mutex;
    };
    static juce::ValueTree read(const juce::File& file) {
        if (file.getSize() > 32 * 1024 * 1024) throw std::runtime_error("Shared library manifest is too large");
        auto xml = juce::XmlDocument::parse(file);
        if (!xml || !xml->hasTagName("LIBRARY")) throw std::runtime_error("Shared library manifest could not be read; it has not been overwritten");
        return juce::ValueTree::fromXml(*xml);
    }
    juce::File directory;
    juce::CriticalSection localMutex;
    juce::InterProcessLock mutex;
    juce::String lastDigest;
    juce::ValueTree lastView;
};
