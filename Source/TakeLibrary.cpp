#include "TakeLibrary.h"
#include "PluginProcessor.h"
#include "TakeRecovery.h"
#include <cmath>

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
juce::String TakeLibrary::maintenance(std::function<void()> work) {
    const juce::ScopedLock guard(lock);
    if (!work || exporting.load() || snapshotPending.load() || maintenancePending.load()) return "Finish the current export/recovery before starting a backup or restore.";
    maintenancePending.store(true); Job job; job.type = "maintenance"; job.maintenance = std::move(work); jobs.push_back(std::move(job)); notify(); return {};
}
void TakeLibrary::waitForMaintenance() { while (maintenancePending.load()) juce::Thread::sleep(5); }
juce::ValueTree TakeLibrary::find(const juce::String& id) { return entries.getChildWithProperty("id", id); }
void TakeLibrary::importFolder(const juce::File& folder)
{ const juce::ScopedLock guard(lock); Job job; job.type = "import"; job.folder = folder; jobs.push_back(std::move(job)); notify(); }
juce::String TakeLibrary::recoverFolder(const juce::File& folder) {
    const juce::ScopedLock guard(lock);
    if (catalogFile==juce::File()) return "Recording recovery requires a saved take library.";
    if (!folder.isDirectory()) return "Choose an interrupted take folder.";
    if (maintenancePending.load() || snapshotPending.load() || exporting.exchange(true)) return "Finish the current export or recovery first.";
    cancelled.store(false); recoveringRecording.store(true); progress.store(0); error.clear(); lastRecoveryPath.clear(); lastRecoverySummary.clear(); lastRecoveryId.clear();
    Job job; job.type="recoverTake"; job.folder=folder; jobs.push_back(std::move(job)); notify(); return {};
}
juce::String TakeLibrary::confirmRecovery(const juce::String& id, bool externalReview) {
    const juce::ScopedLock guard(lock); const auto take=find(id);
    if (!static_cast<bool>(take["recovered"]) || !static_cast<bool>(take["incomplete"])) return "Choose an unconfirmed recovered recording.";
    if (!externalReview && (reviewLoading || reviewId!=id || reviewVersion!="processed")) return "Listen to this recovered processed take or explicitly confirm review in another player.";
    if (maintenancePending.load() || snapshotPending.load() || exporting.exchange(true)) return "Finish the current export or recovery first.";
    cancelled.store(false); recoveringRecording.store(true); progress.store(0); error.clear();
    Job job; job.type="confirmRecovery"; job.id=id; jobs.push_back(std::move(job)); notify(); return {};
}
void TakeLibrary::recoverTake(const Job& job) {
    const auto parent=catalogFile.getParentDirectory().getChildFile("RecoveredRecordings");
    const auto result=TakeRecovery::recover(job.folder,parent,cancelled,[this](double p) { progress.store(p); });
    try { importTake(result.folder); }
    catch (...) { if (result.folder.isAChildOf(parent)) result.folder.deleteRecursively(); throw; }
    const juce::ScopedLock guard(lock); lastRecoveryPath=result.folder.getFullPathName(); lastRecoveryId=entries.getChildWithProperty("path",lastRecoveryPath)["id"].toString();
    lastRecoverySummary="Recovered " + juce::String(result.frames/result.sampleRate,2) + " seconds into a separate take. " + result.warning + "Listen to the processed copy, then confirm it before export or reamping.";
}
void TakeLibrary::approveRecovery(const Job& job) {
    juce::ValueTree previous;
    { const juce::ScopedLock guard(lock); previous=find(job.id).createCopy(); }
    require(previous.isValid() && static_cast<bool>(previous["recovered"]) && static_cast<bool>(previous["incomplete"]),"Recovered take is no longer awaiting review.");
    TakeRecovery::validateReviewed(juce::File(previous["path"].toString()),previous["frames"],previous["sampleRate"],previous["recoveryDrySha256"].toString(),previous["recoveryWetSha256"].toString(),cancelled,previous["recoveryBackingSha256"].toString());
    { const juce::ScopedLock guard(lock); auto take=find(job.id); take.setProperty("recoveryReviewed",true,nullptr); take.setProperty("incomplete",false,nullptr); ++revision; }
    try { persist(job.id); }
    catch (...) { const juce::ScopedLock guard(lock); find(job.id).copyPropertiesAndChildrenFrom(previous,nullptr); ++revision; throw; }
    progress.store(1); const juce::ScopedLock guard(lock); lastRecoverySummary="Recovered recording confirmed. Export and reamping are available; the original interrupted files remain unchanged.";
}
juce::String TakeLibrary::revealRecovery() {
    juce::String path; { const juce::ScopedLock guard(lock); path=lastRecoveryPath; }
    if (!juce::File::isAbsolutePath(path) || !juce::File(path).isDirectory()) return "Recovered recording folder is unavailable.";
    juce::File(path).revealToUser(); return {};
}
juce::String TakeLibrary::edit(const juce::String& id, const juce::String& name, bool favorite)
{
    const auto title = name.trim(); if (title.isEmpty() || title.length() > 80) return "Use a take name between 1 and 80 characters.";
    const juce::ScopedLock guard(lock); if (!find(id).isValid()) return "Take not found.";
    Job job; job.type = "edit"; job.id = id; job.name = title; job.favorite = favorite; jobs.push_back(std::move(job)); notify(); return {};
}
juce::String TakeLibrary::annotate(const juce::String& id, const juce::String& notes)
{
    if (notes.length() > 2000) return "Keep take notes within 2000 characters.";
    const juce::ScopedLock guard(lock); if (!find(id).isValid()) return "Take not found.";
    Job job; job.type = "notes"; job.id = id; job.notes = notes.trim(); jobs.push_back(std::move(job)); notify(); return {};
}
juce::String TakeLibrary::preview(const juce::String& id, const juce::String& version)
{
    const juce::ScopedLock guard(lock); const auto take = find(id);
    if (!take.isValid()) return "Take not found.";
    if (version != "processed" && version != "dry" && !take.getChildWithProperty("id", version).isValid()) return "Take version not found.";
    Job job; job.type = "preview"; job.id = id; job.version = version; job.previewGeneration = ++reviewGeneration;
    review.command("stop"); review.command("cancelLoad");
    reviewId.clear(); reviewVersion.clear(); reviewLoading = true;
    jobs.push_back(std::move(job)); error.clear(); notify(); return {};
}
juce::String TakeLibrary::renameVersion(const juce::String& id, const juce::String& version, const juce::String& name)
{
    const auto title = name.trim(); if (title.isEmpty() || title.length() > 80) return "Use a version name between 1 and 80 characters.";
    const juce::ScopedLock guard(lock);
    if (!find(id).getChildWithProperty("id",version).hasType("REAMP")) return "Choose a reamp version to rename.";
    if (exporting.load()) return "Finish or cancel the export before renaming a version.";
    Job job; job.type = "renameVersion"; job.id = id; job.version = version; job.name = title; jobs.push_back(std::move(job)); notify(); return {};
}
void TakeLibrary::stopReview() {
    const juce::ScopedLock guard(lock); ++reviewGeneration;
    review.command("stop"); review.command("cancelLoad");
    reviewId.clear(); reviewVersion.clear(); reviewLoading = false;
}
juce::String TakeLibrary::reviewControl(const juce::String& id, const juce::String& version, const juce::String& command, double amount)
{
    const juce::ScopedLock guard(lock);
    if (reviewLoading || reviewId.isEmpty() || id != reviewId || version != reviewVersion)
        return "Listen to the selected take version before using its review controls.";
    if (!std::isfinite(amount)) return "Invalid take review value.";
    if (command != "pause" && command != "play" && command != "seek" && command != "a" && command != "b" && command != "loop")
        return "Unknown take review control.";
    return review.command(command, amount);
}
juce::var TakeLibrary::reviewWaveform(const juce::String& id, const juce::String& version)
{
    const juce::ScopedLock guard(lock);
    if (reviewLoading || reviewId.isEmpty() || id != reviewId || version != reviewVersion) {
        auto error = std::make_unique<juce::DynamicObject>(); error->setProperty("error", "Listen to the selected version before reading its waveform."); return juce::var(error.release());
    }
    auto wave = review.waveform(); wave.getDynamicObject()->setProperty("takeId", id); wave.getDynamicObject()->setProperty("version", version); return wave;
}
juce::String TakeLibrary::reviewSection(const juce::String& id, const juce::String& version, const juce::String& command, const juce::String& name, const juce::String& sectionId)
{
    const juce::ScopedLock guard(lock);
    if (reviewLoading || reviewId.isEmpty() || id != reviewId || version != reviewVersion)
        return "Listen to the selected version before editing its sections.";
    if (command != "save" && command != "recall" && command != "remove") return "Unknown take section command.";
    if (sectionId.length() > 64 || (command != "save" && sectionId.isEmpty())) return "Choose a saved section.";
    if (command == "save" && (name.trim().isEmpty() || name.length() > 48)) return "Use a section name between 1 and 48 characters.";
    const auto state = review.status();
    if (static_cast<bool>(state["starting"]) || static_cast<bool>(state["counting"])) return "Wait for review playback to start before editing sections.";
    Job job; job.type = "reviewSection"; job.id = id; job.version = version; job.command = command;
    job.name = name; job.sectionId = sectionId; job.startSeconds = state["a"]; job.endSeconds = state["b"]; job.previewGeneration = reviewGeneration.load();
    jobs.push_back(std::move(job)); notify(); return {};
}
juce::String TakeLibrary::readRigSnapshot(const juce::String& id, const juce::String& version, std::function<void(juce::var)> completed)
{
    const juce::ScopedLock guard(lock); const auto take = find(id);
    if (!take.isValid() || static_cast<bool>(take["incomplete"])) return "Choose a complete take to recover its rig.";
    if (version != "processed" && version != "dry" && !take.getChildWithProperty("id", version).isValid()) return "Take version not found.";
    if (!completed) return "Missing rig recovery callback.";
    if (exporting.load()) return "Finish or cancel the export before recovering a rig.";
    if (snapshotPending.exchange(true)) return "A take rig is already being read.";
    Job job; job.type = "snapshot"; job.id = id; job.version = version; job.completed = std::move(completed);
    jobs.push_back(std::move(job)); notify(); return {};
}
juce::var TakeLibrary::loadRigSnapshot(const Job& job)
{
    juce::File folder, snapshot;
    { const juce::ScopedLock guard(lock); const auto take = find(job.id); require(take.isValid(), "Take not found.");
      const auto path = take["path"].toString(); require(juce::File::isAbsolutePath(path), "Take folder is missing."); folder = juce::File(path);
      if (job.version == "processed" || job.version == "dry") snapshot = folder.getChildFile("Original rig.json");
      else { const auto path = take.getChildWithProperty("id", job.version)["rigPath"].toString(); require(juce::File::isAbsolutePath(path), "Reamp rig snapshot is missing."); snapshot = juce::File(path); } }
    require(snapshot.isAChildOf(folder) && snapshot.hasFileExtension("json") && snapshot.existsAsFile(), "Rig snapshot is missing. Keep it with the take's WAV files.");
    require(snapshot.getSize() > 0 && snapshot.getSize() <= 4 * 1024 * 1024, "Rig snapshot is empty or too large.");
    const auto rig = juce::JSON::parse(snapshot.loadFileAsString());
    require(rig.isObject() && !rig.hasProperty("error") && rig["state"].isString() && rig.hasProperty("schema"), "Rig snapshot is not a valid Cassian document.");
    return rig;
}
juce::String TakeLibrary::reamp(const juce::String& id, const juce::var& rig, double tailSeconds)
{
    if (!std::isfinite(tailSeconds) || tailSeconds < 0 || tailSeconds > 30) return "Choose a reamp tail between 0 and 30 seconds.";
    const juce::ScopedLock guard(lock);
    if (maintenancePending.load()) return "Finish backup/recovery before reamping.";
    if (!find(id).isValid()) return "Take not found.";
    if (static_cast<bool>(find(id)["incomplete"])) return "Check this incomplete recording before using it; choose a complete take to reamp.";
    if (find(id).getNumChildren() >= 64) return "This take already has 64 reamp versions.";
    if (!rig.isObject() || rig.hasProperty("error")) return "Finish loading your rig before reamping.";
    if (exporting.exchange(true)) return "An export is already running.";
    cancelled.store(false); progress.store(0); activeId = id; error.clear();
    Job job; job.type = "reamp"; job.id = id; job.tailSeconds = tailSeconds; job.rig = juce::JSON::parse(juce::JSON::toString(rig)); jobs.push_back(std::move(job)); notify(); return {};
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
    juce::String id; juce::ValueTree previous;
    const bool recovered=metadata["recovered"].isBool() && static_cast<bool>(metadata["recovered"]);
    bool reviewed=false; juce::ValueTree approval;
    { const juce::ScopedLock guard(lock); approval=entries.getChildWithProperty("path",path).createCopy(); reviewed=recovered && static_cast<bool>(approval["recoveryReviewed"]); }
    if (reviewed) {
        try {
            for (const auto* field:{"recoveryDrySha256","recoveryWetSha256","recoveryBackingSha256"}) require(metadata[field].toString()==approval[field].toString(),"Recovery verification metadata changed.");
            const std::atomic<bool> checking {false}; TakeRecovery::validateReviewed(folder,dry->lengthInSamples,dry->sampleRate,approval["recoveryDrySha256"].toString(),approval["recoveryWetSha256"].toString(),checking,approval["recoveryBackingSha256"].toString());
        }
        catch(const std::exception&) { reviewed=false; }
    }
    { const juce::ScopedLock guard(lock);
      auto take = entries.getChildWithProperty("path", path);
      previous=take.createCopy();
      if (!take.isValid()) { require(entries.getNumChildren() < 2048, "The take library is full."); take = juce::ValueTree("TAKE"); take.setProperty("id", juce::Uuid().toString(), nullptr); entries.addChild(take, 0, nullptr); }
      id = take["id"].toString();
      if (!take.hasProperty("name")) take.setProperty("name", metadata["name"].toString().isNotEmpty() ? metadata["name"].toString().substring(0, 80) : folder.getFileName(), nullptr);
      take.setProperty("path", path, nullptr); take.setProperty("frames", dry->lengthInSamples, nullptr); take.setProperty("sampleRate", dry->sampleRate, nullptr);
      take.setProperty("created", metadata["created"].toString().isNotEmpty() ? metadata["created"].toString() : folder.getCreationTime().toISO8601(true), nullptr);
      const bool incomplete=static_cast<bool>(metadata["incomplete"]) || metadata["recordingState"].toString()=="recording";
      take.setProperty("recovered",recovered,nullptr);
      take.setProperty("recoveryReviewed",reviewed,nullptr);
      take.setProperty("incomplete", incomplete && !reviewed, nullptr); take.setProperty("originalRig", folder.getChildFile("Original rig.json").existsAsFile(), nullptr);
      if (recovered) { take.setProperty("recoveryDrySha256",metadata["recoveryDrySha256"],nullptr); take.setProperty("recoveryWetSha256",metadata["recoveryWetSha256"],nullptr); take.setProperty("recoveryBackingSha256",metadata["recoveryBackingSha256"],nullptr); }
      take.setProperty("hasBacking", folder.getChildFile("Backing track.wav").existsAsFile(), nullptr);
      ++revision; }
    try { persist(id); }
    catch (...) {
        const juce::ScopedLock guard(lock); entries.removeChild(find(id),nullptr);
        if (previous.isValid()) entries.addChild(previous,0,nullptr); ++revision; throw;
    }
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
    const auto serialized = merged.createXml()->toString();
    require(serialized.getNumBytesAsUTF8() <= 8 * 1024 * 1024, "Take catalog would exceed 8 MiB. Shorten notes or other metadata before saving.");
    require(catalogFile.getParentDirectory().createDirectory().wasOk(), "Could not create the take catalog folder.");
    juce::TemporaryFile temporary(catalogFile);
    require(temporary.getFile().replaceWithText(serialized) && temporary.overwriteTargetFileWithTemporary(), "Could not save the take catalog.");
    { const juce::ScopedLock guard(lock); entries = merged; ++revision; }
}
void TakeLibrary::playReview(const Job& job)
{
    juce::File file;
    { const juce::ScopedLock guard(lock); const auto take = find(job.id); require(take.isValid(), "Take not found.");
      const juce::File folder(take["path"].toString());
      file = job.version == "processed" ? folder.getChildFile("Guitar processed.wav") : job.version == "dry" ? folder.getChildFile("Guitar dry.wav") : juce::File(take.getChildWithProperty("id", job.version)["path"].toString()); }
    { const juce::ScopedLock guard(lock);
      if (job.previewGeneration != reviewGeneration.load()) return;
      require(file.existsAsFile(), "Take audio is missing. Re-import the folder if it moved.");
      review.load(file); }
    while (static_cast<bool>(review.status()["loading"])) {
        if (threadShouldExit() || job.previewGeneration != reviewGeneration.load()) return; wait(5);
    }
    require(review.status()["error"].toString().isEmpty(), review.status()["error"].toString());
    { const juce::ScopedLock guard(lock);
      if (job.previewGeneration != reviewGeneration.load()) return;
      review.setCountIn(0, 120, 4); const auto failure = review.command("play"); require(failure.isEmpty(), failure);
      reviewId = job.id; reviewVersion = job.version; reviewLoading = false; }
}
void TakeLibrary::exportReamp(const Job& job)
{
    if (cancelled.load() || threadShouldExit()) return;
    juce::File folder;
    { const juce::ScopedLock guard(lock); const auto take = find(job.id); require(take.isValid(), "Take not found."); folder = juce::File(take["path"].toString()); }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> dry(formats.createReaderFor(folder.getChildFile("Guitar dry.wav")));
    require(dry && dry->numChannels == 1 && dry->lengthInSamples > 0 && dry->sampleRate >= 8000 && dry->sampleRate <= 384000 && dry->lengthInSamples * 8. < 4293918720., "Dry take is missing or too large to reamp to a standard WAV.");
    const auto frames = dry->lengthInSamples + static_cast<juce::int64>(std::llround(job.tailSeconds * dry->sampleRate));
    require(frames * 8. < 4293918720., "Take including its tail is too large for a standard WAV.");
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
    for (juce::int64 offset = 0; offset < frames; offset += 1024) {
        if (cancelled.load() || threadShouldExit()) return; // TemporaryFile removes the partial file.
        const int n = static_cast<int>(juce::jmin<juce::int64>(1024, frames - offset));
        audio.clear();
        const int input = static_cast<int>(juce::jlimit<juce::int64>(0, n, dry->lengthInSamples - offset));
        if (input > 0) require(dry->read(&audio, 0, input, offset, true, false), "Could not read the dry take.");
        renderer.renderGuitarOffline(audio, n);
        require(writer->writeFromAudioSampleBuffer(audio, 0, n), "Reamp disk write failed."); progress.store((offset + n) / static_cast<double>(frames));
    }
    writer.reset();
    if (cancelled.load() || threadShouldExit()) return;
    require(!destination.exists() && temporary.overwriteTargetFileWithTemporary(), "Could not finalize the reamp file.");
    require(rigFile.replaceWithText(juce::JSON::toString(job.rig)), "Reamp audio saved, but its rig snapshot could not be saved.");
    { const juce::ScopedLock guard(lock); auto take = find(job.id); juce::ValueTree version("REAMP");
      version.setProperty("id", id, nullptr); version.setProperty("path", destination.getFullPathName(), nullptr);
      version.setProperty("frames", frames, nullptr); version.setProperty("tailSeconds", job.tailSeconds, nullptr);
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
        if (job.type == "maintenance") {
            try {
                job.maintenance();
                if (catalogFile != juce::File()) { const auto loaded = readCatalog(catalogFile); const juce::ScopedLock guard(lock); entries = loaded; ++revision; }
            } catch (const std::exception& e) { const juce::ScopedLock guard(lock); error = e.what(); }
            maintenancePending.store(false); continue;
        }
        if (job.type == "snapshot") {
            juce::var result;
            try { result = loadRigSnapshot(job); }
            catch (const std::exception& e) {
                auto failure = std::make_unique<juce::DynamicObject>(); failure->setProperty("error", juce::String(e.what())); result = juce::var(failure.release());
                const juce::ScopedLock guard(lock); error = e.what();
            }
            snapshotPending.store(false);
            try { job.completed(std::move(result)); }
            catch (const std::exception& e) { const juce::ScopedLock guard(lock); error = e.what(); }
            continue;
        }
        try {
            if (job.type == "import") importTake(job.folder);
            else if (job.type == "recoverTake") recoverTake(job);
            else if (job.type == "confirmRecovery") approveRecovery(job);
            else if (job.type == "edit" || job.type == "notes") {
                juce::ValueTree previous;
                { const juce::ScopedLock guard(lock); auto take = find(job.id); require(take.isValid(), "Take not found."); previous = take.createCopy();
                  if (job.type == "notes") take.setProperty("notes", job.notes, nullptr);
                  else { take.setProperty("name", job.name, nullptr); take.setProperty("favorite", job.favorite, nullptr); } ++revision; }
                try { persist(job.id); }
                catch (...) {
                    const juce::ScopedLock guard(lock); auto take = find(job.id);
                    take.copyPropertiesAndChildrenFrom(previous, nullptr); ++revision; throw;
                }
            }
            else if (job.type == "preview") playReview(job);
            else if (job.type == "reviewSection") {
                const juce::ScopedLock guard(lock);
                if (job.previewGeneration != reviewGeneration.load() || reviewLoading || reviewId != job.id || reviewVersion != job.version) continue;
                const auto failure = job.command == "save" ? review.saveSectionRange(job.name, job.sectionId, job.startSeconds, job.endSeconds)
                    : job.command == "recall" ? review.recallSection(job.sectionId) : review.removeSection(job.sectionId);
                require(failure.isEmpty(), failure);
            }
            else if (job.type == "renameVersion") {
                juce::var previous; bool hadName = false;
                { const juce::ScopedLock guard(lock); auto version = find(job.id).getChildWithProperty("id",job.version); require(version.hasType("REAMP"),"Take version not found.");
                  hadName = version.hasProperty("name"); previous = version["name"]; version.setProperty("name",job.name,nullptr); ++revision; }
                try { persist(job.id); }
                catch (...) {
                    const juce::ScopedLock guard(lock); auto version = find(job.id).getChildWithProperty("id",job.version);
                    if (hadName) version.setProperty("name",previous,nullptr); else version.removeProperty("name",nullptr);
                    ++revision; throw;
                }
            }
            else if (job.type == "reamp") exportReamp(job);
            else if (job.type == "video") exportVideoAudio(job);
        } catch (const std::exception& e) {
            const juce::ScopedLock guard(lock);
            if ((job.type != "preview" && job.type != "reviewSection") || job.previewGeneration == reviewGeneration.load()) {
                error = e.what(); if (job.type == "preview") reviewLoading = false;
            }
        }
        if (job.type == "reamp" || job.type == "video" || job.type == "recoverTake" || job.type == "confirmRecovery") { exporting.store(false); recoveringRecording.store(false); const juce::ScopedLock guard(lock); activeId.clear(); }
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
    { const juce::ScopedLock guard(lock); o->setProperty("error", error); o->setProperty("activeId", activeId); o->setProperty("reviewId", reviewId); o->setProperty("reviewVersion", reviewVersion); o->setProperty("reviewLoading", reviewLoading); o->setProperty("lastExportPath", lastExportPath); o->setProperty("lastExportReport", lastExportReport); }
    { const juce::ScopedLock guard(lock); o->setProperty("lastRecoveryPath",lastRecoveryPath); o->setProperty("lastRecoverySummary",lastRecoverySummary); o->setProperty("lastRecoveryId",lastRecoveryId); }
    o->setProperty("recoveringRecording",recoveringRecording.load());
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
