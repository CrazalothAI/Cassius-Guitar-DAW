#pragma once
#include <juce_audio_devices/juce_audio_devices.h>

// Physical channels are zero-based here; the device settings UI labels them from 1.
inline void selectGuitarChannels(juce::AudioDeviceManager::AudioDeviceSetup& setup,
                                int input = 0, int firstOutput = 0, int outputs = 2)
{
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear(); setup.inputChannels.setBit(juce::jmax(0, input));
    setup.useDefaultOutputChannels = false;
    setup.outputChannels.clear(); setup.outputChannels.setRange(juce::jmax(0, firstOutput), juce::jlimit(1, 2, outputs), true);
}

inline std::unique_ptr<juce::XmlElement> makeInterfaceSetup(const juce::String& driver,
    const juce::String& inputDevice, const juce::String& outputDevice, int input = 0, int firstOutput = 0,
    double rate = 48000, int buffer = 128, int outputs = 2)
{
    juce::AudioDeviceManager::AudioDeviceSetup channels; selectGuitarChannels(channels, input, firstOutput, outputs);
    auto setup = std::make_unique<juce::XmlElement>("DEVICESETUP");
    setup->setAttribute("deviceType", driver);
    setup->setAttribute("audioInputDeviceName", inputDevice);
    setup->setAttribute("audioOutputDeviceName", outputDevice);
    setup->setAttribute("audioDeviceRate", rate); setup->setAttribute("audioDeviceBufferSize", buffer);
    setup->setAttribute("audioDeviceInChans", channels.inputChannels.toString(2));
    setup->setAttribute("audioDeviceOutChans", channels.outputChannels.toString(2));
    return setup;
}

// One ASIO choice is unambiguous. Multiple installed drivers need an explicit
// selection; never favor a manufacturer or guess which connected interface to use.
inline std::unique_ptr<juce::XmlElement> chooseSingleAsioInterface(const juce::StringArray& duplex)
{
    return duplex.size() == 1 ? makeInterfaceSetup("ASIO", duplex[0], duplex[0]) : nullptr;
}
inline std::unique_ptr<juce::XmlElement> findInterfaceSetup(juce::AudioDeviceManager& manager)
{
    juce::StringArray duplex;
    for (auto* type : manager.getAvailableDeviceTypes()) {
        if (type->getTypeName() != "ASIO") continue;
        type->scanForDevices();
        const auto inputs = type->getDeviceNames(true);
        for (const auto& name : type->getDeviceNames(false))
            if (inputs.contains(name)) duplex.addIfNotAlreadyThere(name);
    }
    return chooseSingleAsioInterface(duplex);
}

inline bool matchesRequestedInterface(const juce::XmlElement* requested,
    const juce::AudioDeviceManager::AudioDeviceSetup& actual, const juce::String& driver)
{
    if (!requested || requested->getStringAttribute("deviceType") != driver) return false;
    // JUCE also accepts older state files using one combined device-name attribute.
    const auto combined = requested->getStringAttribute("audioDeviceName");
    const auto input = combined.isNotEmpty() ? combined : requested->getStringAttribute("audioInputDeviceName");
    const auto output = combined.isNotEmpty() ? combined : requested->getStringAttribute("audioOutputDeviceName");
    return input.isNotEmpty() && output.isNotEmpty() && input == actual.inputDeviceName && output == actual.outputDeviceName;
}
