#pragma once
#include <juce_core/juce_core.h>
#include <functional>

// Set by the standalone app only: its audio device, so the editor can warn about
// dropouts (crackles) and offer a larger buffer. Called on the message thread. Kept
// apart from the processor so the app can reach it without the DSP headers.
struct StandaloneDeviceHooks
{
    virtual ~StandaloneDeviceHooks() = default;
    virtual void startToneRecovery() {}
    std::function<int()> deviceDropouts;
    std::function<juce::Array<int>()> deviceBufferSizes;
    std::function<juce::String(int)> setDeviceBufferSize;
    std::function<juce::StringArray()> deviceInputChannels;
    std::function<int()> deviceSelectedInput;
    std::function<juce::String(int)> setDeviceInputChannel;
    std::function<void()> showDeviceSettings;
    std::function<juce::var()> deviceSummary;
    std::function<juce::String(bool)> setInputMonitoring;
    std::function<juce::var()> midiInputs;
    std::function<juce::String(const juce::String&, bool)> setMidiInput;
};
