#include "../Source/PracticeJournal.h"
#include "../Source/PluginProcessor.h"
#include <atomic>
#include <thread>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
template<typename F> void waitFor(F predicate) {
    const auto end = juce::Time::getMillisecondCounter() + 5000;
    while (!predicate() && juce::Time::getMillisecondCounter() < end) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    require(predicate(), "Practice journal worker must finish within five seconds");
}
void ready(PracticeJournal& journal) { waitFor([&] { return !static_cast<bool>(journal.status()["busy"]); }); }
void send(PracticeJournal& journal, const char* command, juce::var args = {}, juce::File transfer = {}) {
    ready(journal); require(journal.command(command, args, transfer).isEmpty(), "Valid journal command must queue"); ready(journal);
    require(journal.status()["error"].toString().isEmpty(), "Valid journal command must persist");
}
void rejected(PracticeJournal& journal, const char* command, juce::var args = {}, juce::File transfer = {}) {
    ready(journal); const auto before = juce::JSON::toString(journal.document());
    require(journal.command(command, args, transfer).isEmpty(), "Rejected document request must reach worker validation"); ready(journal);
    require(journal.status()["error"].toString().isNotEmpty() && juce::JSON::toString(journal.document()) == before, "Rejected changes must preserve the previously published document");
}
juce::var task(const char* title = "Expressive clean", int bpm = 80) {
    return juce::JSON::parse("{\"title\":\"" + juce::String(title) + "\",\"minutes\":10,\"bpm\":" + juce::String(bpm) + "}");
}
juce::var set() {
    auto row = juce::JSON::parse(R"({"id":"","name":"Clean and lead","tasks":[]})"); row["tasks"].getArray()->add(task()); row["tasks"].getArray()->add(task("Alternate picking", 120)); return row;
}
juce::var start() { auto row = task(); row.getDynamicObject()->setProperty("setName", "Clean and lead"); return row; }
}
void runPracticeJournalChecks() {
    const auto parent = juce::File::getSpecialLocation(juce::File::tempDirectory), base = parent.getNonexistentChildFile("Cassian-journal", "", false);
    require(base.createDirectory().wasOk(), "Journal test folder must create");
    struct Cleanup { juce::File path, parent; ~Cleanup() { if (path.isAChildOf(parent)) path.deleteRecursively(); } } cleanup {base, parent};
    const auto file = base.getChildFile("Library/practice-journal.json"), exported = base.getChildFile("Portable.json");
    std::atomic<double> clock {100}; auto now = [&] { return clock.load(); };
    juce::String id;
    {
        PracticeJournal journal(file, now); ready(journal); require(static_cast<bool>(journal.status()["writable"]), "First journal must own writable storage");
        PracticeJournal other(file, now); ready(other); require(!static_cast<bool>(other.status()["writable"]) && other.command("start", start()).isNotEmpty(), "A second window must not change the active journal");
        send(journal, "saveSet", set()); id = journal.document()["sets"][0]["id"].toString();
        require(id.isNotEmpty() && journal.document()["sets"][0]["tasks"].size() == 2, "Reusable set must retain independent exercises");
        auto changed = set(); changed.getDynamicObject()->setProperty("id", id); changed["tasks"].getArray()->getReference(1).getDynamicObject()->setProperty("bpm", 140); send(journal, "saveSet", changed);
        require(journal.document()["sets"].size() == 1 && static_cast<int>(journal.document()["sets"][0]["tasks"][1]["bpm"]) == 140, "Editing a set must preserve its stable identity");
        auto bad = set(); bad["tasks"].getArray()->getReference(0).getDynamicObject()->setProperty("minutes", 0); rejected(journal, "saveSet", bad);
        bad = set(); bad.getDynamicObject()->setProperty("path", "C:/private.wav"); rejected(journal, "saveSet", bad);
        send(journal, "start", start()); clock.store(110);
        require(std::abs(static_cast<double>(journal.status()["active"]["seconds"]) - 10) < 1e-8, "Timer must follow the native monotonic clock");
        rejected(journal, "start", start());
        send(journal, "pause"); clock.store(140);
        require(static_cast<double>(journal.status()["active"]["seconds"]) == 10, "Pauses must stay outside elapsed practice time");
        send(journal, "resume"); clock.store(145); send(journal, "finish", "Clean decay improved\nTry 90 BPM next");
        require(!journal.status()["active"].isObject() && static_cast<double>(journal.document()["sessions"][0]["seconds"]) == 15 && journal.document()["sessions"][0]["state"].toString() == "finished", "Finishing must save only running time and notes");
        send(journal, "removeSet", id); require(journal.document()["sessions"][0]["setName"].toString() == "Clean and lead", "Deleting a plan must preserve its historical session snapshot");
        send(journal, "saveSet", set()); send(journal, "export", {}, exported);
        require(PracticeJournal::readDocument(exported)["sessions"][0]["notes"].toString().contains("90 BPM"), "Portable exports must retain notes");
        send(journal, "start", start()); clock.store(176);
        waitFor([&] { return static_cast<double>(journal.document()["sessions"][0]["seconds"]) >= 30; });
        require(PracticeJournal::readDocument(file)["sessions"][0]["state"].toString() == "running", "Active timer must checkpoint on the disk worker");
        rejected(journal, "removeSession", journal.status()["active"]["id"]);
        clock.store(180); // Shutdown saves the current elapsed time without pretending it finished.
    }
    {
        PracticeJournal reopened(file, now); ready(reopened);
        require(!reopened.status()["active"].isObject() && reopened.document()["sessions"][0]["state"].toString() == "interrupted" && static_cast<double>(reopened.document()["sessions"][0]["seconds"]) == 35, "Reopening must retain an interrupted session without counting closed-app time");
        const auto stable = juce::JSON::toString(reopened.document());
        send(reopened, "import", {}, exported); send(reopened, "import", {}, exported);
        require(juce::JSON::toString(reopened.document()) == stable, "Repeated portable imports must not duplicate sessions or sets");
        const auto foreign = juce::JSON::parse(stable); foreign["sets"].getArray()->getReference(0).getDynamicObject()->setProperty("name", "Conflicting set");
        const auto conflict = base.getChildFile("Conflict.json"); conflict.replaceWithText(juce::JSON::toString(foreign)); rejected(reopened, "import", {}, conflict);
        const auto corrupt = base.getChildFile("Corrupt.json"); corrupt.replaceWithText("{broken"); rejected(reopened, "import", {}, corrupt);
        send(reopened, "removeSession", reopened.document()["sessions"][0]["id"]);
        // External manual edits are preserved even though they bypass the lease.
        const auto changed = reopened.document(); changed.getDynamicObject()->setProperty("schema", 9); file.replaceWithText(juce::JSON::toString(changed));
        const auto checksum = juce::SHA256(file).toHexString(); rejected(reopened, "saveSet", set()); require(juce::SHA256(file).toHexString() == checksum, "External changes must never be overwritten by a stale journal");
    }
    {
        const auto checksum = juce::SHA256(file).toHexString(); PracticeJournal broken(file, now); ready(broken);
        require(!static_cast<bool>(broken.status()["writable"]) && juce::SHA256(file).toHexString() == checksum, "Unsupported documents must fail closed without resetting history");
    }
    {
        PracticeJournal imported(base.getChildFile("Other/practice-journal.json"), now); ready(imported); send(imported, "import", {}, exported);
        require(imported.document()["sets"].size() == 1 && imported.document()["sessions"].size() == 1, "A second PC must be able to import portable plans and finished history");
        auto unfinished = PracticeJournal::readDocument(exported); auto* entry = unfinished["sessions"].getArray()->getReference(0).getDynamicObject(); entry->setProperty("id", "unfinished-import"); entry->setProperty("state", "running");
        const auto rawCheckpoint = base.getChildFile("Unfinished.json"); rawCheckpoint.replaceWithText(juce::JSON::toString(unfinished));
        send(imported, "import", {}, rawCheckpoint); send(imported, "import", {}, rawCheckpoint);
        require(imported.document()["sessions"].size() == 2 && imported.document()["sessions"][1]["state"].toString() == "interrupted", "Repeated imports of an unfinished checkpoint must stay idempotent after normalization");
        unfinished["sets"].getArray()->getReference(0).getDynamicObject()->setProperty("tasks", juce::JSON::parse(R"([{"bpm":80.0,"minutes":10.0,"title":"Expressive clean"},{"bpm":120,"minutes":10,"title":"Alternate picking"}])"));
        rawCheckpoint.replaceWithText(juce::JSON::toString(unfinished)); send(imported, "import", {}, rawCheckpoint);
        send(imported, "start", start()); clock.store(22000); waitFor([&] { return imported.status()["active"]["state"].toString() == "paused"; });
        require(static_cast<double>(imported.status()["active"]["seconds"]) == 21600, "A forgotten timer must stop at the six-hour bound");
        send(imported, "finish", "Bounded timer");
        for (int i = 1; i < 32; ++i) send(imported, "saveSet", set());
        rejected(imported, "saveSet", set());
    }
    {
        auto full = PracticeJournal::readDocument(exported); auto* rows = full["sessions"].getArray(); const auto source = (*rows)[0].clone(); rows->clear();
        for (int i = 0; i < 256; ++i) { auto row = source.clone(); row.getDynamicObject()->setProperty("id", "session-" + juce::String(i)); rows->add(row); }
        const auto importFile = base.getChildFile("Full.json"); importFile.replaceWithText(juce::JSON::toString(full));
        PracticeJournal bounded(base.getChildFile("Full library/practice-journal.json"), now); ready(bounded); send(bounded, "import", {}, importFile);
        rejected(bounded, "start", start()); require(bounded.document()["sessions"].size() == 256, "A full history must reject new timers without silently evicting old work");
        send(bounded, "removeSession", "session-0"); send(bounded, "start", start()); send(bounded, "finish", "Space was explicitly made");
        require(bounded.document()["sessions"].size() == 256, "Explicit removal must permit another session at the documented capacity");
    }
    // Journal work must stay outside plugin state and normal processor audio.
    AmpSuiteAudioProcessor plugin(false); require(!plugin.practiceJournal && !plugin.status()["practiceJournal"].isObject(), "DAW instances must not own a standalone practice journal");
}
