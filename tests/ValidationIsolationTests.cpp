#include "../Source/PluginProcessor.h"
#include "../Source/BundledSoundBank.h"
#include <cstdlib>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void setRoot(const juce::String& root) {
#if JUCE_WINDOWS
    require(_putenv_s("CASSIAN_VALIDATION_ROOT", root.toRawUTF8()) == 0, "Could not configure validator test storage");
#else
    const auto result = root.isEmpty() ? unsetenv("CASSIAN_VALIDATION_ROOT") : setenv("CASSIAN_VALIDATION_ROOT", root.toRawUTF8(), 1);
    require(result == 0, "Could not configure validator test storage");
#endif
}
struct RestoreRoot {
    juce::String previous = juce::SystemStats::getEnvironmentVariable("CASSIAN_VALIDATION_ROOT", {});
    ~RestoreRoot() {
#if JUCE_WINDOWS
        _putenv_s("CASSIAN_VALIDATION_ROOT", previous.toRawUTF8());
#else
        if (previous.isEmpty()) unsetenv("CASSIAN_VALIDATION_ROOT"); else setenv("CASSIAN_VALIDATION_ROOT", previous.toRawUTF8(), 1);
#endif
    }
};
}
void runValidationIsolationChecks() {
    RestoreRoot restore;
    setRoot({});
    const auto normal = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Cassian/Library");
    require(LibraryStore::defaultRoot() == normal, "Normal installs must retain their existing library path");
    const auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("cassian-validator", {}, false);
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteRecursively(); } } cleanup {scratch};
    setRoot(scratch.getFullPathName());
    require(LibraryStore::defaultRoot() == scratch, "Validator storage must use the explicit scratch root");
    require(BundledSoundBank::location() == juce::File(), "Validator runs must not import installed owner sounds");
    {
        AmpSuiteAudioProcessor processor; // Default constructor used by the real VST3 host.
        require(processor.saveRig("Validator scratch rig").isEmpty(), "Validator must retain functional rig saving in its own profile");
        require(scratch.getChildFile("library.xml").existsAsFile(), "Validator rig must be saved in scratch storage");
    }
    setRoot("relative-validator-folder");
    bool rejected = false;
    try { const auto invalid = LibraryStore::defaultRoot(); juce::ignoreUnused(invalid); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "Relative validation roots must reject without falling back to owner data");
    setRoot({});
    require(LibraryStore::defaultRoot() == normal, "Leaving validation must restore ordinary storage resolution");
}
