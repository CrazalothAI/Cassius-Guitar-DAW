#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "AudioBoxSetup.h"
#include "DeviceHooks.h"

class CassianApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Cassian"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String& commandLine) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Cassian";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        properties.setStorageParameters(options);
        auto* settings = properties.getUserSettings();
        if (!settings->containsKey("audioSetup"))
        {
            juce::AudioDeviceManager devices;
            if (auto setup = findAudioBoxSetup(devices))
            {
                settings->setValue("audioSetup", setup.get());
                settings->setValue("shouldMuteInput", false);
            }
        }
        window = std::make_unique<juce::StandaloneFilterWindow>("Cassian", juce::Colour(0xff101312), settings, false);
        // Saved stereo device settings can make JUCE negotiate stereo input and
        // mix the unused second preamp into the guitar. Keep this rig mono.
        auto& deviceManager = window->getDeviceManager();
        if (auto* audioDevice = deviceManager.getCurrentAudioDevice();
            audioDevice && audioDevice->getName().containsIgnoreCase("AudioBox"))
        {
            auto setup = deviceManager.getAudioDeviceSetup();
            selectAudioBoxGuitarInput(setup);
            const auto error = deviceManager.setAudioDeviceSetup(setup, true);
            if (error.isNotEmpty())
                window->getPluginHolder()->getMuteInputValue().setValue(true);
        }
        // Explicit launch-time rig import; ordinary launches recall the saved state.
        const auto args = juce::StringArray::fromTokens(commandLine, true);
        auto* processor = window->getAudioProcessor();
        if (processor && args.contains("--amp"))
        {
            juce::MemoryBlock current; processor->getStateInformation(current);
            if (auto xml = juce::AudioProcessor::getXmlFromBinary(current.getData(), static_cast<int>(current.getSize())))
            {
                bool importedAmp = false, importedPedal = false;
                for (int i = 0; i + 1 < args.size(); ++i)
                {
                    const auto flag = args[i];
                    if (flag != "--amp" && flag != "--cab" && flag != "--pedal") continue;
                    const auto path = args[++i].unquoted();
                    if (!juce::File::isAbsolutePath(path) || !juce::File(path).existsAsFile()) continue;
                    xml->setAttribute(flag == "--amp" ? "modelPath" : flag == "--cab" ? "irPath" : "pedalPath", path);
                    importedAmp |= flag == "--amp"; importedPedal |= flag == "--pedal";
                }
                if (importedAmp)
                {
                    juce::MemoryBlock next; juce::AudioProcessor::copyXmlToBinary(*xml, next);
                    processor->setStateInformation(next.getData(), static_cast<int>(next.getSize()));
                    const auto set = [processor](const juce::String& id, float value)
                    {
                        for (auto* parameter : processor->getParameters())
                            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter); ranged && ranged->paramID == id)
                                ranged->setValueNotifyingHost(ranged->convertTo0to1(value));
                    };
                    set("AMP_CLEAN", 0); set("PEDAL_ON", importedPedal ? 1.0f : 0.0f);
                    set("DRIVE_GAIN", 0); set("GATE_ON", 1); set("GATE_THRESH", -48); set("GATE_RELEASE", 100);
                    set("TIGHT", 70); set("HIGH_CUT", 8500); set("AMP_OUT", -3);
                    set("AMP_BASS", -1); set("AMP_MID", 1); set("AMP_TREBLE", 0); set("PRESENCE", 0);
                    set("DELAY_MIX", 0); set("REVERB_MIX", 4);
                }
            }
        }
        // Let the editor see dropouts on the audio device and change its buffer size.
        if (auto* cassian = dynamic_cast<StandaloneDeviceHooks*>(window->getAudioProcessor()))
        {
            auto& devices = window->getDeviceManager();
            cassian->deviceDropouts = [&devices] { auto* d = devices.getCurrentAudioDevice(); return d != nullptr ? d->getXRunCount() : -1; };
            cassian->deviceBufferSizes = [&devices] { auto* d = devices.getCurrentAudioDevice(); return d != nullptr ? d->getAvailableBufferSizes() : juce::Array<int> {}; };
            cassian->setDeviceBufferSize = [&devices](int size)
            {
                auto setup = devices.getAudioDeviceSetup();
                setup.bufferSize = size;
                return devices.setAudioDeviceSetup(setup, true);
            };
        }
        // Only automatically monitor a confirmed AudioBox device, never a fallback laptop microphone.
        auto* device = window->getDeviceManager().getCurrentAudioDevice();
        if (!device || !device->getName().containsIgnoreCase("AudioBox"))
            window->getPluginHolder()->getMuteInputValue().setValue(true);
        window->setVisible(true);
    }
    void shutdown() override
    {
        if (window) window->getPluginHolder()->savePluginState();
        window.reset(); properties.saveIfNeeded();
    }
    void systemRequestedQuit() override
    {
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay(100, [] { if (auto* app = juce::JUCEApplication::getInstance()) app->systemRequestedQuit(); });
        else quit();
    }
    void anotherInstanceStarted(const juce::String&) override { if (window) window->toFront(true); }
private:
    juce::ApplicationProperties properties;
    std::unique_ptr<juce::StandaloneFilterWindow> window;
};
JUCE_CREATE_APPLICATION_DEFINE(CassianApplication)
