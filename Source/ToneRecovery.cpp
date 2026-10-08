#include "ToneRecovery.h"
#include <stdexcept>

namespace {
constexpr int maxDocument = 4 * 1024 * 1024, retained = 64;
void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
juce::String hash(const juce::String& text) { return juce::SHA256(text.toRawUTF8(), static_cast<size_t>(text.getNumBytesAsUTF8())).toHexString(); }
bool validId(const juce::String& id) { return id.startsWith("tone-") && id.endsWith(".json") && id.length() == 66 && id.substring(25,61).containsOnly("0123456789abcdef-") && id.substring(5,13).containsOnly("0123456789") && id[13] == '-' && id.substring(14,20).containsOnly("0123456789") && id[20] == '-' && id.substring(21,24).containsOnly("0123456789") && id[24] == '-'; }
struct Guard { explicit Guard(juce::InterProcessLock& value) : mutex(value) {require(mutex.enter(3000), "Tone recovery storage is busy. Try again.");} ~Guard() {mutex.exit();} juce::InterProcessLock& mutex; };
juce::var document(const juce::File& file) {
    require(file.existsAsFile() && !file.isSymbolicLink() && file.getSize() > 0 && file.getSize() <= maxDocument, "Tone snapshot is missing or too large.");
    const auto doc = juce::JSON::parse(file.loadFileAsString());
    require(doc.getDynamicObject() && doc["format"].toString() == "Cassian tone snapshot" && doc["version"].isInt() && static_cast<int>(doc["version"]) == 1 && doc["state"].isString() && doc["schema"].isInt() && doc["created"].isString() && doc["name"].isString() && doc["sha256"].toString() == hash(doc["state"].toString()), "Tone snapshot is damaged or unsupported.");
    return doc;
}
void replace(const juce::File& file, const juce::String& text) {
    juce::TemporaryFile temporary(file);
    require(temporary.getFile().replaceWithText(text) && temporary.overwriteTargetFileWithTemporary(), "Could not save tone recovery data. Previous snapshots remain available.");
}
}
ToneRecovery::ToneRecovery(juce::File root) : Thread("Cassian tone recovery"), folder(root.getChildFile("recovery-tones")), shared("CassianToneRecovery-" + hash(folder.getFullPathName())) { startThread(); }
ToneRecovery::~ToneRecovery() { signalThreadShouldExit(); notify(); stopThread(-1); }
juce::String ToneRecovery::capture(const juce::var& rig, const juce::String& name) {
    if (!rig.getDynamicObject() || !rig["schema"].isInt() || !rig["state"].isString() || rig.hasProperty("error")) return "Finish loading the tone before making a recovery snapshot.";
    const juce::ScopedLock guard(lock);
    if (pending.load()) return "Tone recovery storage is busy. Try again.";
    auto copy = std::make_unique<juce::DynamicObject>(); copy->setProperty("schema", rig["schema"]); copy->setProperty("state", rig["state"]);
    queuedRig = juce::var(copy.release()); queuedName = name.isEmpty() ? "Unsaved tone" : name.substring(0,80);
    pending.store(true); error.clear(); notify(); return {};
}
juce::String ToneRecovery::setAutomatic(bool enabled) {
    const juce::ScopedLock guard(lock);
    if (pending.load()) return "Tone recovery storage is busy. Try again.";
    automaticEnabled.store(enabled); writePreferences = true; pending.store(true); error.clear(); notify(); return {};
}
juce::var ToneRecovery::status() {
    const juce::ScopedLock guard(lock); auto row = std::make_unique<juce::DynamicObject>();
    row->setProperty("available", true); row->setProperty("automatic", automaticEnabled.load()); row->setProperty("busy", pending.load()); row->setProperty("error", error); row->setProperty("snapshots", entries.clone()); return juce::var(row.release());
}
juce::var ToneRecovery::read(const juce::String& id) {
    require(validId(id), "Choose an available tone snapshot."); Guard guard(shared); return document(folder.getChildFile(id));
}
void ToneRecovery::scan() {
    auto files = folder.findChildFiles(juce::File::findFiles, false, "tone-*.json");
    for (int i = files.size(); --i >= 0;) if (!validId(files[i].getFileName()) || files[i].isSymbolicLink()) files.remove(i);
    files.sort(); juce::Array<juce::var> rows;
    int bad = 0;
    // Invalid documents never displace valid snapshots from the retained history.
    for (int i = files.size(); --i >= 0;) {
        try {
            const auto doc = document(files[i]);
            if (rows.size() >= retained) { require(files[i].deleteFile(), "Could not prune an old tone snapshot."); continue; }
            auto row = std::make_unique<juce::DynamicObject>(); row->setProperty("id", files[i].getFileName()); row->setProperty("name", doc["name"]); row->setProperty("created", doc["created"]); rows.add(juce::var(row.release()));
        } catch (const std::exception&) { ++bad; }
    }
    const juce::ScopedLock guard(lock); entries = rows;
    if (bad > 0) error = juce::String(bad) + " damaged/unreadable snapshots or retention failures. Valid snapshots are still available.";
}
void ToneRecovery::run() {
    try {
        require(folder.createDirectory().wasOk(), "Could not create tone recovery storage."); Guard guard(shared);
        const auto preferences = folder.getChildFile("preferences.json");
        if (preferences.existsAsFile()) {
            require(preferences.getSize() <= 4096, "Tone recovery preferences are damaged."); const auto value = juce::JSON::parse(preferences.loadFileAsString());
            require(value["automatic"].isBool(), "Tone recovery preferences are damaged."); automaticEnabled.store(static_cast<bool>(value["automatic"]));
        }
        scan();
    } catch (const std::exception& e) { const juce::ScopedLock guard(lock); error = e.what(); }
    pending.store(false);
    while (true) {
        juce::var rig; juce::String name; bool preferences = false;
        { const juce::ScopedLock guard(lock); rig = queuedRig; queuedRig = juce::var(); name = queuedName; preferences = writePreferences; writePreferences = false; }
        if (!rig.isObject() && !preferences) { if (threadShouldExit()) break; wait(250); continue; }
        try {
            require(folder.createDirectory().wasOk(), "Could not create tone recovery storage."); Guard guard(shared);
            if (preferences) replace(folder.getChildFile("preferences.json"), automaticEnabled.load() ? "{\"automatic\":true}" : "{\"automatic\":false}");
            if (rig.isObject()) {
                const auto digest = hash(rig["state"].toString());
                bool unchanged = false;
                if (digest == lastHash) try { unchanged = document(lastFile)["sha256"].toString() == digest; } catch (const std::exception&) {}
                if (!unchanged) {
                    const auto now = juce::Time::getCurrentTime(); auto* object = rig.getDynamicObject();
                    object->setProperty("format", "Cassian tone snapshot"); object->setProperty("version", 1); object->setProperty("name", name); object->setProperty("created", now.toISO8601(true)); object->setProperty("sha256", digest);
                    const auto text = juce::JSON::toString(rig); require(text.getNumBytesAsUTF8() <= maxDocument, "Tone snapshot exceeds the 4 MiB recovery limit. Save a complete personal backup.");
                    const auto file = folder.getChildFile("tone-" + now.formatted("%Y%m%d-%H%M%S-") + juce::String(now.toMilliseconds() % 1000).paddedLeft('0', 3) + "-" + juce::Uuid().toDashedString() + ".json");
                    replace(file, text); document(file); lastHash = digest; lastFile = file;
                }
            }
            scan();
        } catch (const std::exception& e) { const juce::ScopedLock guard(lock); error = e.what(); }
        pending.store(false);
    }
}
