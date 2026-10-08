#include "LibraryBackup.h"
#include <map>
#include <stdexcept>

namespace {
constexpr juce::int64 maxBytes = BackupZip::archiveLimit;
constexpr int maxFiles = 8192;
void require(bool ok, const juce::String& text) { if (!ok) throw std::runtime_error(text.toStdString()); }
void checkCancel(const std::atomic<bool>& cancelled) { require(!cancelled.load(), "Backup/recovery cancelled. Existing work was preserved."); }
class HashStream final : public juce::BufferedInputStream {
public:
    HashStream(juce::InputStream& source, const std::atomic<bool>& flag) : juce::BufferedInputStream(source,65536), cancelled(flag) {}
    int read(void* data, int bytes) override { checkCancel(cancelled); return juce::BufferedInputStream::read(data,bytes); }
private:
    const std::atomic<bool>& cancelled;
};
juce::String digest(const juce::File& file, const std::atomic<bool>& cancelled) {
    auto source=file.createInputStream(); require(source!=nullptr, "Cannot read a backup checksum source.");
    HashStream input(*source,cancelled); const auto hash=juce::SHA256(input).toHexString();
    require(source->getStatus().wasOk() && input.getPosition()==file.getSize(), "Backup source changed or checksum read failed."); return hash;
}
bool safePath(const juce::String& path) {
    if (path.isEmpty() || path.length() > 240 || path.containsAnyOf("\\:<>\"|?*") || path.startsWithChar('/') || path.endsWithChar('/')) return false;
    for (const auto c : path) if (c < 32 || c == 127) return false;
    juce::StringArray parts; parts.addTokens(path, "/", "");
    for (const auto& part : parts) {
        if (part.isEmpty() || part == "." || part == ".." || part.endsWithChar('.') || part.endsWithChar(' ') || part.containsAnyOf("\r\n\t")) return false;
        const auto base = part.upToFirstOccurrenceOf(".", false, false).toUpperCase();
        if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" || (base.length() == 4 && (base.startsWith("COM") || base.startsWith("LPT")) && base[3] >= '1' && base[3] <= '9')) return false;
    }
    return true;
}
bool payloadPath(const juce::String& name) {
    if (!safePath(name)) return false;
    if (name == "library.xml" || name == "takes.xml") return true;
    juce::StringArray parts; parts.addTokens(name, "/", "");
    if (parts.size() == 3 && parts[0] == "takes") return parts[1].containsOnly("0123456789") && (parts[2].endsWithIgnoreCase(".wav") || parts[2].endsWithIgnoreCase(".json"));
    if (parts.size() != 2) return false;
    const auto hash = parts[1].upToLastOccurrenceOf(".", false, false);
    if (hash.length() != 64 || hash.removeCharacters("0123456789abcdef").isNotEmpty()) return false;
    return parts[0] == "assets" ? (parts[1].endsWith(".nam") || parts[1].endsWith(".wav")) : (parts[0] == "practice" || parts[0] == "take-sections") && parts[1].endsWith(".json");
}
struct Folder {
    explicit Folder(const juce::File& parent) : file(parent.getChildFile(".cassian-recovery-" + juce::Uuid().toString())) {
        require(!file.exists() && file.createDirectory().wasOk(), "Could not create backup/recovery scratch storage.");
    }
    ~Folder() { if (!keep && file.isAChildOf(file.getParentDirectory())) file.deleteRecursively(); }
    juce::File file; bool keep = false;
};
juce::ValueTree readTree(const juce::File& file, const char* type, int limit) {
    if (!file.existsAsFile()) return juce::ValueTree(type);
    require(file.getSize() <= limit, "Catalog is too large to back up or recover.");
    const auto xml = juce::XmlDocument::parse(file);
    require(xml && xml->hasTagName(type), "Cannot read " + file.getFileName() + ". It has not been overwritten.");
    return juce::ValueTree::fromXml(*xml);
}
void writeText(const juce::File& file, const juce::String& text) {
    require(file.getParentDirectory().createDirectory().wasOk() && file.replaceWithText(text), "Could not write recovery metadata.");
}
juce::String key(const juce::String& path) {
   #if JUCE_WINDOWS
    return path.replaceCharacter('\\', '/').toLowerCase();
   #else
    return path;
   #endif
}
using Paths = std::map<juce::String, juce::String>;
// Parse embedded documents rather than replacing substrings in names/notes.
juce::var rewriteValue(const juce::Identifier&, const juce::var&, const Paths&, bool, int);
void rewriteTree(juce::ValueTree tree, const Paths& paths, bool portable, int depth = 0) {
    require(depth <= 64, "Recovery metadata is nested too deeply.");
    tree.removeProperty("aliases", nullptr);
    for (int i = 0; i < tree.getNumProperties(); ++i) {
        const auto property = tree.getPropertyName(i);
        tree.setProperty(property, rewriteValue(property, tree[property], paths, portable, depth + 1), nullptr);
    }
    for (auto child : tree) rewriteTree(child, paths, portable, depth + 1);
}
juce::var rewriteValue(const juce::Identifier& property, const juce::var& value, const Paths& paths, bool portable, int depth) {
    require(depth <= 64, "Recovery metadata is nested too deeply.");
    if (value.isObject()) {
        // JUCE represents JSON null as an object var with no DynamicObject.
        // Empty performance-scene slots are intentionally null.
        if (value.getDynamicObject() == nullptr) return value;
        auto copy = value.clone(); auto* object = copy.getDynamicObject();
        auto& properties = object->getProperties();
        for (int i = 0; i < properties.size(); ++i) {
            const auto name = properties.getName(i);
            object->setProperty(name, rewriteValue(name, properties.getValueAt(i), paths, portable, depth + 1));
        }
        return copy;
    }
    if (value.isArray()) { juce::Array<juce::var> rows; for (const auto& row : *value.getArray()) rows.add(rewriteValue({}, row, paths, portable, depth + 1)); return rows; }
    if (!value.isString()) return value;
    const auto name = property.toString(), text = value.toString();
    if ((name == "path" || name.endsWith("Path")) && text.isNotEmpty()) {
        const auto found = paths.find(key(text));
        require(found != paths.end(), "A referenced file is missing from the backup: " + juce::File::createLegalFileName(text).substring(0, 140));
        if (!portable) require(!juce::File::isAbsolutePath(text) && safePath(text), "Backup contains an external file reference.");
        return found->second;
    }
    if (name == "state" || name == "board" || name == "baseline") {
        const auto xml = juce::XmlDocument::parse(text);
        require(xml != nullptr, "Invalid saved rig/board document in backup.");
        auto tree = juce::ValueTree::fromXml(*xml); rewriteTree(tree, paths, portable, depth + 1); return tree.toXmlString();
    }
    if (name == "json") {
        const auto parsed = juce::JSON::parse(text); require(parsed.isObject() || parsed.isArray(), "Invalid scene metadata in backup.");
        return juce::JSON::toString(rewriteValue({}, parsed, paths, portable, depth + 1));
    }
    return value;
}
struct Item { juce::File file; juce::String name, hash; juce::int64 bytes = 0; };
struct Watch { juce::File file; juce::String hash; };
std::vector<Item> verifyArchive(const juce::File& archive, const juce::File& extract, const std::atomic<bool>& cancelled, LibraryBackup::Progress progress) {
    BackupZip::Reader zip(archive); const auto& entries = zip.entries();
    require(entries.size() >= 3 && entries.size() <= maxFiles + 1, "Invalid backup file count.");
    juce::StringArray names; juce::int64 total = 0;
    for (const auto& entry : entries) {
        const auto name = entry.name;
        require(safePath(name) && !entry.symlink && !names.contains(name, true) && (name == "backup.json" || payloadPath(name)), "Unsafe, duplicate or unexpected backup entry.");
        require(entry.bytes >= 0 && entry.bytes <= maxBytes - total, "Oversized backup entry or expansion exceeds 32 GiB."); total += entry.bytes; names.add(name);
    }
    const int manifestIndex = names.indexOf("backup.json"); require(manifestIndex >= 0 && entries[static_cast<size_t>(manifestIndex)].bytes <= 4 * 1024 * 1024, "Backup has no supported manifest.");
    auto manifestStream = zip.open(manifestIndex); require(manifestStream != nullptr, "Could not read backup manifest.");
    juce::MemoryBlock manifestBytes; const auto manifestSize = entries[static_cast<size_t>(manifestIndex)].bytes;
    require(manifestStream->readIntoMemoryBlock(manifestBytes, static_cast<juce::ssize_t>(manifestSize)) == static_cast<size_t>(manifestSize) && manifestStream->isExhausted(), "Truncated backup manifest.");
    if (zip.isZip64()) require((BackupZip::crcUpdate(0xffffffffu, static_cast<const char*>(manifestBytes.getData()), static_cast<int>(manifestBytes.getSize())) ^ 0xffffffffu) == entries[static_cast<size_t>(manifestIndex)].crc, "Backup manifest CRC failed.");
    const auto manifest = juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(manifestBytes.getData()), static_cast<int>(manifestBytes.getSize())));
    require(manifest.isObject() && manifest["format"].toString() == "Cassian personal backup" && manifest["schema"].isInt() && static_cast<int>(manifest["schema"]) == 1 && manifest["files"].isArray(), "Unsupported Cassian backup format.");
    require(manifest["files"].size() == static_cast<int>(entries.size()) - 1, "Backup manifest does not match its files.");
    std::vector<Item> result; juce::StringArray listed; juce::int64 done = 0;
    std::vector<char> buffer(65536);
    for (const auto& row : *manifest["files"].getArray()) {
        checkCancel(cancelled); const auto name = row["path"].toString(), hash = row["sha256"].toString(); const auto bytes = row["bytes"];
        const int index = names.indexOf(name);
        require(row.isObject() && payloadPath(name) && !listed.contains(name, true) && index >= 0 && (bytes.isInt64() || bytes.isInt()) && static_cast<juce::int64>(bytes) == entries[static_cast<size_t>(index)].bytes && hash.length() == 64 && hash.removeCharacters("0123456789abcdef").isEmpty(), "Invalid backup checksum manifest.");
        listed.add(name); const auto file = extract.getChildFile(name); require(file.isAChildOf(extract) && file.getParentDirectory().createDirectory().wasOk(), "Could not prepare recovery folder.");
        auto input = zip.open(index); auto output = file.createOutputStream(); require(input && output, "Could not read/write recovery file: " + name);
        juce::int64 copied = 0; juce::uint32 crc = 0xffffffffu;
        while (copied < static_cast<juce::int64>(bytes)) {
            checkCancel(cancelled); const int wanted = static_cast<int>(juce::jmin<juce::int64>(buffer.size(), static_cast<juce::int64>(bytes) - copied));
            const int count = input->read(buffer.data(), wanted); require(count == wanted && output->write(buffer.data(), static_cast<size_t>(count)), "Truncated backup or recovery disk write failed.");
            if (zip.isZip64()) crc = BackupZip::crcUpdate(crc, buffer.data(), count);
            copied += count; done += count; if (progress) progress(.8 * static_cast<double>(done) / static_cast<double>(juce::jmax<juce::int64>(1, total)));
        }
        require(input->isExhausted(), "Backup entry exceeds its declared length."); output->flush(); require(output->getStatus().wasOk(), "Recovery disk write failed."); output.reset();
        if (zip.isZip64()) require((crc ^ 0xffffffffu) == entries[static_cast<size_t>(index)].crc, "Backup CRC failed: " + name);
        require(digest(file, cancelled) == hash, "Backup checksum failed: " + name);
        if (name.startsWith("assets/")) require(juce::File(name).getFileNameWithoutExtension() == hash, "Sound filename does not match its content identity.");
        result.push_back({file, name, hash, copied});
    }
    require(listed.contains("library.xml") && listed.contains("takes.xml"), "Backup catalogs are missing."); return result;
}
struct Guard { explicit Guard(juce::InterProcessLock& m) : mutex(m) { require(mutex.enter(3000), "Library is busy; finish other library actions and try again."); } ~Guard() { mutex.exit(); } juce::InterProcessLock& mutex; };
juce::String lockName(const char* prefix, const juce::File& file) { const auto path = file.getFullPathName(); return juce::String(prefix) + juce::SHA256(path.toRawUTF8(), static_cast<size_t>(path.getNumBytesAsUTF8())).toHexString(); }
void replace(const juce::File& file, const juce::String& text) { juce::TemporaryFile temp(file); writeText(temp.getFile(), text); require(temp.overwriteTargetFileWithTemporary(), "Could not commit restored catalog."); }
}

