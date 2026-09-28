#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/AudioBoxSetup.h"
#include <iostream>
class Probe final : public juce::AudioIODeviceCallback
{
public:
    std::atomic<int> callbacks {0};
    std::atomic<float> peak {0};
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
    void audioDeviceIOCallbackWithContext(const float* const* input, int inputs,
        float* const* output, int outputs, int samples, const juce::AudioIODeviceCallbackContext&) override
    {
        float maximum = peak.load();
        if (inputs > 0 && input[0])
            for (int i = 0; i < samples; ++i) maximum = juce::jmax(maximum, std::abs(input[0][i]));
        peak.store(maximum); callbacks.fetch_add(1);
        for (int c = 0; c < outputs; ++c) if (output[c]) juce::FloatVectorOperations::clear(output[c], samples);
    }
};
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioDeviceManager manager;
    const auto setup = findAudioBoxSetup(manager);
    if (!setup) { std::cerr << "AudioBox ASIO driver not found\n"; return 1; }
    const auto error = manager.initialise(1, 2, setup.get(), false);
    if (error.isNotEmpty()) { std::cerr << error << '\n'; return 2; }
    auto* device = manager.getCurrentAudioDevice();
    if (!device) return 3;
    std::cout << "Device: " << device->getName() << "\nType: " << device->getTypeName()
        << "\nSample rate: " << device->getCurrentSampleRate() << "\nBlock size: " << device->getCurrentBufferSizeSamples()
        << "\nInput latency samples: " << device->getInputLatencyInSamples()
        << "\nOutput latency samples: " << device->getOutputLatencyInSamples() << std::endl;
    Probe probe; manager.addAudioCallback(&probe);
    juce::Thread::sleep(3000);
    manager.removeAudioCallback(&probe);
    const auto xruns = manager.getXRunCount();
    manager.closeAudioDevice();
    std::cout << "Callbacks: " << probe.callbacks.load() << "\nInput peak: " << probe.peak.load()
        << "\nXruns: " << xruns << std::endl;
    return probe.callbacks.load() > 0 ? 0 : 4;
}
