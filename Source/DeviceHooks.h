#pragma once
#include <juce_core/juce_core.h>
#include <functional>

// Set by the standalone app only: its audio device, so the editor can warn about
// dropouts (crackles) and offer a larger buffer. Called on the message thread. Kept
// apart from the processor so the app can reach it without the DSP headers.
struct StandaloneDeviceHooks
{
    virtual ~StandaloneDeviceHooks() = default;
    std::function<int()> deviceDropouts;
    std::function<juce::Array<int>()> deviceBufferSizes;
    std::function<juce::String(int)> setDeviceBufferSize;
};
