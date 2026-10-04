#include "PracticeSections.h"
#include <cmath>
#include <stdexcept>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
struct Guard {
    explicit Guard(juce::InterProcessLock& value) : lock(value) { require(lock.enter(3000), "Practice sections are busy; try again."); }
    ~Guard() { lock.exit(); }
    juce::InterProcessLock& lock;
};
bool number(const juce::var& v) { return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite(static_cast<double>(v)); }
juce::String lockName(const juce::File& root, const juce::String& key) {
    const auto path = root.getFullPathName().toLowerCase();
    return "CassianSections-" + juce::SHA256(path.toRawUTF8(), static_cast<size_t>(path.getNumBytesAsUTF8())).toHexString() + key;
}
}
void PracticeSections::validateKey(const juce::String& key)
{ require(key.length() == 64 && key.removeCharacters("0123456789abcdef").isEmpty(), "Invalid backing-track identity."); }
void PracticeSections::validate(const juce::var& rows, double seconds)
{
    require(rows.isArray() && rows.size() <= 32, "Invalid practice section list; it has not been overwritten.");
    juce::StringArray ids;
    for (const auto& row : *rows.getArray()) {
        const auto id = row["id"].toString(), name = row["name"].toString();
        require(row.isObject() && row["id"].isString() && id.isNotEmpty() && id.length() <= 64 && !ids.contains(id)
            && row["name"].isString() && name.trim().isNotEmpty() && name.length() <= 48 && number(row["a"]) && number(row["b"]), "Invalid practice section; it has not been overwritten.");
        const double a = row["a"], b = row["b"];
        // Resampling can round the endpoint up by a sample at a different rate.
        require(a >= 0 && b - a >= .05 - 1e-9 && b <= seconds + .001, "Saved practice section is outside this track; it has not been overwritten."); ids.add(id);
    }
}
juce::var PracticeSections::read(const juce::String& key, double seconds)
{
    if (root == juce::File()) {
        auto found = memory.find(key);
        const auto rows = found == memory.end() ? juce::var(juce::Array<juce::var>()) : found->second;
        validate(rows, seconds); return rows;
    }
    const auto file = root.getChildFile(key + ".json");
    if (!file.existsAsFile()) return juce::var(juce::Array<juce::var>());
    require(file.getSize() <= 65536, "Practice section document is too large; it has not been overwritten.");
    const auto data = juce::JSON::parse(file.loadFileAsString());
    require(data.isObject() && data["version"].isInt() && static_cast<int>(data["version"]) == 1 && data["track"].isString() && data["track"].toString() == key, "Practice section document could not be read; it has not been overwritten.");
    validate(data["sections"], seconds); return data["sections"];
}
juce::var PracticeSections::load(const juce::String& key, double seconds)
{
    const juce::ScopedLock guard(local); validateKey(key);
    juce::InterProcessLock mutex(lockName(root, key)); Guard shared(mutex); return read(key, seconds);
}
juce::var PracticeSections::change(const juce::String& key, double seconds, const juce::String& id, const juce::String& name, double a, double b, bool remove)
{
    const juce::ScopedLock guard(local); validateKey(key);
    juce::InterProcessLock mutex(lockName(root, key)); Guard shared(mutex);
    auto old = read(key, seconds); juce::Array<juce::var> rows = *old.getArray();
    int index = -1; for (int i = 0; i < rows.size(); ++i) if (rows[i]["id"].toString() == id) index = i;
    require(id.isEmpty() ? !remove : index >= 0, "That practice section no longer exists. Reload the track.");
    if (remove) rows.remove(index);
    else {
        require(index >= 0 || rows.size() < 32, "This track already has 32 sections. Replace or delete one first.");
        auto row = std::make_unique<juce::DynamicObject>(); row->setProperty("id", id.isEmpty() ? juce::Uuid().toString() : id); row->setProperty("name", name.trim()); row->setProperty("a", a); row->setProperty("b", b);
        if (index >= 0) rows.set(index, juce::var(row.release())); else rows.add(juce::var(row.release()));
    }
    const juce::var result(rows); validate(result, seconds);
    if (root == juce::File()) { require(memory.contains(key) || memory.size() < 64, "Temporary practice library is full."); memory[key] = result; return result; }
    require(root.createDirectory().wasOk(), "Could not create practice section storage.");
    auto data = std::make_unique<juce::DynamicObject>(); data->setProperty("version", 1); data->setProperty("track", key); data->setProperty("sections", result);
    juce::TemporaryFile temporary(root.getChildFile(key + ".json"));
    require(temporary.getFile().replaceWithText(juce::JSON::toString(juce::var(data.release()))) && temporary.overwriteTargetFileWithTemporary(), "Could not save practice sections.");
    return result;
}
