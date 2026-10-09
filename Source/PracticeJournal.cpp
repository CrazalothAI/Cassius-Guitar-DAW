#include "PracticeJournal.h"
#include <cmath>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
juce::var object(std::initializer_list<std::pair<juce::Identifier, juce::var>> values) {
    auto result = std::make_unique<juce::DynamicObject>();
    for (const auto& value : values) result->setProperty(value.first, value.second);
    return juce::var(result.release());
}
bool number(const juce::var& value, double lo, double hi) {
    return (value.isInt() || value.isInt64() || value.isDouble()) && std::isfinite(static_cast<double>(value)) && static_cast<double>(value) >= lo && static_cast<double>(value) <= hi;
}
bool text(const juce::var& value, int limit, bool empty = false) {
    if (!value.isString() || value.toString().length() > limit || (!empty && value.toString().trim().isEmpty())) return false;
    for (const auto c : value.toString()) if ((c < 32 && c != '\n' && c != '\t') || c == 127) return false;
    return true;
}
void keys(const juce::var& row, const juce::String& allowed) {
    require(row.getDynamicObject() != nullptr, "Invalid practice journal row.");
    const auto& properties = row.getDynamicObject()->getProperties();
    for (int i = 0; i < properties.size(); ++i) require(allowed.contains("|" + properties.getName(i).toString() + "|"), "Unknown practice journal field.");
}
void task(const juce::var& row) {
    keys(row, "|title||minutes||bpm|");
    require(text(row["title"], 80) && number(row["minutes"], 1, 120) && number(row["bpm"], 40, 240), "Use a task name, 1–120 minutes and 40–240 BPM.");
    require(static_cast<double>(row["minutes"]) == std::floor(static_cast<double>(row["minutes"])) && static_cast<double>(row["bpm"]) == std::floor(static_cast<double>(row["bpm"])), "Practice targets must be whole numbers.");
}
bool identity(const juce::var& value) {
    if (!text(value, 64)) return false;
    for (const auto c : value.toString()) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}
void recording(const juce::var& row) {
    keys(row, "|takeId||version|");
    require(identity(row["takeId"]) && identity(row["version"]), "Choose a valid take and version identity; file paths are not recording links.");
}
juce::var emptyDocument() { return object({{"schema", 2}, {"sets", juce::Array<juce::var>()}, {"sessions", juce::Array<juce::var>()}}); }
int find(const juce::var& rows, const juce::String& id) {
    for (int i = 0; i < rows.size(); ++i) if (rows[i]["id"].toString() == id) return i;
    return -1;
}
bool equivalent(const juce::var& a, const juce::var& b, bool set) {
    // Identity conflicts concern values, not JSON property order or int/double
    // spelling. Both rows have already passed the same strict schema reader.
    if (set) {
        if (a["name"].toString() != b["name"].toString() || a["tasks"].size() != b["tasks"].size()) return false;
        for (int i = 0; i < a["tasks"].size(); ++i) {
            const auto& x = a["tasks"][i]; const auto& y = b["tasks"][i];
            if (x["title"].toString() != y["title"].toString() || static_cast<double>(x["minutes"]) != static_cast<double>(y["minutes"]) || static_cast<double>(x["bpm"]) != static_cast<double>(y["bpm"])) return false;
        }
    } else {
        for (const auto* key : {"title", "setName", "started", "state", "notes"}) if (a[key].toString() != b[key].toString()) return false;
        for (const auto* key : {"minutes", "bpm", "seconds"}) if (static_cast<double>(a[key]) != static_cast<double>(b[key])) return false;
        if (a["recordings"].size() != b["recordings"].size()) return false;
        // Recording links are a set. Reordering exported JSON is not a conflict.
        for (const auto& link : *a["recordings"].getArray()) {
            bool matched = false;
            for (const auto& other : *b["recordings"].getArray()) if (link["takeId"].toString() == other["takeId"].toString() && link["version"].toString() == other["version"].toString()) matched = true;
            if (!matched) return false;
        }
    }
    return true;
}
double elapsed(const juce::var& row, double at, double since, bool playing) {
    return juce::jlimit(0., 21600., static_cast<double>(row["seconds"]) + (playing ? std::max(0., at - since) : 0.));
}
}

PracticeJournal::PracticeJournal(juce::File document, std::function<double()> clock)
    : Thread("Cassian practice journal"), file(std::move(document)), now(std::move(clock)), view(emptyDocument()) { startThread(); }
