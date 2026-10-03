#include "../Source/PluginProcessor.h"
#include "../Source/AudioInterfaceSetup.h"
#include <complex>
#include <iostream>
#include <stdexcept>
#include <thread>
static void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void runLibraryChecks(const juce::File& fixture);
void runQualityChecks(const juce::File& fixture);
void runSoundChecks(const juce::File& fixture);
static void set(AmpSuiteAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
struct Measurement { double rms, stereoDifference, preAmpPeak; };
static Measurement measure(float frequency, float amplitude,
    std::initializer_list<std::pair<const char*, float>> settings)
{
    AmpSuiteAudioProcessor p(false);
    set(p, "REVERB_MIX", 0); set(p, "GATE_THRESH", -80); set(p, "MASTER_VOL", -6);
    for (const auto& setting : settings) set(p, setting.first, setting.second);
    p.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> block(2, 128); juce::MidiBuffer midi;
    double energy = 0, difference = 0; int count = 0;
    for (int b = 0; b < 240; ++b)
    {
        block.clear();
        for (int i = 0; i < 128; ++i)
            block.setSample(0, i, amplitude * std::sin(juce::MathConstants<float>::twoPi * frequency * static_cast<float>(b * 128 + i) / 48000.0f));
        p.processBlock(block, midi);
        if (b < 120) continue;
        for (int i = 0; i < 128; ++i)
        {
            const double left = block.getSample(0, i), right = block.getSample(1, i);
            check(std::isfinite(left) && std::isfinite(right), "Tone shaping must remain finite");
            energy += left * left; difference += (left - right) * (left - right); ++count;
        }
    }
    return {std::sqrt(energy / count), std::sqrt(difference / count), static_cast<double>(p.status()["prePedal"])};
}
int main(int argc, char** argv)
{
    std::cout << std::unitbuf;
    juce::ScopedJuceInitialiser_GUI initialise;
    try
    {
        juce::AudioDeviceManager::AudioDeviceSetup guitarSetup;
        guitarSetup.inputChannels.setRange(0, 2, true);
        guitarSetup.outputChannels.setRange(0, 2, true);
        guitarSetup.sampleRate = 48000; guitarSetup.bufferSize = 512;
        selectGuitarChannels(guitarSetup);
        check(!guitarSetup.useDefaultInputChannels && guitarSetup.inputChannels.toInteger() == 1,
              "Initial setup must use only guitar input 1, excluding the unused preamp");
        check(guitarSetup.outputChannels.toInteger() == 3 && guitarSetup.sampleRate == 48000 && guitarSetup.bufferSize == 512,
              "Input correction must preserve stereo playback and device timing");
        selectGuitarChannels(guitarSetup, 3, 4, 1);
        check(guitarSetup.inputChannels.toInteger() == 8 && guitarSetup.outputChannels.toInteger() == 16,
              "Interface setup must support a different guitar input and mono output without changing timing");
        for (const auto& name : {"AudioBox ASIO", "Focusrite USB ASIO", "MOTU USB", "Generic interface"}) {
            auto requested = makeInterfaceSetup("ASIO", name, name, 3, 4);
            check(requested->getStringAttribute("audioDeviceInChans") == "1000" && requested->getStringAttribute("audioDeviceOutChans") == "110000",
                  "Saved channel masks must use JUCE's binary format for arbitrary physical channels");
            guitarSetup.inputDeviceName = name; guitarSetup.outputDeviceName = name;
            check(matchesRequestedInterface(requested.get(), guitarSetup, "ASIO"), "Monitoring must not depend on the interface manufacturer");
            check(!matchesRequestedInterface(requested.get(), guitarSetup, "Windows Audio"), "A fallback driver must not inherit automatic monitoring");
            guitarSetup.inputDeviceName = "Built-in microphone";
            check(!matchesRequestedInterface(requested.get(), guitarSetup, "ASIO"), "A fallback microphone must not inherit monitoring");
        }
        auto requested = makeInterfaceSetup("Windows Audio", "USB input", "USB output");
        guitarSetup.inputDeviceName = "USB input"; guitarSetup.outputDeviceName = "USB output";
        check(matchesRequestedInterface(requested.get(), guitarSetup, "Windows Audio"), "Explicit non-ASIO interfaces must retain monitoring");
        requested->setAttribute("audioDeviceName", "Legacy interface");
        guitarSetup.inputDeviceName = guitarSetup.outputDeviceName = "Legacy interface";
        check(matchesRequestedInterface(requested.get(), guitarSetup, "Windows Audio"), "Older combined-name device settings must remain supported");
        check(!matchesRequestedInterface(nullptr, guitarSetup, "Windows Audio"), "An unselected default device must remain unconfirmed");
        check(chooseSingleAsioInterface({"Generic USB ASIO"}) != nullptr, "One ASIO interface can configure automatically regardless of its name");
        check(chooseSingleAsioInterface({}) == nullptr && chooseSingleAsioInterface({"Interface A", "Interface B"}) == nullptr,
              "Absent or ambiguous ASIO devices require explicit selection");
        GuitarGate gateTest;
        gateTest.prepare(48000); gateTest.configure(-48, 80, true);
        float gateGain = 0;
        for (int i = 0; i < 4800; ++i) gateGain = gateTest.tick(.0001f);
        check(gateGain == 0, "Idle noise must not open the gate");
        for (int i = 0; i < 480; ++i) gateGain = gateTest.tick(.1f);
        check(gateGain > .99f, "Gate must open quickly for a picked note");
        for (int i = 0; i < 4800; ++i) gateGain = gateTest.tick(.0025f);
        check(gateGain > .99f, "Hysteresis must retain notes below the opening threshold");
        for (int i = 0; i < 24000; ++i) gateGain = gateTest.tick(0);
        check(gateGain == 0, "Gate must close fully after a note ends");
        gateTest.configure(-48, 80, false);
        for (int i = 0; i < 480; ++i) gateGain = gateTest.tick(0);
        check(gateGain > .99f, "Gate bypass must pass audio");
        AmpSuiteAudioProcessor processor(false);
        set(processor, "REVERB_MIX", 0); set(processor, "GATE_THRESH", -80);
        set(processor, "MASTER_VOL", -6);
        processor.prepareToPlay(48000, 64);
        juce::AudioBuffer<float> buffer(2, 257); // Deliberately larger than prepared block.
        juce::MidiBuffer midi;
        for (int block = 0; block < 20; ++block)
        {
            buffer.clear();
            for (int i = 0; i < buffer.getNumSamples(); ++i) buffer.setSample(0, i, 0.1f * std::sin(static_cast<float>(i + block * 257) * 0.05f));
            processor.processBlock(buffer, midi);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                check(std::isfinite(buffer.getSample(0, i)), "Output must be finite");
                check(std::abs(buffer.getSample(0, i)) <= 1, "Output must be bounded");
                check(std::abs(buffer.getSample(0, i) - buffer.getSample(1, i)) < 1e-5f, "Mono input must reach both outputs");
            }
        }
        check(buffer.getMagnitude(0, 257) > 0.001f, "Bypassed amp must pass audio");
        // The second input channel is deliberately ignored. A hot signal
        // arriving there must not leak into the guitar path as crackle.
        set(processor, "GATE_ON", 0); set(processor, "REVERB_MIX", 0);
        processor.prepareToPlay(48000, 64);
        juce::AudioBuffer<float> unusedInput(2, 128);
        unusedInput.clear();
        for (int i = 0; i < unusedInput.getNumSamples(); ++i) unusedInput.setSample(1, i, 0.5f);
        processor.processBlock(unusedInput, midi);
        check(unusedInput.getMagnitude(0, 0, unusedInput.getNumSamples()) < 1e-6f,
              "Unused input must not enter the amp chain");
        const auto meterStatus = processor.status();
        check(meterStatus.hasProperty("prePedal") && meterStatus.hasProperty("postAmp")
              && meterStatus.hasProperty("postCab"), "Stage meters must be exposed in status");
        const auto fallbackQuiet = measure(440, .03f, {{"AMP_CLEAN", 0}, {"DRIVE_GAIN", 0}}).rms;
        const auto fallbackLoud = measure(440, .30f, {{"AMP_CLEAN", 0}, {"DRIVE_GAIN", 0}}).rms;
        check(fallbackQuiet > .001f, "Metal fallback must produce audible output without a capture");
        check(fallbackLoud / fallbackQuiet < 2.0, "Metal fallback must saturate like a high-gain amp: 20 dB more input, under 6 dB more output");
        // A loaded NAM must not colour the clean channel. Compare the same
        // waveform through two complete processors, only one with a capture.
        check(argc > 1, "Pass a NAM fixture for the clean-channel regression");
        AmpSuiteAudioProcessor withCapture(false), withoutCapture(false);
        withCapture.requestFile(true, juce::File(argv[1]));
        bool loaded = false;
        for (int attempt = 0; attempt < 500; ++attempt)
        {
            if (withCapture.status().getProperty("model", {}).toString().isNotEmpty()) { loaded = true; break; }
            juce::Thread::sleep(10);
        }
        check(loaded, "NAM fixture must load");
        // With a capture, Drive is input gain, not an extra waveshaper.
        AmpSuiteAudioProcessor driveProcessor(false);
        driveProcessor.requestFile(true, juce::File(argv[1]));
        for (int attempt = 0; attempt < 500 && driveProcessor.status().getProperty("model", {}).toString().isEmpty(); ++attempt) juce::Thread::sleep(10);
        check(driveProcessor.status().getProperty("model", {}).toString().isNotEmpty(), "Drive test capture must load");
        // A capture recorded at a different rate should be rendered through
        // the rate converter instead of being silently bypassed.
        set(driveProcessor, "GATE_ON", 0); set(driveProcessor, "REVERB_MIX", 0);
        driveProcessor.prepareToPlay(44100, 128);
        juce::AudioBuffer<float> converted(2, 128);
        float convertedPeak = 0;
        for (int b = 0; b < 80; ++b)
        {
            converted.clear();
            for (int i = 0; i < converted.getNumSamples(); ++i)
                converted.setSample(0, i, .06f * std::sin(static_cast<float>(b * 128 + i) * .031f));
            driveProcessor.processBlock(converted, midi);
            convertedPeak = std::max(convertedPeak, converted.getMagnitude(0, 0, converted.getNumSamples()));
        }
        const auto convertedStatus = driveProcessor.status();
        check(convertedPeak > .0001f, "Mismatched-rate capture must still produce audio");
        check(convertedStatus.getProperty("message", {}).toString().indexOfIgnoreCase("bypassed") < 0,
              "Mismatched-rate capture must not be reported as bypassed");
        const auto renderDriven = [&](float inputDb, float driveDb)
        {
            set(driveProcessor, "INPUT_GAIN", inputDb); set(driveProcessor, "DRIVE_GAIN", driveDb);
            set(driveProcessor, "GATE_ON", 0); set(driveProcessor, "REVERB_MIX", 0);
            driveProcessor.prepareToPlay(48000, 128);
            std::vector<float> rendered;
            juce::AudioBuffer<float> audio(2, 128);
            for (int b = 0; b < 200; ++b)
            {
                audio.clear();
                for (int i = 0; i < 128; ++i)
                    audio.setSample(0, i, .03f * std::sin(static_cast<float>(b * 128 + i) * .0288f));
                driveProcessor.processBlock(audio, midi);
                // Input gain ramps on prepare; allow the capture's receptive
                // field to flush that startup transient before comparing.
                if (b >= 150)
                    rendered.insert(rendered.end(), audio.getReadPointer(0), audio.getReadPointer(0) + 128);
            }
            return rendered;
        };
        const auto driven = renderDriven(0, 12), calibrated = renderDriven(12, 0);
        for (size_t i = 0; i < driven.size(); ++i)
            check(std::abs(driven[i] - calibrated[i]) < 1e-5f,
                  "Capture Drive must match equivalent input gain without extra clipping");
        set(driveProcessor, "INPUT_GAIN", 0); set(driveProcessor, "DRIVE_GAIN", 0); set(driveProcessor, "GATE_ON", 1);
        if (argc > 3)
        {
            withCapture.requestPedal(juce::File(argv[2]));
            withCapture.requestFile(false, juce::File(argv[3]));
            bool rigLoaded = false;
            for (int attempt = 0; attempt < 1000; ++attempt)
            {
                const auto rig = withCapture.status();
                if (rig.getProperty("pedal", {}).toString().isNotEmpty() && rig.getProperty("ir", {}).toString().isNotEmpty()) { rigLoaded = true; break; }
                juce::Thread::sleep(10);
            }
            check(rigLoaded, "Pedal and cabinet must load");
            set(withCapture, "PEDAL_ON", 1);
            juce::MemoryBlock rigState; withCapture.getStateInformation(rigState);
            const auto rigXml = juce::AudioProcessor::getXmlFromBinary(rigState.getData(), static_cast<int>(rigState.getSize()));
            check(rigXml && rigXml->getStringAttribute("pedalPath") == juce::String(argv[2]), "Rig state must preserve pedal path");
        }
        for (auto* p : {&withCapture, &withoutCapture})
        {
            set(*p, "AMP_CLEAN", 1); set(*p, "GATE_THRESH", -80);
            set(*p, "REVERB_MIX", 0); set(*p, "MASTER_VOL", -6);
            p->prepareToPlay(48000, 64);
        }
        juce::AudioBuffer<float> captured(2, 257), direct(2, 257);
        for (int block = 0; block < 20; ++block)
        {
            captured.clear(); direct.clear();
            for (int i = 0; i < 257; ++i)
            {
                const float sample = 0.15f * std::sin(static_cast<float>(i + block * 257) * 0.0288f);
                captured.setSample(0, i, sample); direct.setSample(0, i, sample);
            }
            withCapture.processBlock(captured, midi); withoutCapture.processBlock(direct, midi);
            for (int i = 0; i < 257; ++i)
                check(std::abs(captured.getSample(0, i) - direct.getSample(0, i)) < 1e-5f, "Clean must bypass NAM coloration");
        }
        check(direct.getMagnitude(0, 257) > 0.01f, "Clean must produce audio without a capture");
        set(withCapture, "AMP_CLEAN", 0); set(withCapture, "GATE_THRESH", -48);
        withCapture.prepareToPlay(48000, 64);
        for (int b = 0; b < 100; ++b)
        {
            captured.clear();
            for (int i = 0; i < 257; ++i) captured.setSample(0, i, .00001f * std::sin(static_cast<float>(i + b * 257)));
            withCapture.processBlock(captured, midi);
            check(captured.getMagnitude(0, 257) < 1e-7f, "Post-amp gate must silence idle NAM output");
        }
        float rigPeak = 0;
        for (int b = 0; b < 200; ++b)
        {
            captured.clear();
            for (int i = 0; i < 257; ++i) captured.setSample(0, i, .1f * std::sin(static_cast<float>(i + b * 257) * .0288f));
            withCapture.processBlock(captured, midi);
            rigPeak = std::max(rigPeak, captured.getMagnitude(0, 257));
            for (int i = 0; i < 257; ++i) check(std::isfinite(captured.getSample(0, i)) && std::abs(captured.getSample(0, i)) <= 1, "Full rig must remain finite and bounded");
        }
        check(rigPeak > .0001f, "Full rig must pass played notes");
        std::cout << "Rig output peak: " << rigPeak << '\n';
        // Measured entering the amp: the built-in high-gain amp re-saturates its input.
        const auto bassDry = measure(60, .1f, {{"TIGHT", 20}}).preAmpPeak;
        const auto bassTight = measure(60, .1f, {{"TIGHT", 180}}).preAmpPeak;
        check(bassTight < bassDry * .2, "Tight must remove low-frequency energy before the amp");
        const auto attackDry = measure(1000, .1f, {{"TIGHT", 20}}).preAmpPeak;
        const auto attackTight = measure(1000, .1f, {{"TIGHT", 180}}).preAmpPeak;
        check(attackTight > attackDry * .85, "Tight must retain the guitar's midrange attack");
        // High cut follows the amp on both channels; the clean channel keeps the test linear
        // (the metal channel band-limits 12 kHz before the amp to keep hiss out of the distortion).
        const auto bright = measure(12000, .1f, {{"AMP_CLEAN", 1}, {"HIGH_CUT", 20000}}).rms;
        const auto dark = measure(12000, .1f, {{"AMP_CLEAN", 1}, {"HIGH_CUT", 3000}}).rms;
        check(dark < bright * .25, "High cut must attenuate treble energy");
        const auto softDry = measure(220, .03f, {{"AMP_CLEAN", 1}, {"CLEAN_COMP", 0}}).rms;
        const auto hardDry = measure(220, .6f, {{"AMP_CLEAN", 1}, {"CLEAN_COMP", 0}}).rms;
        const auto softComp = measure(220, .03f, {{"AMP_CLEAN", 1}, {"CLEAN_COMP", 100}}).rms;
        const auto hardComp = measure(220, .6f, {{"AMP_CLEAN", 1}, {"CLEAN_COMP", 100}}).rms;
        check(hardComp / softComp < (hardDry / softDry) * .65, "Clean compression must control dynamics");
        const auto narrow = measure(440, .1f, {{"DELAY_TIME", 40}, {"DELAY_MIX", 80}, {"DELAY_WIDTH", 0}});
        const auto wide = measure(440, .1f, {{"DELAY_TIME", 40}, {"DELAY_MIX", 80}, {"DELAY_WIDTH", 100}});
        check(narrow.stereoDifference < 1e-6, "Zero width must keep mono delay centered");
        check(wide.stereoDifference > .001, "Width must produce distinct left and right repeats");
        // Status polling runs on the editor thread while the callback renders.
        // It must never force a silent block by taking the DSP swap lock.
        AmpSuiteAudioProcessor pollingProcessor(false);
        set(pollingProcessor, "GATE_ON", 0); set(pollingProcessor, "REVERB_MIX", 0);
        pollingProcessor.prepareToPlay(48000, 64);
        std::atomic<bool> stopPolling {false};
        std::thread poller([&] { while (!stopPolling.load()) { const auto snapshot = pollingProcessor.status(); (void) snapshot; } });
        bool silentBlock = false;
        juce::AudioBuffer<float> pollAudio(2, 64);
        for (int b = 0; b < 3000; ++b)
        {
            pollAudio.clear();
            // A played tone: the amp blocks DC, as coupling capacitors do.
            for (int i = 0; i < 64; ++i) pollAudio.setSample(0, i, .1f * std::sin(juce::MathConstants<float>::twoPi * 220.0f * static_cast<float>(b * 64 + i) / 48000.0f));
            pollingProcessor.processBlock(pollAudio, midi);
            if (b > 20 && pollAudio.getMagnitude(0, 64) < .001f) silentBlock = true;
        }
        stopPolling.store(true); poller.join();
        check(!silentBlock, "Editor status polling must not interrupt audio");
        // Test PitchTracker directly
        PitchTracker tracker;
        tracker.prepare(48000);
        tracker.setActive(true);
        for (int i = 0; i < 4800; ++i)
        {
            const float sample = 0.2f * std::sin(juce::MathConstants<float>::twoPi * 82.41f * static_cast<float>(i) / 48000.0f);
            tracker.processSample(sample);
        }
        std::cout << "Tracker active: " << tracker.isNoteActive() << " Hz: " << tracker.getDetectedHz()
                  << " Midi: " << tracker.getDetectedMidiNote() << " Name: " << PitchTracker::midiNoteToName(tracker.getDetectedMidiNote()) << '\n';
        check(tracker.isNoteActive(), "Tuner must detect active guitar note");
        check(tracker.getDetectedMidiNote() == 40, "82.4 Hz must identify as MIDI note 40 (E2)");
        check(PitchTracker::midiNoteToName(tracker.getDetectedMidiNote()) == "E2", "Tuner must name E2");
        check(std::abs(tracker.getDetectedCents()) < 6.0f, "Tuner cents detune must be close to zero");

        // Test Dynamic Resonance Filter:
        // The notch sits ahead of the amp; measure it there, before saturation.
        const auto resOff = measure(280, 0.4f, {{"DYN_RES_ON", 0}}).preAmpPeak;
        const auto resOn = measure(280, 0.4f, {{"DYN_RES_ON", 1}, {"DYN_RES_AMOUNT", 100}}).preAmpPeak;
        check(resOn < resOff * 0.85, "Dynamic resonance must carve 280 Hz chugs under heavy energy");

        // Test MicroDelay stereo phase difference
        const auto microNarrow = measure(440, 0.1f, {{"DELAY_MIX", 0}, {"MICRO_DELAY", 0.0f}}).stereoDifference;
        const auto microSpread = measure(440, 0.1f, {{"DELAY_MIX", 0}, {"MICRO_DELAY", 0.5f}}).stereoDifference;
        check(microNarrow < 1e-6, "Zero micro-delay must leave mono stereo difference at zero");
        check(microSpread > 0.001, "Micro-delay must produce sub-sample phase separation");

        set(processor, "AMP_CLEAN", 1);
        set(processor, "TIGHT", 85);
        set(processor, "INPUT_GAIN", 7.5f);
        set(processor, "DYN_RES_ON", 1);
        set(processor, "THICKEN_ON", 1);
        set(processor, "PIEZO_ON", 1);
        set(processor, "MICRO_DELAY", 0.65f);
        juce::MemoryBlock state; processor.getStateInformation(state);
        set(processor, "INPUT_GAIN", -20);
        set(processor, "AMP_CLEAN", 0);
        set(processor, "DYN_RES_ON", 0);
        set(processor, "THICKEN_ON", 0);
        set(processor, "PIEZO_ON", 0);
        set(processor, "MICRO_DELAY", 0.0f);
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        check(std::abs(processor.apvts.getRawParameterValue("INPUT_GAIN")->load() - 7.5f) < 0.02f, "State must restore parameters");
        check(processor.apvts.getRawParameterValue("AMP_CLEAN")->load() == 1, "State must restore clean channel");
        check(processor.apvts.getRawParameterValue("TIGHT")->load() == 85, "State must restore low cut");
        check(processor.apvts.getRawParameterValue("DYN_RES_ON")->load() == 1, "State must restore dynamic resonance");
        check(processor.apvts.getRawParameterValue("THICKEN_ON")->load() == 1, "State must restore thicken sub");
        check(processor.apvts.getRawParameterValue("PIEZO_ON")->load() == 1, "State must restore piezo simulation");
        check(std::abs(processor.apvts.getRawParameterValue("MICRO_DELAY")->load() - 0.65f) < 0.02f, "State must restore micro-delay");
        const char invalid[] = "invalid";
        processor.setStateInformation(invalid, sizeof(invalid));
        check(std::abs(processor.apvts.getRawParameterValue("INPUT_GAIN")->load() - 7.5f) < 0.02f, "Invalid state must be ignored");
        // Streaming resampling: a 44.1 <-> 48 kHz round trip in irregular blocks returns
        // exactly as many samples as it was given, with no drift or discontinuity.
        {
            StreamResampler up, down;
            up.prepare(44100, 48000, 600); down.prepare(48000, 44100, 600); down.pushSilence(16);
            std::vector<float> in(44100), out, mid(700), back(600);
            for (size_t i = 0; i < in.size(); ++i) in[i] = .5f * std::sin(juce::MathConstants<float>::twoPi * 1000.0f * static_cast<float>(i) / 44100.0f);
            for (size_t offset = 0, n = 1; offset < in.size(); offset += n, n = 1 + (n * 37 + 11) % 256)
            {
                n = std::min(n, in.size() - offset);
                up.push(in.data() + offset, static_cast<int>(n));
                const int produced = up.available(); up.pull(mid.data(), produced); down.push(mid.data(), produced);
                check(down.available() >= static_cast<int>(n), "Resampler must always deliver a full block");
                down.pull(back.data(), static_cast<int>(n)); out.insert(out.end(), back.begin(), back.begin() + static_cast<std::ptrdiff_t>(n));
            }
            check(out.size() == in.size(), "Resampler must neither drop nor add samples");
            int latency = 0; double bestError = 1e9;
            for (int lag = 0; lag < 40; ++lag)
            {
                double error = 0;
                for (size_t i = 2000; i < 40000; ++i) error = std::max(error, static_cast<double>(std::abs(out[i] - in[i - static_cast<size_t>(lag)])));
                if (error < bestError) { bestError = error; latency = lag; }
            }
            std::cout << "Resampler round trip: latency " << latency << " samples, max error " << bestError << '\n';
            check(bestError < .01, "Resampled audio must match the input after a fixed latency");
        }
        // Capture loudness is matched to -18 dB; amp-only metadata enables the built-in speaker.
        {
            const auto dir = juce::File(argv[1]).getParentDirectory();
            NamWrapper quiet {dir.getChildFile("lstm.nam")}, reference {juce::File(argv[1])};
            check(quiet.hasLoudness() && std::abs(quiet.levelMatchDb() - (-18.0 - quiet.loudness())) < 1e-6, "Quiet captures must be raised to the target loudness");
            check(quiet.levelMatchDb() > 15 && reference.levelMatchDb() < 5, "Level matching must follow each capture's loudness");
            check(quiet.cabinetIsKnown() && !quiet.hasCabinet(), "Amp-only gear type must be recognised");
            AmpSuiteAudioProcessor rig(false); rig.prepareToPlay(48000, 128);
            rig.requestFile(true, juce::File(argv[1]));
            for (int t = 0; t < 200 && !rig.status()["message"].toString().startsWith("Loaded"); ++t) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            juce::AudioBuffer<float> audio(2, 128);
            for (int b = 0; b < 4; ++b) { audio.clear(); audio.setSample(0, 0, .1f); rig.processBlock(audio, midi); }
            const auto rigStatus = rig.status();
            check(static_cast<bool>(rigStatus["ampLevelled"]) && static_cast<bool>(rigStatus["speakerSim"]), "An amp-only capture without an IR must use the built-in speaker");
            check(!static_cast<bool>(rigStatus["fallbackAmp"]), "A loaded capture replaces the built-in amp");
            // Removing the capture returns the metal channel to the built-in amp.
            rig.requestFile(true, juce::File());
            for (int t = 0; t < 200 && rig.status()["message"].toString() != "Stage cleared"; ++t) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            for (int b = 0; b < 2; ++b) { audio.clear(); rig.processBlock(audio, midi); }
            check(rig.status()["model"].toString().isEmpty() && static_cast<bool>(rig.status()["fallbackAmp"]), "Removing a capture must restore the built-in amp");
        }
        // Wide ranges put their musical centre at mid-travel; switches are real on/off parameters.
        {
            AmpSuiteAudioProcessor ranges(false);
            const std::pair<const char*, float> centres[] {{"HIGH_CUT", 8000}, {"TIGHT", 70}, {"DELAY_TIME", 300}, {"GATE_RELEASE", 150}};
            for (const auto& [id, centre] : centres)
                check(std::abs(ranges.apvts.getParameter(id)->convertFrom0to1(0.5f) - centre) < 0.5f, "Skewed range must centre on its musical value");
            for (const auto* id : {"AMP_CLEAN", "GATE_ON", "PEDAL_ON", "DYN_RES_ON", "THICKEN_ON", "PIEZO_ON"})
                check(dynamic_cast<juce::AudioParameterBool*>(ranges.apvts.getParameter(id)) != nullptr, "Switches must be on/off parameters");
            check(dynamic_cast<juce::AudioParameterBool*>(ranges.apvts.getParameter("MICRO_DELAY")) == nullptr, "A 0-1 ms control is not a switch");
            // Sessions saved before the switch/skew change still restore their values.
            const auto legacy = juce::ValueTree::fromXml(R"(<AmpSuiteState><PARAM id="GATE_ON" value="0.0"/><PARAM id="HIGH_CUT" value="6500.0"/><PARAM id="AMP_CLEAN" value="1.0"/></AmpSuiteState>)");
            juce::MemoryBlock saved; juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), saved);
            ranges.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
            check(ranges.apvts.getRawParameterValue("GATE_ON")->load() == 0 && ranges.apvts.getRawParameterValue("AMP_CLEAN")->load() == 1, "Saved switch states must restore");
            check(std::abs(ranges.apvts.getRawParameterValue("HIGH_CUT")->load() - 6500) < 1, "Saved skewed values must restore");
        }
        // The noise shield closes on hiss near the gate threshold, stays open for loud
        // playing, and leaves only the fixed band-limit when the gate is switched off.
        {
            auto hissThrough = [](float detectorLevel, bool dynamic) {
                NoiseShield shield; shield.prepare(48000);
                juce::Random random(11); std::vector<float> x(256), detector(256, detectorLevel);
                double energy = 0;
                for (int b = 0; b < 400; ++b)
                {
                    for (auto& v : x) v = .01f * (random.nextFloat() * 2 - 1);
                    shield.process(x.data(), detector.data(), 256, -48, dynamic);
                    if (b >= 200) for (auto v : x) energy += v * v;
                }
                return energy;
            };
            const auto loud = hissThrough(1.0f, true), decaying = hissThrough(.005f, true), raw = hissThrough(.005f, false);
            check(decaying < loud * .5, "Hiss near the gate threshold must be filtered ahead of the amp");
            check(std::abs(raw / loud - 1) < .05, "With the gate off the shield must not react to level");
        }
        // Mains hum is learned between notes and cancelled under them, on either mains
        // frequency; a DI without hum passes through untouched.
        {
            struct Run { double residual, hum; bool cancelling; float mains; bool exact; };
            const auto run = [](double mainsHz, float humLevel) {
                HumCanceller canceller; canceller.prepare(48000);
                juce::Random random(5); std::vector<float> x(128), in(128);
                // Hum left in the output: its energy at the mains harmonics over the last
                // second, while a 110 Hz note rings (Hann-windowed, so the note leaks nothing).
                std::vector<std::complex<double>> before(30), after(30);
                long long n = 0; bool exact = true;
                for (int b = 0; b < 48000 * 5 / 128; ++b)
                {
                    for (int i = 0; i < 128; ++i)
                    {
                        const double t = static_cast<double>(n + i) / 48000;
                        float buzz = 0;
                        for (int k = 1; k <= 30; ++k)
                            buzz += humLevel / static_cast<float>(k) * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * mainsHz * k * t + k));
                        // Three seconds of quiet strings, then a held note.
                        in[i] = x[i] = (t > 3 ? .2f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 110 * t)) : 0.0f)
                                     + 1e-4f * (random.nextFloat() * 2 - 1) + buzz;
                    }
                    canceller.process(x.data(), 128);
                    for (int i = 0; i < 128; ++i, ++n)
                    {
                        if (x[i] != in[i]) exact = false;
                        if (n < 48000 * 4) continue;
                        const double t = static_cast<double>(n) / 48000, w = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * (t - 4));
                        for (int k = 1; k <= 30; ++k)
                        {
                            const auto phasor = std::polar(w, -juce::MathConstants<double>::twoPi * mainsHz * k * t);
                            before[static_cast<size_t>(k - 1)] += static_cast<double>(in[i]) * phasor;
                            after[static_cast<size_t>(k - 1)] += static_cast<double>(x[i]) * phasor;
                        }
                    }
                }
                double residual = 0, hum = 0;
                for (int k = 0; k < 30; ++k) { residual += std::norm(after[static_cast<size_t>(k)]); hum += std::norm(before[static_cast<size_t>(k)]); }
                return Run {residual, hum, canceller.cancelling(), canceller.mainsHz(), exact};
            };
            const auto sixty = run(60, .003f), fifty = run(50, .003f), none = run(60, 0);
            check(sixty.cancelling && sixty.residual < sixty.hum * .01, "60 Hz hum must be cancelled by 20 dB under a note");
            check(fifty.cancelling && fifty.mains == 50 && fifty.residual < fifty.hum * .01, "50 Hz mains must be found and cancelled");
            check(!none.cancelling && none.exact, "Without hum the DI must pass through untouched");
        }
        // The metronome keeps its own time standalone and follows a playing host's grid.
        {
            Metronome click; click.prepare(48000);
            const Metronome::Settings settings {true, 120, 3, -6};
            juce::AudioBuffer<float> audio(2, 64), one(2, 1);
            // One sample at a time over 1.9 s, so each click's sample is exact.
            std::vector<int> beats; std::vector<long long> onsets; int seen = click.clickCount();
            for (long long n = 0; n < 48000 * 19 / 10; ++n)
            {
                one.clear();
                click.process(one.getArrayOfWritePointers(), 2, 1, settings, {});
                if (click.clickCount() != seen) { seen = click.clickCount(); beats.push_back(click.currentBeat()); onsets.push_back(n); }
            }
            check(beats == std::vector<int> {0, 1, 2, 0}, "120 BPM in 3 must click on 0, 0.5, 1 and 1.5 s with the bar accented");
            check(onsets.size() == 4 && onsets[0] == 0 && std::abs(onsets[1] - 24000) <= 1 && std::abs(onsets[3] - 72000) <= 1, "Clicks must land on the beat");
            // A host at 90 BPM, half a beat before bar 2 of a 4/4 song.
            Metronome synced; synced.prepare(48000);
            juce::AudioPlayHead::PositionInfo host;
            host.setIsPlaying(true); host.setBpm(90); host.setTimeSignature(juce::AudioPlayHead::TimeSignature {4, 4});
            long long first = -1, n = 0;
            for (int b = 0; b < 400 && first < 0; ++b, n += 64)
            {
                const double ppq = 3.5 + static_cast<double>(n) * 90.0 / 60.0 / 48000.0;
                host.setPpqPosition(ppq); host.setPpqPositionOfLastBarStart(ppq < 4 ? 0.0 : 4.0);
                audio.clear();
                synced.process(audio.getArrayOfWritePointers(), 2, 64, settings, host);
                for (int i = 0; i < 64 && first < 0; ++i) if (audio.getSample(0, i) != 0) first = n + i;
            }
            check(std::abs(first - 16000) <= 1 && synced.currentBeat() == 0, "A playing host's downbeat must click on time and accented");
            Metronome off; off.prepare(48000); audio.clear();
            off.process(audio.getArrayOfWritePointers(), 2, 64, {false, 120, 4, 0}, {});
            check(audio.getMagnitude(0, 64) == 0, "A metronome that is off adds nothing");
        }
        // Master controls the whole mix, including a click on an idle high-gain rig.
        {
            const auto clickPeak = [&](float master) {
                AmpSuiteAudioProcessor rig(false);
                set(rig, "REVERB_MIX", 0); set(rig, "METRO_ON", 1); set(rig, "METRO_LEVEL", -12);
                set(rig, "DRIVE_GAIN", 24); set(rig, "MASTER_VOL", master);
                rig.prepareToPlay(48000, 128);
                juce::AudioBuffer<float> audio(2, 128); float peak = 0;
                for (int b = 0; b < 24; ++b) { audio.clear(); rig.processBlock(audio, midi); peak = std::max(peak, audio.getMagnitude(0, 0, 128)); }
                return peak;
            };
            const float normal = clickPeak(-6), quiet = clickPeak(-26);
            std::cout << "Metronome master ratio: " << normal / quiet << '\n';
            check(normal > .01f && std::abs(normal / quiet - 10) < .05f, "Turning Master down 20 dB must turn the metronome down 20 dB too");
        }
        // A mono guitar + stereo backing input shares a channel with the output.
        // The backing must survive intact and bypass both clean and high-gain rigs.
        {
            AmpSuiteAudioProcessor cleanRig(false), metalRig(false);
            for (auto* rig : {&cleanRig, &metalRig})
            {
                auto layout = rig->getBusesLayout(); layout.inputBuses.set(1, juce::AudioChannelSet::stereo());
                check(rig->setBusesLayout(layout), "Stereo backing bus must be supported");
                set(*rig, "REVERB_MIX", 100); set(*rig, "DRIVE_GAIN", 24); set(*rig, "MASTER_VOL", -12);
            }
            set(cleanRig, "AMP_CLEAN", 1); set(metalRig, "AMP_CLEAN", 0);
            cleanRig.prepareToPlay(48000, 64); metalRig.prepareToPlay(48000, 64);
            juce::AudioBuffer<float> a(3, 257), b(3, 257);
            float peak = 0;
            for (int block = 0; block < 24; ++block)
            {
                a.clear(); b.clear();
                for (int i = 0; i < 257; ++i) {
                    const float x = .08f * std::sin(static_cast<float>(block * 257 + i) * .05f);
                    a.setSample(1, i, x); a.setSample(2, i, 2 * x);
                    b.setSample(1, i, x); b.setSample(2, i, 2 * x);
                }
                cleanRig.processBlock(a, midi); metalRig.processBlock(b, midi);
                peak = std::max(peak, a.getMagnitude(0, 0, 257));
                for (int i = 0; i < 257; ++i) {
                    check(std::abs(a.getSample(1, i) - 2 * a.getSample(0, i)) < 1e-6f, "Backing track stereo balance must survive output overlap");
                    check(std::abs(a.getSample(0, i) - b.getSample(0, i)) < 1e-6f, "Backing audio must not enter the distortion or effects");
                }
            }
            check(peak > .01f, "The backing bus's left channel must not be overwritten by the guitar rig");
        }
        // Stopping or changing the click level during a beep must not chop its waveform.
        {
            Metronome click; click.prepare(48000);
            juce::AudioBuffer<float> audio(2, 512); audio.clear();
            click.process(audio.getArrayOfWritePointers(), 2, 512, {true, 120, 4, -3}, {});
            const float last = audio.getSample(0, 511);
            audio.clear(); click.process(audio.getArrayOfWritePointers(), 2, 512, {false, 120, 4, -3}, {});
            check(std::abs(audio.getSample(0, 0) - last) < .08f, "Turning off the metronome must fade its current click");
            audio.clear(); click.process(audio.getArrayOfWritePointers(), 2, 512, {false, 120, 4, -3}, {});
            check(audio.getMagnitude(0, 0, 512) == 0, "The stopped metronome must settle to silence");
            click.prepare(48000); audio.clear();
            click.process(audio.getArrayOfWritePointers(), 2, 512, {true, 120, 4, -3}, {});
            const float before = audio.getSample(0, 511);
            audio.clear(); click.process(audio.getArrayOfWritePointers(), 2, 512, {true, 120, 4, -40}, {});
            check(std::abs(audio.getSample(0, 0) - before) < .08f, "Metronome level changes must ramp instead of stepping");
        }
        // Measured EQ response, stereo isolation, bypass, and automation continuity.
        {
            const auto rms = [](double rate, float frequency, PedalEq::Settings settings) {
                PedalEq eq; eq.prepare(rate, settings);
                juce::AudioBuffer<float> audio(2, 257); double energy = 0; int samples = 0;
                for (int b = 0; b < 80; ++b) {
                    audio.clear();
                    for (int i = 0; i < 257; ++i)
                        audio.setSample(0, i, .03f * std::sin(juce::MathConstants<float>::twoPi * frequency * static_cast<float>(b * 257 + i) / static_cast<float>(rate)));
                    eq.process(audio);
                    check(audio.getMagnitude(1, 0, 257) == 0, "EQ must not leak signal between stereo channels");
                    if (b < 40) continue;
                    for (int i = 0; i < 257; ++i) { const float x = audio.getSample(0, i); check(std::isfinite(x), "EQ output must be finite"); energy += x * x; ++samples; }
                }
                return std::sqrt(energy / samples);
            };
            for (const double sampleRate : {44100.0, 48000.0, 96000.0}) {
                const PedalEq::Settings flat {true, 0, 0, 0, 0};
                check(rms(sampleRate, 80, {true, -6, 0, 0, 0}) < rms(sampleRate, 80, flat) * .7, "Body EQ must reduce booming lows");
                check(rms(sampleRate, 350, {true, 0, -6, 0, 0}) < rms(sampleRate, 350, flat) * .55, "Mud EQ must cut low mids by 6 dB at its centre");
                check(rms(sampleRate, 1200, {true, 0, 0, 6, 0}) > rms(sampleRate, 1200, flat) * 1.85, "Focus EQ must lift note definition");
                check(rms(sampleRate, 10000, {true, 0, 0, 0, -6}) < rms(sampleRate, 10000, flat) * .6, "Fizz EQ must tame upper-treble noise");
                check(rms(sampleRate, 1000, {true, 0, 0, 0, -6}) > rms(sampleRate, 1000, flat) * .9, "Fizz cuts must preserve the note's midrange");
            }
            PedalEq eq; eq.prepare(48000, {false, -12, -12, 12, -12});
            juce::AudioBuffer<float> audio(2, 257), original(2, 257);
            juce::Random random(3);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 257; ++i) audio.setSample(ch, i, .03f * (random.nextFloat() * 2 - 1));
            original.makeCopyOf(audio); eq.process(audio);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 257; ++i)
                check(audio.getSample(ch, i) == original.getSample(ch, i), "Bypassed EQ must leave cleans unchanged bit for bit");
            eq.prepare(48000, {true, 0, 0, -12, 0});
            float previous = 0, largestStep = 0;
            for (int b = 0; b < 60; ++b) {
                if (b == 20) eq.configure({true, 0, 0, 12, 0});
                if (b == 40) eq.configure({false, 0, 0, 12, 0});
                for (int i = 0; i < 257; ++i) audio.setSample(0, i, .03f * std::sin(static_cast<float>(b * 257 + i) * .15f));
                eq.process(audio);
                for (int i = 0; i < 257; ++i) { const float x = audio.getSample(0, i); largestStep = std::max(largestStep, std::abs(x - previous)); previous = x; }
            }
            check(largestStep < .025f, "EQ changes and bypass must not produce gain-step clicks");
            AmpSuiteAudioProcessor restored(false);
            set(restored, "EQ_ON", 1); set(restored, "EQ_FIZZ", -4);
            juce::MemoryBlock saved; restored.getStateInformation(saved);
            set(restored, "EQ_ON", 0); set(restored, "EQ_FIZZ", 0); restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
            check(restored.apvts.getRawParameterValue("EQ_ON")->load() == 1 && restored.apvts.getRawParameterValue("EQ_FIZZ")->load() == -4, "Sessions must recall the EQ pedal");
            const auto old = juce::ValueTree::fromXml(R"(<AmpSuiteState><PARAM id="AMP_CLEAN" value="1"/></AmpSuiteState>)");
            juce::AudioProcessor::copyXmlToBinary(*old.createXml(), saved); restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
            check(restored.apvts.getRawParameterValue("EQ_ON")->load() == 0 && restored.apvts.getRawParameterValue("EQ_FIZZ")->load() == 0, "Older sessions must restore without the new EQ colouring them");
        }
        // The pick attack shaper never steps the gain: the boost ramps up and back down.
        {
            GuitarGate gate; gate.prepare(48000); gate.configure(-60, 140, true, 100);
            float largestStep = 0, previous = 1, peak = 1;
            for (int n = 0; n < 48000; ++n)
            {
                const float t = static_cast<float>(n % 9600) / 48000; // a pick every 200 ms
                const float x = .3f * std::exp(-t * 30) * std::sin(juce::MathConstants<float>::twoPi * 110 * t)
                              + (t < .002f ? .2f * std::sin(juce::MathConstants<float>::twoPi * 5000 * t) : 0.0f);
                gate.tick(x);
                largestStep = std::max(largestStep, std::abs(gate.attackGain() - previous));
                previous = gate.attackGain(); peak = std::max(peak, previous);
            }
            check(peak > 1.2f, "Pick attack must still lift the attack");
            // It rises over about a millisecond and a half; the old shaper jumped 0.65 in one sample.
            check(largestStep < .05f, "Pick attack must not step the gain (a click in front of the amp)");
        }
        // The pitch tracker only runs while the tuner is open or Thicken needs it.
        {
            AmpSuiteAudioProcessor tunerRig(false); set(tunerRig, "GATE_ON", 0); tunerRig.prepareToPlay(48000, 128);
            juce::AudioBuffer<float> audio(2, 128);
            auto play = [&] { for (int b = 0; b < 60; ++b) { audio.clear();
                for (int i = 0; i < 128; ++i) audio.setSample(0, i, .2f * std::sin(juce::MathConstants<float>::twoPi * 110.0f * static_cast<float>(b * 128 + i) / 48000.0f));
                tunerRig.processBlock(audio, midi); } };
            play();
            check(!static_cast<bool>(tunerRig.status()["tunerActive"]), "The tuner must stay idle while closed");
            tunerRig.setTunerActive(true); play();
            check(tunerRig.status()["tunerNote"].toString() == "A2", "The tuner must track once opened");
        }
        // The processor fixture must come from NAM's example_models directory
        // (the loudness checks also require its sibling lstm.nam). An optional
        // fourth path exercises the foundation with a real user amp capture.
        runLibraryChecks(juce::File(argc > 4 ? argv[4] : argv[1]));
        runQualityChecks(juce::File(argv[1]));
        runSoundChecks(juce::File(argv[1]));
        std::cout << "Processor checks passed\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
