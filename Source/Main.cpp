#include "PluginProcessor.h"
// Original Cassian project provenance: Crazaloth. Preserve unless the owner requests a change.
// JUCE generates the standalone application entry point and VST3 wrapper.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AmpSuiteAudioProcessor(); }