PracticeJournal::~PracticeJournal() { signalThreadShouldExit(); notify(); stopThread(-1); }
void PracticeJournal::validate(const juce::var& doc) {
    keys(doc, "|schema||sets||sessions|");
    const auto schema = static_cast<int>(doc["schema"]);
    require(doc["schema"].isInt() && (schema == 1 || schema == 2) && doc["sets"].isArray() && doc["sets"].size() <= 32 && doc["sessions"].isArray() && doc["sessions"].size() <= 256, "Unsupported or oversized practice journal. Existing data was preserved.");
    for (const auto* category : {"sets", "sessions"}) {
        juce::StringArray ids;
        for (const auto& row : *doc[category].getArray()) {
            const auto id = row["id"].toString();
            require(text(row["id"], 64) && !ids.contains(id), "Invalid or duplicate practice identity."); ids.add(id);
            if (juce::String(category) == "sets") {
                keys(row, "|id||name||tasks|");
                require(text(row["name"], 48) && row["tasks"].isArray() && row["tasks"].size() >= 1 && row["tasks"].size() <= 8, "A practice set needs a name and 1–8 tasks.");
                for (const auto& item : *row["tasks"].getArray()) task(item);
            } else {
                keys(row, schema == 1 ? "|id||title||minutes||bpm||setName||started||seconds||state||notes|" : "|id||title||minutes||bpm||setName||started||seconds||state||notes||recordings|");
                task(object({{"title", row["title"]}, {"minutes", row["minutes"]}, {"bpm", row["bpm"]}}));
                require(text(row["setName"], 48, true) && text(row["started"], 40) && juce::Time::fromISO8601(row["started"].toString()).toMilliseconds() > 0 && number(row["seconds"], 0, 21600) && text(row["notes"], 1000, true), "Invalid practice session metadata.");
                const auto state = row["state"].toString(); require(state == "running" || state == "paused" || state == "finished" || state == "interrupted", "Invalid practice session state.");
                if (schema == 2) {
                    require(row["recordings"].isArray() && row["recordings"].size() <= 8, "A session can link up to eight recording versions.");
                    juce::StringArray links;
                    for (const auto& link : *row["recordings"].getArray()) {
                        recording(link); const auto key = link["takeId"].toString() + ":" + link["version"].toString();
                        require(!links.contains(key), "Duplicate practice recording link."); links.add(key);
                    }
                }
            }
        }
    }
    require(juce::JSON::toString(doc).getNumBytesAsUTF8() <= 512 * 1024, "Practice journal exceeds 512 KiB. Export it and remove old sessions first.");
}
juce::var PracticeJournal::readDocument(const juce::File& source) {
    require(source.existsAsFile() && !source.isSymbolicLink() && source.getSize() <= 512 * 1024, "Practice journal is missing, linked or too large.");
    auto input = source.createInputStream(); require(input != nullptr, "Cannot read practice journal.");
    juce::MemoryBlock bytes; input->readIntoMemoryBlock(bytes, 512 * 1024 + 1);
    require(bytes.getSize() <= 512 * 1024 && input->isExhausted() && input->getStatus().wasOk(), "Practice journal read failed.");
    const auto content = juce::String::createStringFromData(bytes.getData(), static_cast<int>(bytes.getSize()));
    int depth = 0; bool quoted = false, escaped = false;
    for (const auto c : content) {
        if (quoted) { if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == '"') quoted = false; }
        else if (c == '"') quoted = true;
        else if (c == '[' || c == '{') require(++depth <= 12, "Practice journal nesting is too deep.");
        else if (c == ']' || c == '}') require(--depth >= 0, "Invalid practice journal nesting.");
    }
    require(depth == 0 && !quoted, "Practice journal is incomplete.");
    auto doc = juce::JSON::parse(content); validate(doc);
    if (static_cast<int>(doc["schema"]) == 1) {
        doc.getDynamicObject()->setProperty("schema", 2);
        for (auto& row : *doc["sessions"].getArray()) row.getDynamicObject()->setProperty("recordings", juce::Array<juce::var>());
        validate(doc);
    }
    return doc;
}
void PracticeJournal::write(const juce::File& destination, const juce::var& doc) {
    validate(doc); require(!destination.isSymbolicLink() && destination.getParentDirectory().createDirectory().wasOk(), "Cannot create practice journal storage.");
    juce::TemporaryFile temporary(destination);
    require(temporary.getFile().replaceWithText(juce::JSON::toString(doc)) && temporary.overwriteTargetFileWithTemporary(), "Cannot save practice journal. Previous data was preserved.");
}
juce::String PracticeJournal::command(const juce::String& action, const juce::var& args, juce::File transfer) {
    const juce::ScopedLock guard(mutex);
    if (!writable) return "Practice journal is unavailable. Close other Cassian windows or check its storage error.";
    if (busy) return "Practice journal is saving. Try again shortly.";
    if (juce::JSON::toString(args).getNumBytesAsUTF8() > 8192) return "Practice request is too large.";
    pendingAction = action; pendingArguments = args.clone(); pendingTransfer = std::move(transfer); error.clear(); busy = true; notify(); return {};
}
void PracticeJournal::publish(const juce::var& doc, const juce::String& active, double since, bool playing) {
    const juce::ScopedLock guard(mutex); view = doc.clone(); activeId = active; started = since; running = playing; ++revision;
}
juce::var PracticeJournal::document() { const juce::ScopedLock guard(mutex); return view.clone(); }
juce::var PracticeJournal::status() {
    const juce::ScopedLock guard(mutex);
    juce::var active; const auto index = find(view["sessions"], activeId);
    if (index >= 0) { active = view["sessions"][index].clone(); active.getDynamicObject()->setProperty("seconds", elapsed(active, now(), started, running)); }
    return object({{"available", true}, {"writable", writable}, {"busy", busy}, {"revision", revision}, {"error", error}, {"active", active}, {"setCount", view["sets"].size()}, {"sessionCount", view["sessions"].size()}});
}
void PracticeJournal::run() {
    const auto path = file.getFullPathName().toLowerCase();
    juce::InterProcessLock lease("CassianPracticeJournal-" + juce::SHA256(path.toRawUTF8(), static_cast<size_t>(path.getNumBytesAsUTF8())).toHexString());
    if (!lease.enter(0)) { const juce::ScopedLock guard(mutex); error = "Another Cassian window owns this practice journal. Close it and reopen Cassian."; busy = false; return; }
    struct Lease { juce::InterProcessLock& lock; ~Lease() { lock.exit(); } } held {lease};
    auto doc = emptyDocument(); juce::String active; double since = now(), checkpoint = since; bool ticking = false;
    auto digest = [&] { return file.existsAsFile() ? juce::SHA256(file).toHexString() : juce::String(); };
    auto expected = digest();
    auto persist = [&](const juce::var& value) {
        require(digest() == expected, "Practice journal changed outside Cassian. Close and reopen the app; the changed document was preserved.");
        write(file, value); expected = digest();
    };
    try {
        if (file.existsAsFile()) doc = readDocument(file);
        require(digest() == expected, "Practice journal changed while loading. Reopen Cassian.");
        bool changed = false;
        for (auto& row : *doc["sessions"].getArray()) if (row["state"].toString() == "running" || row["state"].toString() == "paused") { row.getDynamicObject()->setProperty("state", "interrupted"); changed = true; }
        if (changed) persist(doc);
        publish(doc, active, since, ticking);
        const juce::ScopedLock guard(mutex); writable = true; busy = false;
    } catch (const std::exception& e) { const juce::ScopedLock guard(mutex); error = e.what(); busy = false; return; }
    while (true) {
        juce::String action; juce::var args; juce::File transfer;
        const auto at = now(); const bool checkpointDue = ticking && at - checkpoint >= 30;
        { const juce::ScopedLock guard(mutex);
            if (threadShouldExit() && pendingAction.isEmpty()) break;
            action = pendingAction; pendingAction.clear(); args = pendingArguments; transfer = pendingTransfer;
            if (checkpointDue) busy = true;
        }
        if (action.isEmpty() && !checkpointDue) { wait(100); continue; }
        try {
            auto next = doc.clone(); auto nextActive = active; auto nextSince = since; bool nextTicking = ticking;
            auto* sessions = next["sessions"].getArray(); auto* sets = next["sets"].getArray(); const auto index = find(next["sessions"], active);
            if (index >= 0) { sessions->getReference(index).getDynamicObject()->setProperty("seconds", elapsed((*sessions)[index], at, since, ticking)); nextSince = at; }
            if (action == "saveSet") {
                keys(args, "|id||name||tasks|"); const auto id = args["id"].toString(); const auto slot = find(next["sets"], id);
                require(id.isEmpty() || slot >= 0, "That practice set no longer exists.");
                const auto row = object({{"id", id.isEmpty() ? juce::Uuid().toString() : id}, {"name", args["name"]}, {"tasks", args["tasks"].clone()}});
                if (slot < 0) sets->add(row); else sets->set(slot, row);
            } else if (action == "removeSet") {
                const auto slot = find(next["sets"], args.toString()); require(args.isString() && slot >= 0, "Choose an existing practice set."); sets->remove(slot);
            } else if (action == "start") {
                require(active.isEmpty(), "Finish the current practice session first."); require(sessions->size() < 256, "History has 256 sessions. Export it and remove an old session first.");
                keys(args, "|title||minutes||bpm||setName|"); task(object({{"title", args["title"]}, {"minutes", args["minutes"]}, {"bpm", args["bpm"]}}));
                require(text(args["setName"], 48, true), "Invalid practice set name.");
                nextActive = juce::Uuid().toString(); nextSince = at; nextTicking = true;
                sessions->insert(0, object({{"id", nextActive}, {"title", args["title"]}, {"minutes", args["minutes"]}, {"bpm", args["bpm"]}, {"setName", args["setName"]}, {"started", juce::Time::getCurrentTime().toISO8601(true)}, {"seconds", 0.}, {"state", "running"}, {"notes", ""}, {"recordings", juce::Array<juce::var>()}}));
            } else if (action == "pause" || action == "resume" || action == "finish") {
                require(index >= 0, "Start a practice session first."); auto* row = sessions->getReference(index).getDynamicObject();
                if (action == "finish") { require(text(args, 1000, true), "Practice notes can use up to 1000 characters."); row->setProperty("notes", args); row->setProperty("state", "finished"); nextActive.clear(); nextTicking = false; }
                else { require(action == "pause" ? ticking : !ticking, "That timer state has already changed."); require(action != "resume" || static_cast<double>((*sessions)[index]["seconds"]) < 21600, "This timer reached six hours. Finish it and start another session."); nextTicking = action == "resume"; row->setProperty("state", nextTicking ? "running" : "paused"); }
            } else if (action == "removeSession") {
                const auto slot = find(next["sessions"], args.toString()); require(args.isString() && slot >= 0 && args.toString() != active, "Finish the session before removing it."); sessions->remove(slot);
            } else if (action == "addRecording" || action == "removeRecording") {
                keys(args, "|sessionId||takeId||version|");
                const auto slot = find(next["sessions"], args["sessionId"].toString());
                require(text(args["sessionId"], 64) && slot >= 0, "That practice session no longer exists.");
                auto link = object({{"takeId", args["takeId"]}, {"version", args["version"]}}); recording(link);
                auto* links = sessions->getReference(slot)["recordings"].getArray();
                int existing = -1;
                for (int i = 0; i < links->size(); ++i) if ((*links)[i]["takeId"].toString() == link["takeId"].toString() && (*links)[i]["version"].toString() == link["version"].toString()) existing = i;
                if (action == "removeRecording") { require(existing >= 0, "That recording link no longer exists."); links->remove(existing); }
                else if (existing < 0) { require(links->size() < 8, "A session can link up to eight recording versions. Remove a link first."); links->add(link); }
            } else if (action == "import") {
                require(active.isEmpty(), "Finish the timer before importing practice history.");
                const auto imported = readDocument(transfer);
                for (const auto* category : {"sets", "sessions"}) for (const auto& incoming : *imported[category].getArray()) {
                    const bool set = juce::String(category) == "sets";
                    auto row = incoming.clone(); if (!set && (row["state"].toString() == "running" || row["state"].toString() == "paused")) row.getDynamicObject()->setProperty("state", "interrupted");
                    const auto slot = find(next[category], row["id"].toString());
                    if (slot >= 0) { require(equivalent(next[category][slot], row, set), "An imported identity conflicts with existing history. Existing data was preserved."); continue; }
                    next[category].getArray()->add(row);
                }
            } else if (action == "export") {
                require(transfer != file && transfer.hasFileExtension("json") && !transfer.isAChildOf(file.getParentDirectory()), "Choose a JSON export outside practice journal storage.");
                auto exported = next.clone(); for (auto& row : *exported["sessions"].getArray()) if (row["state"].toString() == "running" || row["state"].toString() == "paused") row.getDynamicObject()->setProperty("state", "interrupted");
                write(transfer, exported);
            } else require(action.isEmpty(), "Unknown practice journal command.");
            const auto activeIndex = find(next["sessions"], nextActive);
            if (activeIndex >= 0 && elapsed((*sessions)[activeIndex], at, nextSince, nextTicking) >= 21600) { sessions->getReference(activeIndex).getDynamicObject()->setProperty("state", "paused"); nextTicking = false; }
            persist(next); doc = std::move(next); active = nextActive; since = nextSince; ticking = nextTicking; checkpoint = at; publish(doc, active, since, ticking);
        } catch (const std::exception& e) { const juce::ScopedLock guard(mutex); error = e.what(); checkpoint = at; }
        { const juce::ScopedLock guard(mutex); busy = false; }
    }
    // A normal shutdown keeps elapsed work but never claims a session was finished.
    if (const auto index = find(doc["sessions"], active); index >= 0) try {
        auto* row = doc["sessions"].getArray()->getReference(index).getDynamicObject(); row->setProperty("seconds", elapsed(doc["sessions"][index], now(), since, ticking)); row->setProperty("state", "interrupted"); persist(doc);
    } catch (...) {} // Existing checkpoints remain available if final storage fails.
}
