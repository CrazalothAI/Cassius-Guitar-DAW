#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>

// Standalone reference snapshots. No capture/audio copying or automatic recall.
// JSON writes, scans and retention run on this worker, never in the audio callback.
class ToneRecovery final : private juce::Thread
{
public:
    explicit ToneRecovery(juce::File libraryRoot);
    ~ToneRecovery() override;
    juce::String capture(const juce::var& rig, const juce::String& name);
    juce::String setAutomatic(bool enabled);
    juce::var status();
    juce::var read(const juce::String& id);
    bool automatic() const { return automaticEnabled.load(); }
    bool busy() const { return pending.load(); }
private:
    void run() override;
    void scan();
    juce::File folder, lastFile;
    juce::InterProcessLock shared;
    juce::CriticalSection lock;
    juce::var queuedRig, entries = juce::Array<juce::var>();
    juce::String queuedName, lastHash, error;
    bool writePreferences = false;
    std::atomic<bool> automaticEnabled {true}, pending {true};
};
