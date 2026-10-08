#include "PluginProcessor.h"

void AmpSuiteAudioProcessor::startToneRecovery() {
    if (!toneRecovery && showDeviceSettings && sharedStore.enabled()) {
        toneRecovery = std::make_unique<ToneRecovery>(sharedStore.root()); startTimer(60000);
    }
}
void AmpSuiteAudioProcessor::timerCallback() {
    if (toneRecovery && toneRecovery->automatic() && !toneRecovery->busy() && !rigLoading.load() && !static_cast<bool>(backupStatus()["busy"]) && static_cast<int>(practice.status()["recordMode"]) == 0) captureRecoveryTone();
}
juce::String AmpSuiteAudioProcessor::captureRecoveryTone() {
    if (!toneRecovery) return "Automatic tone recovery is available in standalone. Save your DAW project separately.";
    if (static_cast<bool>(backupStatus()["busy"]) || static_cast<bool>(takes.status()["exporting"]) || rigLoading.load() || static_cast<int>(practice.status()["recordMode"]) != 0) return "Finish recording, loading, exports and backup/recovery before capturing a tone snapshot.";
    const auto rig = getRig(); if (rig.hasProperty("error")) return rig["error"].toString();
    juce::String name; { const juce::ScopedLock guard(requestLock); name = activeRig.name; }
    return toneRecovery->capture(rig, name);
}
juce::String AmpSuiteAudioProcessor::setAutomaticRecovery(bool enabled) { return toneRecovery ? toneRecovery->setAutomatic(enabled) : juce::String("Automatic tone recovery is available in standalone."); }
juce::var AmpSuiteAudioProcessor::toneRecoveryStatus() {
    if (toneRecovery) return toneRecovery->status();
    auto row = std::make_unique<juce::DynamicObject>(); row->setProperty("available", false); row->setProperty("snapshots", juce::Array<juce::var>()); return juce::var(row.release());
}
juce::String AmpSuiteAudioProcessor::recoverTone(const juce::String& id) {
    if (!toneRecovery) return "Automatic tone recovery is available in standalone.";
    if (toneRecovery->busy() || static_cast<bool>(backupStatus()["busy"]) || rigLoading.load() || static_cast<int>(practice.status()["recordMode"]) != 0) return "Finish recording, loading and recovery operations first.";
    try {
        const auto doc = toneRecovery->read(id);
        if (const auto error = validateRigDocument(doc); error.isNotEmpty()) return "Cannot recover tone: " + error;
        const juce::ScopedLock guard(requestLock); const auto previous = library.tree.createCopy();
        const auto error = importRig(doc["name"].toString().substring(0, 58) + " (tone recovery)", doc);
        if (error.isNotEmpty()) {library.tree = previous; ++library.revision;}
        return error;
    } catch (const std::exception& e) {return e.what();}
}
