#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/AudioInterfaceSetup.h"
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
int main(int argc, char** argv)
{
    const auto help = [] {
        std::cout << "Cassian interface diagnostic (silent outputs, no recording)\n"
            << "  --list                         List available driver types and device names\n"
            << "  --driver TYPE --device NAME    Choose a combined input/output interface\n"
            << "  --driver TYPE --input-device NAME --output-device NAME\n"
            << "                                 Choose separate input/output endpoints\n"
            << "  --input N --output N           Guitar input / first output channel (1-based)\n"
            << "  --outputs N                    Number of output channels (1 or 2)\n"
            << "  --rate HZ --buffer SAMPLES     Requested timing (default 48000 / 128)\n"
            << "With no arguments, use the sole installed ASIO interface; otherwise choose one explicitly.\n";
    };
    juce::String driver = "ASIO", inputName, outputName;
    bool list = false, explicitDevice = false; int inputChannel = 1, outputChannel = 1, outputs = 2, buffer = 128; double rate = 48000;
    for (int i = 1; i < argc; ++i) {
        const juce::String flag(argv[i]);
        if (flag == "--help" || flag == "-h") { help(); return 0; }
        if (flag == "--list") { list = true; continue; }
        if (flag != "--driver" && flag != "--device" && flag != "--input-device" && flag != "--output-device"
            && flag != "--input" && flag != "--output" && flag != "--outputs" && flag != "--rate" && flag != "--buffer") {
            std::cerr << "Unknown option: " << flag << '\n'; return 1;
        }
        if (++i >= argc) { std::cerr << "Missing value for " << flag << '\n'; return 1; }
        const juce::String value(argv[i]);
        if (flag == "--driver") { driver = value; explicitDevice = true; }
        else if (flag == "--device") { inputName = outputName = value; explicitDevice = true; }
        else if (flag == "--input-device") { inputName = value; explicitDevice = true; }
        else if (flag == "--output-device") { outputName = value; explicitDevice = true; }
        else {
            if (value.isEmpty() || !value.containsOnly(flag == "--rate" ? "0123456789." : "0123456789") || value.getDoubleValue() <= 0) {
                std::cerr << "Invalid value for " << flag << '\n'; return 1;
            }
            if (flag == "--input") inputChannel = value.getIntValue();
            if (flag == "--output") outputChannel = value.getIntValue();
            if (flag == "--outputs") outputs = value.getIntValue();
            if (flag == "--rate") rate = value.getDoubleValue();
            if (flag == "--buffer") buffer = value.getIntValue();
        }
    }
    if (inputChannel < 1 || inputChannel > 256 || outputChannel < 1 || outputChannel > 256
        || outputs < 1 || outputs > 2 || rate < 8000 || rate > 384000 || buffer < 16 || buffer > 65536) {
        std::cerr << "Channel or timing request is out of range\n"; return 1;
    }
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioDeviceManager manager;
    if (list) {
        for (auto* type : manager.getAvailableDeviceTypes()) {
            type->scanForDevices(); std::cout << "Driver: " << type->getTypeName() << '\n';
            for (const auto& name : type->getDeviceNames(true)) std::cout << "  Input: " << name << '\n';
            for (const auto& name : type->getDeviceNames(false)) std::cout << "  Output: " << name << '\n';
        }
        return 0;
    }
    auto setup = explicitDevice ? makeInterfaceSetup(driver, inputName, outputName, inputChannel - 1, outputChannel - 1, rate, buffer, outputs)
                                : findInterfaceSetup(manager);
    if (!setup || (explicitDevice && (inputName.isEmpty() || outputName.isEmpty()))) {
        std::cerr << "Choose an interface with --list, then --driver and --device (or separate input/output names).\n"; return 1;
    }
    // Apply channel/timing flags to an automatically selected interface as well.
    if (!explicitDevice) setup = makeInterfaceSetup("ASIO", setup->getStringAttribute("audioInputDeviceName"),
        setup->getStringAttribute("audioOutputDeviceName"), inputChannel - 1, outputChannel - 1, rate, buffer, outputs);
    const auto error = manager.initialise(1, outputs, setup.get(), false);
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
