#include "TakeLibrary.h"
#include "PluginProcessor.h"

namespace {
void require(bool condition, const juce::String& reason) { if (!condition) throw std::runtime_error(reason.toStdString()); }
juce::var json(const juce::File& file) { return file.existsAsFile() && file.getSize() <= 4 * 1024 * 1024 ? juce::JSON::parse(file.loadFileAsString()) : juce::var(); }
juce::ValueTree readCatalog(const juce::File& file) {
    if (!file.existsAsFile()) return juce::ValueTree("TAKES");
    require(file.getSize() <= 8 * 1024 * 1024, "Take catalog is too large.");
    const auto xml = juce::XmlDocument::parse(file);
    require(xml && xml->hasTagName("TAKES"), "Could not read the take catalog.");
    auto tree = juce::ValueTree::fromXml(*xml);
    require(tree.getNumChildren() <= 2048, "Take catalog has too many entries."); return tree;
}
}
TakeLibrary::TakeLibrary(juce::File file, PracticeEngine& player) : Thread("Cassian take library"), catalogFile(std::move(file)), review(player)
{ startThread(); }
TakeLibrary::~TakeLibrary() { cancelled.store(true); stopReview(); signalThreadShouldExit(); notify(); stopThread(-1); }
juce::ValueTree TakeLibrary::find(const juce::String& id) { return entries.getChildWithProperty("id", id); }
void TakeLibrary::importFolder(const juce::File& folder)
{ const juce::ScopedLock guard(lock); Job job; job.type = "import"; job.folder = folder; jobs.push_back(std::move(job)); notify(); }
juce::String TakeLibrary::edit(const juce::String& id, const juce::String& name, bool favorite)
{
    const auto title = name.trim(); if (title.isEmpty() || title.length() > 80) return "Use a take name between 1 and 80 characters.";
    const juce::ScopedLock guard(lock); if (!find(id).isValid()) return "Take not found.";
    Job job; job.type = "edit"; job.id = id; job.name = title; job.favorite = favorite; jobs.push_back(std::move(job)); notify(); return {};
}
juce::String TakeLibrary::preview(const juce::String& id, const juce::String& version)
{
    const juce::ScopedLock guard(lock); const auto take = find(id);
    if (!take.isValid()) return "Take not found.";
    if (version != "processed" && version != "dry" && !take.getChildWithProperty("id", version).isValid()) return "Take version not found.";
    Job job; job.type = "preview"; job.id = id; job.version = version; job.previewGeneration = ++reviewGeneration;
    jobs.push_back(std::move(job)); error.clear(); notify(); return {};
}
void TakeLibrary::stopReview() { const juce::ScopedLock guard(lock); ++reviewGeneration; review.command("stop"); }
juce::String TakeLibrary::reamp(const juce::String& id, const juce::var& rig)
{
    const juce::ScopedLock guard(lock);
    if (!find(id).isValid()) return "Take not found.";
    if (static_cast<bool>(find(id)["incomplete"])) return "Check this incomplete recording before using it; choose a complete take to reamp.";
    if (find(id).getNumChildren() >= 64) return "This take already has 64 reamp versions.";
    if (!rig.isObject() || rig.hasProperty("error")) return "Finish loading your rig before reamping.";
    if (exporting.exchange(true)) return "An export is already running.";
    cancelled.store(false); progress.store(0); activeId = id; error.clear();
    Job job; job.type = "reamp"; job.id = id; job.rig = juce::JSON::parse(juce::JSON::toString(rig)); jobs.push_back(std::move(job)); notify(); return {};
}
void TakeLibrary::importTake(const juce::File& folder)
{
    require(folder.isDirectory(), "Take folder is missing.");
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> dry(formats.createReaderFor(folder.getChildFile("Guitar dry.wav"))), wet(formats.createReaderFor(folder.getChildFile("Guitar processed.wav")));
    require(dry && wet && dry->numChannels == 1 && wet->numChannels == 2 && dry->lengthInSamples > 0 && dry->lengthInSamples == wet->lengthInSamples && dry->sampleRate == wet->sampleRate && dry->sampleRate >= 8000 && dry->sampleRate <= 384000,
        "Choose a take folder with matching mono Guitar dry.wav and stereo Guitar processed.wav files.");
    const auto metadata = json(folder.getChildFile("Cassian take.json"));
    const auto path = folder.getFullPathName();
    juce::String id;
    { const juce::ScopedLock guard(lock);
      auto take = entries.getChildWithProperty("path", path);
      if (!take.isValid()) { require(entries.getNumChildren() < 2048, "The take library is full."); take = juce::ValueTree("TAKE"); take.setProperty("id", juce::Uuid().toString(), nullptr); entries.addChild(take, 0, nullptr); }
      id = take["id"].toString();
      if (!take.hasProperty("name")) take.setProperty("name", metadata["name"].toString().isNotEmpty() ? metadata["name"].toString().substring(0, 80) : folder.getFileName(), nullptr);
      take.setProperty("path", path, nullptr); take.setProperty("frames", dry->lengthInSamples, nullptr); take.setProperty("sampleRate", dry->sampleRate, nullptr);
      take.setProperty("created", metadata["created"].toString().isNotEmpty() ? metadata["created"].toString() : folder.getCreationTime().toISO8601(true), nullptr);
      take.setProperty("incomplete", static_cast<bool>(metadata["incomplete"]), nullptr); take.setProperty("originalRig", folder.getChildFile("Original rig.json").existsAsFile(), nullptr);
      ++revision; }
    persist(id);
}
void TakeLibrary::persist(const juce::String& changedId)
{
    if (catalogFile == juce::File()) return;
    // Merge only the changed entry, so another app's unmodified takes survive.
    juce::InterProcessLock mutex("CassianTakes-" + juce::SHA256(catalogFile.getFullPathName().toRawUTF8(), static_cast<size_t>(catalogFile.getFullPathName().getNumBytesAsUTF8())).toHexString());
    require(mutex.enter(3000), "Take library is busy; try again.");
    struct Unlock { juce::InterProcessLock& lock; ~Unlock() { lock.exit(); } } unlock {mutex};
    auto merged = readCatalog(catalogFile); juce::ValueTree changed;
    { const juce::ScopedLock guard(lock); changed = find(changedId).createCopy(); }
    const auto old = merged.getChildWithProperty("id", changedId); if (old.isValid()) merged.removeChild(old, nullptr);
    merged.addChild(changed, 0, nullptr); require(merged.getNumChildren() <= 2048, "The shared take library is full.");
    require(catalogFile.getParentDirectory().createDirectory().wasOk(), "Could not create the take catalog folder.");
    juce::TemporaryFile temporary(catalogFile);
    require(temporary.getFile().replaceWithText(merged.createXml()->toString()) && temporary.overwriteTargetFileWithTemporary(), "Could not save the take catalog.");
    { const juce::ScopedLock guard(lock); entries = merged; ++revision; }
}
void TakeLibrary::playReview(const Job& job)
{
    juce::File file;
    { const juce::ScopedLock guard(lock); const auto take = find(job.id); require(take.isValid(), "Take not found.");
      const juce::File folder(take["path"].toString());
      file = job.version == "processed" ? folder.getChildFile("Guitar processed.wav") : job.version == "dry" ? folder.getChildFile("Guitar dry.wav") : juce::File(take.getChildWithProperty("id", job.version)["path"].toString()); }
    if (job.previewGeneration != reviewGeneration.load()) return;
    require(file.existsAsFile(), "Take audio is missing. Re-import the folder if it moved.");
    review.load(file);
    while (static_cast<bool>(review.status()["loading"])) {
        if (threadShouldExit() || job.previewGeneration != reviewGeneration.load()) return; wait(5);
    }
    require(review.status()["error"].toString().isEmpty(), review.status()["error"].toString());
    { const juce::ScopedLock guard(lock);
      if (job.previewGeneration != reviewGeneration.load()) return;
      review.setCountIn(0, 120, 4); const auto failure = review.command("play"); require(failure.isEmpty(), failure); reviewId = job.id; }
}
void TakeLibrary::exportReamp(const Job& job)
{
    if (cancelled.load() || threadShouldExit()) return;
    juce::File folder;
    { const juce::ScopedLock guard(lock); const auto take = find(job.id); require(take.isValid(), "Take not found."); folder = juce::File(take["path"].toString()); }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> dry(formats.createReaderFor(folder.getChildFile("Guitar dry.wav")));
    require(dry && dry->numChannels == 1 && dry->lengthInSamples > 0 && dry->sampleRate >= 8000 && dry->sampleRate <= 384000 && dry->lengthInSamples * 8. < 4293918720., "Dry take is missing or too large to reamp to a standard WAV.");
    AmpSuiteAudioProcessor renderer(false); renderer.setNonRealtime(true); renderer.prepareToPlay(dry->sampleRate, 1024);
    const auto failure = renderer.applyRig(job.rig, false); require(failure.isEmpty(), failure);
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 30000;
    while (renderer.getRig().hasProperty("error")) {
        if (cancelled.load() || threadShouldExit()) return;
        require(!renderer.status()["message"].toString().startsWith("Load failed:"), renderer.status()["message"].toString());
        require(juce::Time::getMillisecondCounterHiRes() < deadline, "Rig preparation timed out."); wait(5);
    }
    require(!renderer.status()["message"].toString().startsWith("Load failed:"), renderer.status()["message"].toString());
    // Re-prepare only this isolated instance with its final settings. This resets
    // DSP histories/ramp starts consistently; live audio remains untouched.
    renderer.prepareToPlay(dry->sampleRate, 1024);
    const auto id = juce::Uuid().toString(), basename = "Reamp " + id;
    const auto destination = folder.getChildFile(basename + ".wav"), rigFile = folder.getChildFile(basename + ".json");
    juce::TemporaryFile temporary(destination); juce::WavAudioFormat wav;
    auto stream = temporary.getFile().createOutputStream(); require(stream != nullptr, "Could not create reamp output.");
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), dry->sampleRate, 2, 32, {}, 0));
    require(writer != nullptr, "Could not create reamp WAV writer."); stream.release();
    juce::AudioBuffer<float> audio(2, 1024);
    for (juce::int64 offset = 0; offset < dry->lengthInSamples; offset += 1024) {
        if (cancelled.load() || threadShouldExit()) return; // TemporaryFile removes the partial file.
        const int n = static_cast<int>(juce::jmin<juce::int64>(1024, dry->lengthInSamples - offset));
        audio.clear(); require(dry->read(&audio, 0, n, offset, true, false), "Could not read the dry take.");
        renderer.renderGuitarOffline(audio, n);
        require(writer->writeFromAudioSampleBuffer(audio, 0, n), "Reamp disk write failed."); progress.store((offset + n) / static_cast<double>(dry->lengthInSamples));
    }
    writer.reset();
    if (cancelled.load() || threadShouldExit()) return;
    require(!destination.exists() && temporary.overwriteTargetFileWithTemporary(), "Could not finalize the reamp file.");
    require(rigFile.replaceWithText(juce::JSON::toString(job.rig)), "Reamp audio saved, but its rig snapshot could not be saved.");
    { const juce::ScopedLock guard(lock); auto take = find(job.id); juce::ValueTree version("REAMP");
      version.setProperty("id", id, nullptr); version.setProperty("path", destination.getFullPathName(), nullptr);
      version.setProperty("name", "Reamp " + juce::Time::getCurrentTime().formatted("%H:%M:%S"), nullptr); version.setProperty("rigPath", rigFile.getFullPathName(), nullptr);
      take.addChild(version, -1, nullptr); ++revision; }
    persist(job.id);
}
void TakeLibrary::run()
{
    try { if (catalogFile != juce::File()) { const auto loaded = readCatalog(catalogFile); const juce::ScopedLock guard(lock); entries = loaded; ++revision; } }
    catch (const std::exception& e) { const juce::ScopedLock guard(lock); error = e.what(); }
    while (!threadShouldExit()) {
        Job job; bool ready = false;
        { const juce::ScopedLock guard(lock); if (!jobs.empty()) { job = std::move(jobs.front()); jobs.erase(jobs.begin()); ready = true; } }
        if (!ready) { wait(250); continue; }
        { const juce::ScopedLock guard(lock); error.clear(); }
        try {
            if (job.type == "import") importTake(job.folder);
            else if (job.type == "edit") {
                { const juce::ScopedLock guard(lock); auto take = find(job.id); require(take.isValid(), "Take not found."); take.setProperty("name", job.name, nullptr); take.setProperty("favorite", job.favorite, nullptr); ++revision; }
                persist(job.id);
            }
            else if (job.type == "preview") playReview(job);
            else if (job.type == "reamp") exportReamp(job);
        } catch (const std::exception& e) { const juce::ScopedLock guard(lock); error = e.what(); }
        if (job.type == "reamp") { exporting.store(false); const juce::ScopedLock guard(lock); activeId.clear(); }
    }
}
juce::var TakeLibrary::list()
{
    const juce::ScopedLock guard(lock); juce::Array<juce::var> takes;
    for (const auto& entry : entries) {
        auto o = std::make_unique<juce::DynamicObject>(); for (int i = 0; i < entry.getNumProperties(); ++i) { const auto key = entry.getPropertyName(i); o->setProperty(key, entry[key]); }
        juce::Array<juce::var> versions;
        for (const auto& version : entry) { auto v = std::make_unique<juce::DynamicObject>(); for (int i = 0; i < version.getNumProperties(); ++i) { const auto key = version.getPropertyName(i); v->setProperty(key, version[key]); } versions.add(juce::var(v.release())); }
        o->setProperty("versions", versions); takes.add(juce::var(o.release()));
    }
    return takes;
}
juce::var TakeLibrary::status()
{
    auto o = std::make_unique<juce::DynamicObject>();
    { const juce::ScopedLock guard(lock); o->setProperty("error", error); o->setProperty("activeId", activeId); o->setProperty("reviewId", reviewId); }
    o->setProperty("revision", static_cast<int>(revision.load())); o->setProperty("exporting", exporting.load()); o->setProperty("progress", progress.load()); return juce::var(o.release());
}
juce::String TakeLibrary::reveal(const juce::String& id)
{
    juce::String path;
    { const juce::ScopedLock guard(lock); path = find(id)["path"].toString(); }
    if (path.isEmpty() || !juce::File::isAbsolutePath(path)) return "Take not found.";
    const juce::File folder(path); if (!folder.isDirectory()) return "Take folder is missing. Re-import it if it moved.";
    folder.revealToUser(); return {};
}