LibraryBackup::Report LibraryBackup::create(const juce::File& root, const juce::File& archive, const juce::var& currentRig, const std::atomic<bool>& cancelled, Progress progress, bool includeTakes, std::optional<juce::StringArray> selectedTakeIds, bool forceZip64)
{
    require(root != juce::File() && archive != juce::File(), "Backups require shared library storage and an output file.");
    require(archive.getParentDirectory().isDirectory(), "Choose an existing backup destination folder.");
    require(!archive.isAChildOf(root), "Save the backup outside the Cassian library.");
    Folder staging(archive.getParentDirectory()); std::vector<Item> items; std::vector<Watch> watches; Paths paths; juce::StringArray names;
    auto watch = [&](const juce::File& file) {
        require(key(file.getFullPathName()) != key(archive.getFullPathName()), "Choose a backup destination that does not replace a sound, recording or catalog.");
        watches.push_back({file, file.existsAsFile() ? digest(file, cancelled) : juce::String()});
    };
    const auto libraryFile = root.getChildFile("library.xml"), takesFile = root.getChildFile("takes.xml"); watch(libraryFile);
    auto library = readTree(libraryFile, "LIBRARY", 32 * 1024 * 1024); juce::ValueTree takes("TAKES");
    if (includeTakes) {watch(takesFile); takes = readTree(takesFile, "TAKES", 8 * 1024 * 1024);}
    if (selectedTakeIds) {
        require(includeTakes && !selectedTakeIds->isEmpty() && selectedTakeIds->size() <= 2048, "Choose at least one recorded take for a selective backup.");
        juce::ValueTree selected("TAKES"); juce::StringArray seen;
        for (const auto& id : *selectedTakeIds) {
            require(id.isNotEmpty() && id.length() <= 128 && !seen.contains(id), "Invalid or duplicate backup take selection."); seen.add(id);
            int matches = 0;
            for (const auto& take : takes) if (take.hasType("TAKE") && take["id"].toString() == id) { selected.addChild(take.createCopy(), -1, nullptr); ++matches; }
            require(matches == 1, "A selected take is missing or has an ambiguous identity. Refresh the take list and choose again.");
        }
        takes = selected;
    }
    require(takes.getNumChildren() <= 2048, "Too many takes to back up.");
    juce::int64 total = 0;
    auto add = [&](const juce::File& file, const juce::String& name, bool observe = true) {
        require(payloadPath(name) && file.existsAsFile() && !file.isSymbolicLink(), "Missing or unsupported backup source: " + file.getFileName());
        require(key(file.getFullPathName()) != key(archive.getFullPathName()), "Choose a backup destination that does not replace a sound, recording or catalog.");
        if (names.contains(name)) return;
        require(items.size() < maxFiles && file.getSize() <= maxBytes, "Backup has too many or oversized files.");
        total += file.getSize(); require(total <= maxBytes - 32 * 1024 * 1024, "Selected content exceeds the 32 GiB backup limit. Choose fewer takes or a tone-library backup.");
        const auto hash = digest(file, cancelled); items.push_back({file, name, hash, file.getSize()}); names.add(name); if (observe) watches.push_back({file, hash});
    };
    // Managed and external library assets are packed by content, not local path.
    for (auto asset : library) if (asset.hasType("ASSET")) {
        const auto path = asset["path"].toString(); require(juce::File::isAbsolutePath(path), "Relink missing library sounds before backing up.");
        const juce::File file(path); require(file.existsAsFile() && file.hasFileExtension("nam;wav"), "Missing sound: " + file.getFileName());
        const auto hash = digest(file, cancelled); require(asset["id"].toString() == asset["kind"].toString() + ":" + hash, "A library sound changed. Relink the original before backing up.");
        const auto name = "assets/" + hash + file.getFileExtension().toLowerCase(); add(file, name); paths[key(path)] = name;
        const auto aliases = juce::JSON::parse(asset["aliases"].toString()); if (aliases.isArray()) for (const auto& alias : *aliases.getArray()) paths[key(alias.toString())] = name;
    }
    // Also cover sounds used only by take/current-rig snapshots (not in catalog).
    std::function<void(juce::ValueTree)> discover = [&](juce::ValueTree tree) {
        if (tree.hasType("ASSET")) {
            const auto path = tree["path"].toString();
            if (path.isNotEmpty() && !paths.contains(key(path))) {
                require(juce::File::isAbsolutePath(path), "A snapshot sound is missing. Relink before backing up."); const juce::File file(path);
                require(file.existsAsFile() && file.hasFileExtension("nam;wav"), "Missing snapshot sound: " + file.getFileName()); const auto hash = digest(file, cancelled);
                require(tree["id"].toString() == tree["kind"].toString() + ":" + hash, "A snapshot sound has changed.");
                const auto name = "assets/" + hash + file.getFileExtension().toLowerCase(); add(file, name); paths[key(path)] = name;
            }
        }
        for (const auto* stage : {"model", "ir", "pedal", "irB", "pedal1", "ambience", "ambience1"}) {
            const auto property = juce::String(stage) + "Path", path = tree[property].toString();
            if (path.isEmpty() || paths.contains(key(path))) continue;
            require(juce::File::isAbsolutePath(path), "A rig sound is missing. Relink before backing up."); const juce::File file(path);
            require(file.existsAsFile() && file.hasFileExtension("nam;wav"), "Missing rig sound: " + file.getFileName());
            const auto hash = digest(file, cancelled), id = tree[juce::String(stage) + "Id"].toString();
            const auto kind = juce::String(stage).startsWith("ambience") ? "ambience" : juce::String(stage) == "model" ? "amp" : (juce::String(stage) == "ir" || juce::String(stage) == "irB") ? "cab" : "pedal";
            require(id.isEmpty() || id == juce::String(kind) + ":" + hash, "A rig sound changed; relink it before backing up.");
            const auto name = "assets/" + hash + file.getFileExtension().toLowerCase(); add(file, name); paths[key(path)] = name;
        }
        for (const auto& child : tree) discover(child);
    };
    auto discoverRig = [&](const juce::var& rig) { require(rig.isObject() && rig["state"].isString(), "Invalid rig snapshot in backup."); const auto xml = juce::XmlDocument::parse(rig["state"].toString()); require(xml != nullptr, "Unreadable rig snapshot."); discover(juce::ValueTree::fromXml(*xml)); };
    for (const auto& rig : library) if (rig.hasType("RIG")) { auto object = std::make_unique<juce::DynamicObject>(); object->setProperty("state", rig["state"]); discoverRig(juce::var(object.release())); }
    struct Document { juce::File file; juce::String name; juce::var value; }; std::vector<Document> documents;
    int takeIndex = 0;
    for (const auto& take : takes) {
        checkCancel(cancelled); const auto path = take["path"].toString(); require(take.hasType("TAKE") && juce::File::isAbsolutePath(path), "Invalid take folder reference."); const juce::File folder(path);
        require(folder.isDirectory() && folder.getChildFile("Guitar dry.wav").existsAsFile() && folder.getChildFile("Guitar processed.wav").existsAsFile(), "Missing recorded audio: " + folder.getFileName());
        const auto prefix = "takes/" + juce::String(takeIndex++); paths[key(path)] = prefix;
        for (const auto& version : take) for (const auto* property : {"path", "rigPath"}) { const auto text = version[property].toString(); require(juce::File::isAbsolutePath(text) && juce::File(text).isAChildOf(folder) && juce::File(text).existsAsFile(), "A reamp version is missing from its take folder."); }
        for (const auto& file : folder.findChildFiles(juce::File::findFiles, false)) {
            if (!file.hasFileExtension("wav;json")) continue;
            const auto name = prefix + "/" + file.getFileName(); require(safePath(name), "Rename an unsupported take filename before backing up."); paths[key(file.getFullPathName())] = name;
            if (file.hasFileExtension("json")) {
                require(file.getSize() <= 4 * 1024 * 1024, "Take metadata is too large."); watch(file); const auto value = juce::JSON::parse(file.loadFileAsString()); require(value.isObject() || value.isArray(), "Cannot read take metadata.");
                if (value.hasProperty("state")) discoverRig(value); documents.push_back({file, name, value});
            } else add(file, name);
        }
    }
    if (currentRig.isObject()) {
        discoverRig(currentRig); auto rig = juce::ValueTree("RIG"); rig.setProperty("id", juce::Uuid().toString(), nullptr); rig.setProperty("name", "Current tone at backup", nullptr); rig.setProperty("schema", currentRig["schema"], nullptr); rig.setProperty("state", currentRig["state"], nullptr); library.addChild(rig, -1, nullptr);
    }
    // Review sections are keyed by whole-file audio hashes. A subset must not
    // include annotations for unrelated takes, even when stored in one folder.
    juce::StringArray selectedAudioHashes;
    if (selectedTakeIds) for (const auto& item : items) if (item.name.startsWith("takes/") && item.file.hasFileExtension("wav")) selectedAudioHashes.addIfNotAlreadyThere(item.hash);
    for (const auto* section : {"practice", "take-sections"}) {
        if (!includeTakes && juce::String(section) == "take-sections") continue;
        for (const auto& file : root.getChildFile(section).findChildFiles(juce::File::findFiles, false, "*.json")) {
            if (selectedTakeIds && juce::String(section) == "take-sections" && !selectedAudioHashes.contains(file.getFileNameWithoutExtension())) continue;
            add(file, juce::String(section) + "/" + file.getFileName());
        }
    }
    for (const auto& document : documents) { const auto file = staging.file.getChildFile(document.name); writeText(file, juce::JSON::toString(rewriteValue({}, document.value, paths, true, 0))); add(file, document.name, false); }
    rewriteTree(library, paths, true); rewriteTree(takes, paths, true);
    const auto portableLibrary = staging.file.getChildFile("library.xml"), portableTakes = staging.file.getChildFile("takes.xml");
    writeText(portableLibrary, library.toXmlString()); writeText(portableTakes, takes.toXmlString()); add(portableLibrary, "library.xml", false); add(portableTakes, "takes.xml", false);
    juce::Array<juce::var> rows; for (const auto& item : items) { auto row = std::make_unique<juce::DynamicObject>(); row->setProperty("path", item.name); row->setProperty("bytes", item.bytes); row->setProperty("sha256", item.hash); rows.add(juce::var(row.release())); }
    auto manifest = std::make_unique<juce::DynamicObject>(); manifest->setProperty("format", "Cassian personal backup"); manifest->setProperty("schema", 1); manifest->setProperty("scope", selectedTakeIds ? "selected-takes" : includeTakes ? "complete" : "tone-library"); manifest->setProperty("takeCount", takes.getNumChildren()); manifest->setProperty("appVersion", JucePlugin_VersionString); manifest->setProperty("created", juce::Time::getCurrentTime().toISO8601(true)); manifest->setProperty("files", rows);
    const auto manifestFile = staging.file.getChildFile("backup.json"); writeText(manifestFile, juce::JSON::toString(juce::var(manifest.release()))); items.push_back({manifestFile, "backup.json", digest(manifestFile, cancelled), manifestFile.getSize()});
    std::vector<BackupZip::Source> sources; for (const auto& item : items) sources.push_back({item.file,item.name,item.bytes});
    checkCancel(cancelled); juce::TemporaryFile temporary(archive); BackupZip::write(temporary.getFile(), sources, cancelled, progress, forceZip64);
    Folder verification(archive.getParentDirectory()); verifyArchive(temporary.getFile(), verification.file, cancelled, [&](double p) {if (progress) progress(.75 + .2 * p);});
    for (const auto& observed : watches) { checkCancel(cancelled); require((observed.file.existsAsFile() ? digest(observed.file, cancelled) : juce::String()) == observed.hash, "Library or recording changed during backup. Finish edits/recording and try again."); }
    require(temporary.overwriteTargetFileWithTemporary(), "Could not replace backup output; previous backup was preserved."); if (progress) progress(1);
    int rigs = 0; for (const auto& row : library) if (row.hasType("RIG")) ++rigs;
    return {archive, static_cast<int>(items.size() - 1), rigs, takes.getNumChildren(), total};
}

