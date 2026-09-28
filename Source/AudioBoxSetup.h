#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
inline void selectAudioBoxGuitarInput(juce::AudioDeviceManager::AudioDeviceSetup& setup)
{
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    setup.inputChannels.setBit(0); // Guitar is connected to AudioBox input 1.
    setup.useDefaultOutputChannels = false;
    setup.outputChannels.clear();
    setup.outputChannels.setRange(0, 2, true); // Headphones use AudioBox outputs 1/2.
}
inline std::unique_ptr<juce::XmlElement> findAudioBoxSetup(juce::AudioDeviceManager& manager)
{
    for (auto* type : manager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() != "ASIO") continue;
        type->scanForDevices();
        for (const auto& name : type->getDeviceNames())
        {
            if (!name.containsIgnoreCase("AudioBox")) continue;
            auto setup = std::make_unique<juce::XmlElement>("DEVICESETUP");
            setup->setAttribute("deviceType", "ASIO");
            setup->setAttribute("audioDeviceName", name);
            setup->setAttribute("audioDeviceRate", 48000);
            setup->setAttribute("audioDeviceBufferSize", 128);
            setup->setAttribute("audioDeviceInChans", "1");
            setup->setAttribute("audioDeviceOutChans", "3");
            return setup;
        }
    }
    return {};
}
