#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <functional>
#include <optional>
#include "BackupZip.h"

// Personal archives, never factory sound-distribution packages. Runs on a worker.
class LibraryBackup
{
public:
    struct Report { juce::File location; int files = 0, rigs = 0, takes = 0; juce::int64 bytes = 0; juce::String warning; int sectionsAdded = 0, sectionsKept = 0, sectionsSkipped = 0; };
    using Progress = std::function<void(double)>;
    using Validator = std::function<juce::String(const juce::var&)>;
    // Small archives retain classic ZIP; large ones automatically use ZIP64.
    // The explicit format override also permits small compatibility fixtures.
    static Report create(const juce::File& root, const juce::File& archive, const juce::var& currentRig,
                         const std::atomic<bool>& cancelled, Progress = {}, bool includeTakes = true,
                         std::optional<juce::StringArray> selectedTakeIds = std::nullopt,
                         bool forceZip64 = false);
    // New rig/take identities, isolated media and additive catalog commits.
    // Existing files, saved tones and the active playing rig are preserved.
    static Report restore(const juce::File& archive, const juce::File& root,
                          const std::atomic<bool>& cancelled, Validator, Progress = {});
};
