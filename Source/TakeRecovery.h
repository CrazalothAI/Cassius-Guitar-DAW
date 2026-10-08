#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <functional>

// Explicit recovery of a readable, checkpointed prefix. Never repairs or
// overwrites source stems, guesses raw data lengths, or resumes recording.
namespace TakeRecovery {
struct Result { juce::File folder; juce::int64 frames = 0; double sampleRate = 0; juce::String warning; };
juce::String lockName(const juce::File&);
void writeMetadata(const juce::File&, const juce::var&);
juce::String digest(const juce::File&, const std::atomic<bool>&);
Result recover(const juce::File& source, const juce::File& destinationRoot, const std::atomic<bool>& cancelled, std::function<void(double)> progress = {});
void validateReviewed(const juce::File& folder, juce::int64 frames, double rate, const juce::String& dryHash, const juce::String& wetHash, const std::atomic<bool>& cancelled, const juce::String& backingHash = {});
}