LibraryBackup::Report LibraryBackup::restore(const juce::File& archive, const juce::File& root, const std::atomic<bool>& cancelled, Validator validator, Progress progress)
{
    require(root != juce::File() && root.createDirectory().wasOk() && validator != nullptr, "Recovery requires shared storage and rig validation.");
    Folder staging(root); const auto files = verifyArchive(archive, staging.file, cancelled, progress); Paths paths;
    const auto recovered = root.getChildFile("Recovered").getChildFile(juce::Time::getCurrentTime().formatted("%Y-%m-%d-%H-%M-%S-") + juce::Uuid().toString());
    juce::int64 bytes = 0;
    for (const auto& item : files) { paths[key(item.name)] = recovered.getChildFile(item.name).getFullPathName(); bytes += item.bytes;
        if (item.name.startsWith("takes/")) { const auto parent = item.name.upToLastOccurrenceOf("/", false, false); paths[key(parent)] = recovered.getChildFile(parent).getFullPathName(); } }
    auto library = readTree(staging.file.getChildFile("library.xml"), "LIBRARY", 32 * 1024 * 1024), takes = readTree(staging.file.getChildFile("takes.xml"), "TAKES", 8 * 1024 * 1024);
    require(takes.getNumChildren() <= 2048, "Recovery has too many takes."); rewriteTree(library, paths, false); rewriteTree(takes, paths, false);
    int rigs = 0;
    for (auto entry : library) {
        require(entry.hasType("ASSET") || entry.hasType("RIG"), "Unexpected library metadata in backup.");
        if (entry.hasType("RIG")) {
            const auto schema = entry["schema"].toString(); require(schema == "1" || schema == "2" || schema == "3", "Unsupported recovered rig schema.");
            auto doc = std::make_unique<juce::DynamicObject>(); doc->setProperty("schema", schema.getIntValue()); doc->setProperty("state", entry["state"]);
            const auto error = validator(juce::var(doc.release())); require(error.isEmpty(), "Cannot recover rig: " + error);
            entry.setProperty("id", juce::Uuid().toString(), nullptr); entry.setProperty("name", (entry["name"].toString().substring(0, 68) + " (recovered)").substring(0, 80), nullptr); ++rigs;
        } else {
            const auto kind = entry["kind"].toString(), id = entry["id"].toString(), path = entry["path"].toString();
            require(kind == "amp" || kind == "pedal" || kind == "cab" || kind == "ambience", "Unsupported recovered sound kind.");
            const auto relative = juce::File(path).getRelativePathFrom(recovered).replaceCharacter('\\', '/');
            require(paths.contains(key(relative)) && id == kind + ":" + digest(staging.file.getChildFile(relative), cancelled), "Recovered sound identity does not match its checksum.");
            entry.setProperty("managed", true, nullptr); entry.setProperty("ownership", "User", nullptr);
        }
    }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    for (auto take : takes) {
        require(take.hasType("TAKE"), "Invalid recovered take catalog."); take.setProperty("id", juce::Uuid().toString(), nullptr);
        take.setProperty("name", (take["name"].toString().substring(0, 68) + " (recovered)").substring(0, 80), nullptr);
        const auto folder = staging.file.getChildFile(juce::File(take["path"].toString()).getRelativePathFrom(recovered));
        require(folder.isAChildOf(staging.file) && folder.getChildFile("Guitar dry.wav").existsAsFile() && folder.getChildFile("Guitar processed.wav").existsAsFile(), "Recovered take is missing its original audio.");
        std::unique_ptr<juce::AudioFormatReader> dry(formats.createReaderFor(folder.getChildFile("Guitar dry.wav"))), wet(formats.createReaderFor(folder.getChildFile("Guitar processed.wav")));
        require(dry && wet && dry->numChannels == 1 && wet->numChannels == 2 && dry->lengthInSamples > 0 && dry->lengthInSamples == wet->lengthInSamples && dry->sampleRate == wet->sampleRate && dry->sampleRate >= 8000 && dry->sampleRate <= 384000, "Recovered take has invalid or mismatched original audio.");
        take.setProperty("frames", dry->lengthInSamples, nullptr); take.setProperty("sampleRate", dry->sampleRate, nullptr);
    }
    for (const auto& item : files) if (item.name.startsWith("takes/") && item.file.hasFileExtension("json")) {
        require(item.bytes <= 4 * 1024 * 1024, "Recovered take metadata is too large."); const auto doc = juce::JSON::parse(item.file.loadFileAsString()); require(doc.isObject() || doc.isArray(), "Invalid recovered take metadata.");
        auto rewritten = rewriteValue({}, doc, paths, false, 0); if (rewritten.hasProperty("state")) { const auto error = validator(rewritten); require(error.isEmpty(), "Invalid recovered take rig: " + error); }
        writeText(item.file, juce::JSON::toString(rewritten));
    }
    checkCancel(cancelled);
    const auto libraryFile = root.getChildFile("library.xml"), takesFile = root.getChildFile("takes.xml");
    juce::InterProcessLock libraryLock(lockName("CassianLibrary-", root)), takeLock(lockName("CassianTakes-", takesFile)); Guard first(libraryLock), second(takeLock);
    auto current = readTree(libraryFile, "LIBRARY", 32 * 1024 * 1024), currentTakes = readTree(takesFile, "TAKES", 8 * 1024 * 1024);
    const auto before = current.toXmlString(), beforeTakes = currentTakes.toXmlString(); const bool hadLibrary = libraryFile.existsAsFile();
    // Existing metadata wins on sound-ID collisions; a missing original path
    // gains the recovered copy. Saved rigs/takes always receive fresh IDs.
    juce::StringArray removedIds; removedIds.addTokens(current["removedIds"].toString(), ",", "");
    for (const auto& entry : library) {
        // Explicit recovery restores these sound identities. Keep unrelated
        // deletions, but do not let the next normal save delete a recovered sound.
        if (entry.hasType("ASSET")) removedIds.removeString(entry["id"].toString());
        auto existing = current.getChildWithProperty("id", entry["id"]);
        if (!existing.isValid()) current.addChild(entry.createCopy(), -1, nullptr);
        else if (entry.hasType("ASSET") && (!juce::File::isAbsolutePath(existing["path"].toString()) || !juce::File(existing["path"].toString()).existsAsFile())) existing.setProperty("path", entry["path"], nullptr);
    }
    current.setProperty("removedIds", removedIds.joinIntoString(","), nullptr);
    for (const auto& take : takes) currentTakes.addChild(take.createCopy(), -1, nullptr);
    require(current.toXmlString().getNumBytesAsUTF8() <= 32 * 1024 * 1024 && currentTakes.toXmlString().getNumBytesAsUTF8() <= 8 * 1024 * 1024 && currentTakes.getNumChildren() <= 2048, "Recovery would exceed the library/catalog capacity.");
    writeText(staging.file.getChildFile("library.xml"), library.toXmlString()); writeText(staging.file.getChildFile("takes.xml"), takes.toXmlString());
    writeText(staging.file.getChildFile("Before restore library.xml"), before);
    writeText(staging.file.getChildFile("Before restore takes.xml"), beforeTakes);
    checkCancel(cancelled);
    require(recovered.getParentDirectory().createDirectory().wasOk() && !recovered.exists() && staging.file.moveFileTo(recovered), "Could not publish recovered media."); staging.keep = true;
    bool keepMediaOnFailure = false;
    // Retain a pre-restore catalog journal with the recovered media. This also
    // permits manual recovery if power is lost between the two atomic writes.
    try {
        replace(libraryFile, current.toXmlString());
        try { replace(takesFile, currentTakes.toXmlString()); }
        catch (...) {
            try { if (hadLibrary) replace(libraryFile, before); else require(libraryFile.deleteFile(), "Could not roll back library catalog."); }
            catch (...) { keepMediaOnFailure = true; throw std::runtime_error(("Catalog rollback failed. Recovered media and before-restore catalogs remain at " + recovered.getFullPathName()).toStdString()); }
            throw;
        }
    } catch (...) { if (!keepMediaOnFailure) recovered.deleteRecursively(); throw; }
    // Practice/review sections are keyed by audio content. Never replace a
    // user's existing ranges; keep conflicts in the recovered archive copy.
    int sectionWarnings = 0;
    for (const auto& item : files) if (item.name.startsWith("practice/") || item.name.startsWith("take-sections/")) {
        const auto target = root.getChildFile(item.name), sectionRoot = target.getParentDirectory();
        const auto sectionPath = sectionRoot.getFullPathName().toLowerCase();
        juce::InterProcessLock mutex("CassianSections-" + juce::SHA256(sectionPath.toRawUTF8(), static_cast<size_t>(sectionPath.getNumBytesAsUTF8())).toHexString() + target.getFileNameWithoutExtension());
        try {
            Guard guard(mutex); if (target.exists()) continue;
            require(sectionRoot.createDirectory().wasOk(), "Cannot create section folder."); juce::TemporaryFile temporary(target);
            require(recovered.getChildFile(item.name).copyFileTo(temporary.getFile()) && temporary.overwriteTargetFileWithTemporary(), "Cannot recover sections.");
        } catch (const std::exception&) { ++sectionWarnings; }
    }
    if (progress) progress(1);
    const auto warning = sectionWarnings > 0 ? juce::String(sectionWarnings) + " section files could not be added automatically. Their verified copies remain in the recovered folder." : juce::String();
    return {recovered, static_cast<int>(files.size()), rigs, takes.getNumChildren(), bytes, warning};
}
