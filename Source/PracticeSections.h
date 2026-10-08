#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <map>
#include <optional>

// Control/loader-thread storage, separate from tone rigs. Writers read the
// latest per-track document under a cross-process lock before changing one row.
class PracticeSections
{
public:
    explicit PracticeSections(juce::File directory = {}) : root(std::move(directory)) {}
    juce::var load(const juce::String& key, double duration);
    // Read-only validation shared by normal loading and archive recovery.
    // An unavailable external track permits structural validation only; its
    // actual duration is checked when the original audio is loaded later.
    static juce::var readDocument(const juce::File&, const juce::String& key,
                                 std::optional<double> duration = std::nullopt);
    juce::var change(const juce::String& key, double duration, const juce::String& id,
                     const juce::String& name, double a, double b, bool remove);
private:
    juce::var read(const juce::String& key, double duration);
    static void validate(const juce::var&, double duration);
    static void validateKey(const juce::String&);
    juce::File root;
    juce::CriticalSection local;
    std::map<juce::String, juce::var> memory;
};
