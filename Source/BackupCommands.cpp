#include "PluginProcessor.h"

juce::String AmpSuiteAudioProcessor::requestBackup(bool restore, const juce::File& file, bool includeTakes, std::optional<juce::StringArray> selectedTakeIds)
{
    if (selectedTakeIds && (restore || !includeTakes || selectedTakeIds->isEmpty())) return "Choose recorded takes for a new selective backup.";
    if (!sharedStore.enabled()) return "Backups require shared library storage.";
    if (static_cast<int>(practice.status()["recordMode"]) != 0) return "Finish recording before backup/recovery.";
    if (rigLoading.load()) return "Finish rig loading before backup/recovery.";
    if (file == juce::File()) return "Choose a backup file.";
    if (backupBusy.exchange(true)) return "A backup/recovery is already running.";
    const auto current = restore ? juce::var() : getRig();
    if (current.hasProperty("error")) { backupBusy.store(false); return current["error"].toString(); }
    backupCancelled.store(false); backupProgress.store(0);
    { const juce::ScopedLock guard(backupLock); backupOperation = restore ? "restore" : "backup"; backupError.clear(); backupSummary.clear(); backupLocation.clear(); }
    const auto failure = takes.maintenance([this, restore, file, current, includeTakes, selectedTakeIds] {
        try {
            const auto update = [this](double value) { backupProgress.store(value); };
            const auto report = restore ? LibraryBackup::restore(file, sharedStore.root(), backupCancelled, [this](const juce::var& doc) { return validateRigDocument(doc); }, update)
                                        : LibraryBackup::create(sharedStore.root(), file, current, backupCancelled, update, includeTakes, selectedTakeIds);
            const juce::ScopedLock guard(backupLock); backupLocation = report.location.getFullPathName();
            backupSummary = restore ? juce::String(report.rigs) + " recovered rigs and " + juce::String(report.takes) + " takes added. Current tone preserved."
                                    : juce::String(selectedTakeIds ? "Verified selected-takes backup saved: " : includeTakes ? "Verified complete backup saved: " : "Verified tone-library backup saved: ") + juce::String(report.files) + " files, " + juce::String(report.takes) + " takes.";
            if (restore) backupSummary += " " + juce::String(report.sectionsAdded) + " section files added; " + juce::String(report.sectionsKept) + " existing files kept.";
            if (report.warning.isNotEmpty()) backupSummary += " " + report.warning;
        } catch (const std::exception& e) { const juce::ScopedLock guard(backupLock); backupError = e.what(); }
        backupBusy.store(false);
    });
    if (failure.isNotEmpty()) { const juce::ScopedLock guard(backupLock); backupError = failure; backupBusy.store(false); }
    return failure;
}
juce::var AmpSuiteAudioProcessor::backupStatus() {
    auto object = std::make_unique<juce::DynamicObject>(); const juce::ScopedLock guard(backupLock);
    object->setProperty("available", sharedStore.enabled()); object->setProperty("busy", backupBusy.load() || takes.maintenanceBusy());
    object->setProperty("progress", backupProgress.load()); object->setProperty("operation", backupOperation);
    object->setProperty("error", backupError); object->setProperty("summary", backupSummary); object->setProperty("path", backupLocation);
    return juce::var(object.release());
}
juce::String AmpSuiteAudioProcessor::revealBackup() {
    juce::String path; { const juce::ScopedLock guard(backupLock); path = backupLocation; }
    if (!juce::File::isAbsolutePath(path) || !juce::File(path).exists()) return "Backup/recovery output is no longer available.";
    juce::File(path).revealToUser(); return {};
}
