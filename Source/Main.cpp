#include "PluginProcessor.h"
// JUCE generates the standalone application entry point and VST3 wrapper.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AmpSuiteAudioProcessor(); }
