#include "../Source/PluginProcessor.h"
#include "../Source/AudioBoxSetup.h"
#include <iostream>
#include <stdexcept>
#include <thread>
static void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static void set(AmpSuiteAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
struct Measurement { double rms, stereoDifference; };
static Measurement measure(float frequency, float amplitude,
    std::initializer_list<std::pair<const char*, float>> settings)
{
    AmpSuiteAudioProcessor p;
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
    return {std::sqrt(energy / count), std::sqrt(difference / count)};
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;
    try
    {
        juce::AudioDeviceManager::AudioDeviceSetup guitarSetup;
        guitarSetup.inputChannels.setRange(0, 2, true);
        guitarSetup.outputChannels.setRange(0, 2, true);
        guitarSetup.sampleRate = 48000; guitarSetup.bufferSize = 512;
        selectAudioBoxGuitarInput(guitarSetup);
        check(!guitarSetup.useDefaultInputChannels && guitarSetup.inputChannels.toInteger() == 1,
              "AudioBox must use only guitar input 1, excluding the unused preamp");
        check(guitarSetup.outputChannels.toInteger() == 3 && guitarSetup.sampleRate == 48000 && guitarSetup.bufferSize == 512,
              "Input correction must preserve stereo playback and device timing");
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
        AmpSuiteAudioProcessor processor;
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
        // The second AudioBox preamp is deliberately ignored. A hot signal
        // arriving there must not leak into the guitar path as crackle.
        set(processor, "GATE_ON", 0); set(processor, "REVERB_MIX", 0);
        processor.prepareToPlay(48000, 64);
        juce::AudioBuffer<float> unusedInput(2, 128);
        unusedInput.clear();
        for (int i = 0; i < unusedInput.getNumSamples(); ++i) unusedInput.setSample(1, i, 0.5f);
        processor.processBlock(unusedInput, midi);
        check(unusedInput.getMagnitude(0, 0, unusedInput.getNumSamples()) < 1e-6f,
              "Unused AudioBox input must not enter the amp chain");
        const auto meterStatus = processor.status();
        check(meterStatus.hasProperty("prePedal") && meterStatus.hasProperty("postAmp")
              && meterStatus.hasProperty("postCab"), "Stage meters must be exposed in status");
        const auto fallbackQuiet = measure(440, .03f, {{"AMP_CLEAN", 0}, {"DRIVE_GAIN", 0}}).rms;
        const auto fallbackLoud = measure(440, .30f, {{"AMP_CLEAN", 0}, {"DRIVE_GAIN", 0}}).rms;
        check(fallbackQuiet > .001f, "Metal fallback must produce audible output without a capture");
        check(fallbackLoud / fallbackQuiet < 8.0, "Metal fallback must compress and distort instead of remaining linear");
        const auto noPedal = measure(440, .10f, {{"AMP_CLEAN", 0}, {"PEDAL_ON", 0}, {"DRIVE_GAIN", 0}}).rms;
        const auto builtInPedal = measure(440, .10f, {{"AMP_CLEAN", 0}, {"PEDAL_ON", 1}, {"DRIVE_GAIN", 0}}).rms;
        check(builtInPedal > noPedal * 1.15, "Metal pedal switch must provide a built-in TS push without a capture");
        // A loaded NAM must not colour the clean channel. Compare the same
        // waveform through two complete processors, only one with a capture.
        check(argc > 1, "Pass a NAM fixture for the clean-channel regression");
        AmpSuiteAudioProcessor withCapture, withoutCapture;
        withCapture.requestFile(true, juce::File(argv[1]));
        bool loaded = false;
        for (int attempt = 0; attempt < 500; ++attempt)
        {
            if (withCapture.status().getProperty("model", {}).toString().isNotEmpty()) { loaded = true; break; }
            juce::Thread::sleep(10);
        }
        check(loaded, "NAM fixture must load");
        // With a capture, Drive is input gain, not an extra waveshaper.
        AmpSuiteAudioProcessor driveProcessor;
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
        const auto bassDry = measure(60, .1f, {{"TIGHT", 20}}).rms;
        const auto bassTight = measure(60, .1f, {{"TIGHT", 180}}).rms;
        check(bassTight < bassDry * .2, "Tight must remove low-frequency energy before the amp");
        const auto attackDry = measure(1000, .1f, {{"TIGHT", 20}}).rms;
        const auto attackTight = measure(1000, .1f, {{"TIGHT", 180}}).rms;
        check(attackTight > attackDry * .85, "Tight must retain the guitar's midrange attack");
        const auto bright = measure(12000, .1f, {{"HIGH_CUT", 20000}}).rms;
        const auto dark = measure(12000, .1f, {{"HIGH_CUT", 3000}}).rms;
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
        AmpSuiteAudioProcessor pollingProcessor;
        set(pollingProcessor, "GATE_ON", 0); set(pollingProcessor, "REVERB_MIX", 0);
        pollingProcessor.prepareToPlay(48000, 64);
        std::atomic<bool> stopPolling {false};
        std::thread poller([&] { while (!stopPolling.load()) { const auto snapshot = pollingProcessor.status(); (void) snapshot; } });
        bool silentBlock = false;
        juce::AudioBuffer<float> pollAudio(2, 64);
        for (int b = 0; b < 3000; ++b)
        {
            pollAudio.clear();
            for (int i = 0; i < 64; ++i)
                pollAudio.setSample(0, i, .1f * std::sin(juce::MathConstants<float>::twoPi * 220.0f * static_cast<float>(b * 64 + i) / 48000.0f));
            pollingProcessor.processBlock(pollAudio, midi);
            if (b > 20 && pollAudio.getMagnitude(0, 64) < .001f) silentBlock = true;
        }
        stopPolling.store(true); poller.join();
        check(!silentBlock, "Editor status polling must not interrupt audio");
        // Test PitchTracker directly
        PitchTracker tracker;
        tracker.prepare(48000);
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

        // Test Low-Tuned Dynamic Resonance Filter:
        const auto resOff = measure(280, 0.4f, {{"DYN_RES_ON", 0}}).rms;
        const auto resOn = measure(280, 0.4f, {{"DYN_RES_ON", 1}, {"DYN_RES_AMOUNT", 100}}).rms;
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
        std::cout << "Processor checks passed\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
