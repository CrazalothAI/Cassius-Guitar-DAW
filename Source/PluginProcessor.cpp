#include "PluginProcessor.h"
#include "PluginEditor.h"

AmpSuiteAudioProcessor::AmpSuiteAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::mono(), true)
                                     .withInput("Backing track", juce::AudioChannelSet::stereo(), false)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      Thread("AmpSuite asset loader"), apvts(*this, nullptr, "AmpSuiteState", Params::layout())
{
    for (size_t i = 0; i < parameters.size(); ++i)
        parameters[i] = apvts.getRawParameterValue(Params::definitions[i].id);
    startThread();
}
AmpSuiteAudioProcessor::~AmpSuiteAudioProcessor() { signalThreadShouldExit(); notify(); stopThread(-1); }

bool AmpSuiteAudioProcessor::isBusesLayoutSupported(const BusesLayout& b) const
{
    const auto in = b.getMainInputChannelSet(), out = b.getMainOutputChannelSet();
    const auto backing = b.getChannelSet(true, 1);
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
        && (backing.isDisabled() || backing == juce::AudioChannelSet::stereo())
        && (out == juce::AudioChannelSet::stereo() || out == in);
}
void AmpSuiteAudioProcessor::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    const juce::ScopedLock lock(dspLock);
    rate = sampleRate; hostBlock = juce::jmax(1, maximumBlockSize);
    // A bounded internal quantum keeps model, convolution, and gate state
    // predictable even when a host delivers large or varying callbacks.
    maxBlock = juce::jmax(1, juce::jmin(hostBlock, 256));
    processLoad.reset(rate, maxBlock);
    clipHoldSamples = 0; inputClipped.store(false);
    const juce::dsp::ProcessSpec spec {rate, static_cast<juce::uint32>(maxBlock),
                                     static_cast<juce::uint32>(getTotalNumOutputChannels())};
    for (auto* gain : {&inputGain, &ampGain, &masterGain}) { gain->prepare(spec); gain->setRampDurationSeconds(0.02); }
    inputGain.setGainDecibels(value(Params::input)); ampGain.setGainDecibels(value(Params::ampOut));
    masterGain.setGainDecibels(value(Params::master));
    gate.prepare(rate);
    pitchTracker.prepare(rate);
    dynamicResonance.prepare(rate);
    piezo.prepare(rate);
    subSynth.prepare(rate);
    microDelay.prepare(rate);
    cab.prepare(spec); tone.prepare(rate);
    cleanAudio.resize(static_cast<size_t>(maxBlock));
    gateEnvelope.resize(static_cast<size_t>(maxBlock));
    pedalAudio.resize(static_cast<size_t>(maxBlock));
    subSynthAudio.resize(static_cast<size_t>(maxBlock));
    pedalBlend.reset(rate, 0.02); pedalBlend.setCurrentAndTargetValue(value(Params::pedalOn) >= .5f ? 1.0f : 0.0f);
    pedalFallbackBlend.reset(rate, 0.02); pedalFallbackBlend.setCurrentAndTargetValue(value(Params::pedalOn) >= .5f ? 1.0f : 0.0f);
    cleanLow = cleanHigh = metalLow = metalLow2 = 0;
    pedalLow = 0.0f;
    inputDcX1 = inputDcY1 = 0.0f;
    inputDcR = std::exp(-juce::MathConstants<float>::twoPi * 12.0f / static_cast<float>(rate));
    highCutState = {};
    cleanCompressor.prepare({rate, static_cast<juce::uint32>(maxBlock), 1}); cleanCompressor.reset();
    cleanCompressor.setThreshold(-20); cleanCompressor.setRatio(2.5f);
    cleanCompressor.setAttack(15); cleanCompressor.setRelease(140);
    compressionMix.reset(rate, 0.03); compressionMix.setCurrentAndTargetValue(value(Params::cleanComp) / 100);
    highCutoff.reset(rate, 0.03); highCutoff.setCurrentAndTargetValue(value(Params::highCut));
    stereoWidth.reset(rate, 0.05); stereoWidth.setCurrentAndTargetValue(value(Params::delayWidth) / 100);
    cleanBlend.reset(rate, 0.03); cleanBlend.setCurrentAndTargetValue(value(Params::clean) >= 0.5f ? 1.0f : 0.0f);
    tightCutoff.reset(rate, 0.03); tightCutoff.setCurrentAndTargetValue(value(Params::tight));
    delay.setMaximumDelayInSamples(static_cast<int>(rate * 1.5)); delay.prepare(spec); delay.reset();
    driveGain.reset(rate, 0.02); driveGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(value(Params::drive)));
    delayTime.reset(rate, 0.05); delayTime.setCurrentAndTargetValue(value(Params::delayTime) * static_cast<float>(rate) / 1000);
    delayMix.reset(rate, 0.02); delayMix.setCurrentAndTargetValue(value(Params::delayMix) / 100);
    reverb.prepare(spec); reverb.reset(); limiter.prepare(spec); limiter.setThreshold(-0.5f); limiter.setRelease(60);
    if (model) model->prepare(rate, maxBlock);
    if (pedal) pedal->prepare(rate, maxBlock);
    reportedRate.store(rate); reportedBlock.store(hostBlock);
    prePedalPeak.store(0); postPedalPeak.store(0); postAmpPeak.store(0); postCabPeak.store(0);
    swapBypasses.store(0);
    ampExpectedRate.store(model ? model->expectedRate() : 0);
    pedalExpectedRate.store(pedal ? pedal->expectedRate() : 0);
}
void AmpSuiteAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    auto mainBuffer = getBusBuffer(buffer, false, 0);
    auto backingBuffer = getBusBuffer(buffer, true, 1);
    // Asset replacement never makes the audio thread wait or destroy a model.
    // Pass the current input through during the short swap window instead of
    // clearing a block, which was audible as a click/static burst.
    const juce::ScopedTryLock lock(dspLock);
    if (!lock.isLocked())
    {
        if (mainBuffer.getNumChannels() > 1)
            mainBuffer.copyFrom(1, 0, mainBuffer, 0, 0, mainBuffer.getNumSamples());
        swapBypasses.fetch_add(1);
        outputPeak.store(mainBuffer.getMagnitude(0, 0, mainBuffer.getNumSamples()));
        return;
    }
    if (mainBuffer.getNumChannels() == 0 || mainBuffer.getNumSamples() == 0) return;
    const juce::AudioProcessLoadMeasurer::ScopedTimer timing(processLoad, mainBuffer.getNumSamples());
    inputPeak.store(mainBuffer.getMagnitude(0, 0, mainBuffer.getNumSamples()));
    clipHoldSamples = inputPeak.load() >= .995f ? static_cast<int>(rate)
        : juce::jmax(0, clipHoldSamples - mainBuffer.getNumSamples());
    inputClipped.store(clipHoldSamples > 0);
    for (int offset = 0; offset < mainBuffer.getNumSamples(); offset += maxBlock)
    {
        const int size = juce::jmin(maxBlock, mainBuffer.getNumSamples() - offset);
        float* channels[2] {mainBuffer.getWritePointer(0, offset), nullptr};
        if (mainBuffer.getNumChannels() > 1) channels[1] = mainBuffer.getWritePointer(1, offset);
        juce::AudioBuffer<float> chunk(channels, juce::jmin(2, mainBuffer.getNumChannels()), size);
        processChunk(chunk);
    }
    // Optional DAW backing-track bus is mixed after the amp chain, so it never
    // reaches the gate, pedal, NAM, or cabinet.
    if (backingBuffer.getNumChannels() > 0)
    {
        const auto samples = juce::jmin(mainBuffer.getNumSamples(), backingBuffer.getNumSamples());
        for (int i = 0; i < samples; ++i)
        {
            const auto left = backingBuffer.getSample(0, i);
            const auto right = backingBuffer.getNumChannels() > 1 ? backingBuffer.getSample(1, i) : left;
            mainBuffer.addSample(0, i, left);
            if (mainBuffer.getNumChannels() > 1) mainBuffer.addSample(1, i, right);
        }
    }
    outputPeak.store(mainBuffer.getMagnitude(0, 0, mainBuffer.getNumSamples()));
}
void AmpSuiteAudioProcessor::processChunk(juce::AudioBuffer<float>& buffer)
{
    // The AudioBox guitar is input 1. If a host exposes a stereo input bus,
    // ignore input 2 instead of averaging its noise into the high-gain chain.
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch) buffer.copyFrom(ch, 0, buffer, 0, 0, buffer.getNumSamples());
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    inputGain.setGainDecibels(value(Params::input)); inputGain.process(context);
    auto* mono = buffer.getWritePointer(0);

    // Audio interfaces can leave a small DC/subsonic component on the input.
    // Strip it before detection and distortion so it cannot repeatedly open
    // the gate or bias the NAM/tube stages into an unstable region.
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float x = mono[i];
        const float y = x - inputDcX1 + inputDcR * inputDcY1;
        inputDcX1 = x;
        inputDcY1 = y;
        mono[i] = y;
    }

    // 1. Real-time Pitch Tracking & Tuner
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        pitchTracker.processSample(mono[i]);
    const float trackedPitch = pitchTracker.getTrackedPitchHz();

    // 2. Tim Henson Acoustic Piezo Resonator
    piezo.configure(value(Params::piezoOn) >= 0.5f, value(Params::piezoBlend));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        mono[i] = piezo.processSample(mono[i]);

    // 3. Noise Gate & Intelligent "Chug" Attack Dynamics
    gate.configure(value(Params::gate), value(Params::gateRelease), value(Params::gateOn) >= 0.5f, value(Params::chugAttack));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float gain = gate.tick(buffer.getSample(0, i));
        gateEnvelope[static_cast<size_t>(i)] = gain;
        mono[i] *= gain;
    }
    gateLevel.store(gateEnvelope[static_cast<size_t>(buffer.getNumSamples() - 1)]);

    // 4. Thall Dynamic 200-400 Hz Resonance Suppression Notch
    dynamicResonance.configure(value(Params::dynResOn) >= 0.5f, value(Params::dynResAmount));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        mono[i] = dynamicResonance.processSample(mono[i]);

    // 5. Thall "Thicken" Sub-Octave Parallel Synthesizer
    subSynth.configure(value(Params::thickenOn) >= 0.5f, value(Params::thickenMix));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        subSynthAudio[static_cast<size_t>(i)] = subSynth.processSample(mono[i], trackedPitch);

    driveGain.setTargetValue(juce::Decibels::decibelsToGain(value(Params::drive)));
    cleanBlend.setTargetValue(value(Params::clean) >= 0.5f ? 1.0f : 0.0f);
    tightCutoff.setTargetValue(value(Params::tight));
    compressionMix.setTargetValue(value(Params::cleanComp) / 100);
    const float cleanHP = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 45.0f / static_cast<float>(rate));
    const float cleanLP = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 6500.0f / static_cast<float>(rate));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float gain = driveGain.getNextValue();
        // Independent clean preamp: gentle saturation and cabinet-like rolloff.
        // Keep it warm alongside the capture so channel changes can crossfade.
        cleanLow += cleanHP * (mono[i] - cleanLow);
        const float cleanDrive = 1.0f + std::log2(gain) * 0.15f;
        const float dryClean = mono[i] - cleanLow;
        const float compressed = cleanCompressor.processSample(0, dryClean) * 1.41254f;
        const float cleanSample = std::tanh((dryClean + compressionMix.getNextValue() * (compressed - dryClean)) * cleanDrive) / cleanDrive;
        cleanHigh += cleanLP * (cleanSample - cleanHigh);
        cleanAudio[static_cast<size_t>(i)] = cleanHigh;
        const float cutoff = tightCutoff.getNextValue();
        const float hp = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoff / static_cast<float>(rate));
        metalLow += hp * (mono[i] - metalLow);
        const float highPassed = mono[i] - metalLow;
        metalLow2 += hp * (highPassed - metalLow2);
        mono[i] += juce::jlimit(0.0f, 1.0f, (cutoff - 20.0f) / 10.0f) * (highPassed - metalLow2 - mono[i]);
        // A capture supplies the distortion. Drive pushes its input rather
        // than adding a second clipper. When no capture is loaded, keep the
        // metal channel useful with a real tube-like fallback instead of the
        // old effectively-linear path at 0 dB Drive.
        if (model)
            mono[i] *= gain;
        else
        {
            const float fallbackDrive = value(Params::clean) >= 0.5f
                ? 1.0f
                // A guitar pickup arrives much quieter than a captured amp's
                // calibrated test signal. Give the standalone fallback a
                // convincing power-stage push at its 0 dB starting point.
                : 2.8f + 0.36f * (gain - 1.0f);
            const float shaped = std::tanh(mono[i] * fallbackDrive);
            const float secondStage = std::tanh(shaped * 2.4f);
            const float blended = 0.84f * shaped + 0.16f * secondStage;
            mono[i] = blended / std::max(0.65f, std::tanh(fallbackDrive));
        }
    }
    prePedalPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    const bool pedalRequested = value(Params::pedalOn) >= .5f && value(Params::clean) < .5f;
    pedalBlend.setTargetValue(pedalRequested && pedal ? 1.0f : 0.0f);
    pedalFallbackBlend.setTargetValue(pedalRequested && !pedal ? 1.0f : 0.0f);
    if (pedal && (pedalBlend.isSmoothing() || pedalBlend.getTargetValue() > 0))
    {
        std::copy_n(mono, buffer.getNumSamples(), pedalAudio.data());
        pedal->process(pedalAudio.data(), buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            mono[i] += pedalBlend.getNextValue() * (pedalAudio[static_cast<size_t>(i)] - mono[i]);
    }
    else
    {
        pedalBlend.skip(buffer.getNumSamples());
        // Keep the metal path useful when a Fortin/TS NAM is not loaded yet.
        // This is a smooth, Tube-Screamer-style tightening stage: trim the
        // sub-bass, push the mids into a soft diode curve, and retain a little
        // dry signal so palm mutes do not become hollow. A loaded pedal NAM
        // takes over the same slot above.
        const float lowCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 120.0f / static_cast<float>(rate));
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            pedalLow += lowCoeff * (mono[i] - pedalLow);
            const float highPassed = mono[i] - pedalLow;
            const float diode = std::tanh(highPassed * 4.2f);
            // Keep the boost musical when it feeds a high-gain NAM. The
            // diode curve supplies the bite; the dry portion prevents the
            // combined pedal + capture gain from hard-clipping pick attacks.
            const float tightened = 0.48f * mono[i] + 0.58f * diode;
            mono[i] += pedalFallbackBlend.getNextValue() * (tightened - mono[i]);
        }
    }
    postPedalPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (model) model->process(mono, buffer.getNumSamples());
    postAmpPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch) buffer.copyFrom(ch, 0, mono, buffer.getNumSamples());
    cab.process(context);
    postCabPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float mix = cleanBlend.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto& sample = buffer.getWritePointer(ch)[i];
            sample += mix * (cleanAudio[static_cast<size_t>(i)] - sample);
        }
    }

    // Blend parallel sub-synthesis layer (bypasses pre-gain distortion)
    if (value(Params::thickenOn) >= 0.5f)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.getWritePointer(ch)[i] += subSynthAudio[static_cast<size_t>(i)];
    }

    ampGain.setGainDecibels(value(Params::ampOut)); ampGain.process(context);
    tone.update(value(Params::bass), value(Params::mid), value(Params::treble), value(Params::presence)); tone.process(buffer);
    highCutoff.setTargetValue(value(Params::highCut));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float cutoff = highCutoff.getNextValue();
        const float coefficient = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * juce::jmin(cutoff, static_cast<float>(rate) * 0.45f) / static_cast<float>(rate));
        const float amount = juce::jlimit(0.0f, 1.0f, (20000.0f - cutoff) / 1000.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto& sample = buffer.getWritePointer(ch)[i];
            auto& memory = highCutState[static_cast<size_t>(ch)];
            memory[0] += coefficient * (sample - memory[0]);
            memory[1] += coefficient * (memory[0] - memory[1]);
            sample += amount * (memory[1] - sample);
            // The input was already gated before the nonlinear stages. A
            // second full-depth multiplication here made every pick attack
            // amplitude-modulate the NAM output, which was heard as crackle.
            // Keep the post-amp noise suppression, but use a softer curve so
            // the gate cannot chop the attack into clicks.
            sample *= std::sqrt(gateEnvelope[static_cast<size_t>(i)]);
        }
    }
    delayTime.setTargetValue(value(Params::delayTime) * static_cast<float>(rate) / 1000);
    delayMix.setTargetValue(value(Params::delayMix) / 100);
    stereoWidth.setTargetValue(value(Params::delayWidth) / 100);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto time = delayTime.getNextValue(), mix = delayMix.getNextValue();
        const auto width = stereoWidth.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto& sample = buffer.getWritePointer(ch)[i];
            const float wet = delay.popSample(ch, time * (ch == 1 ? 1.0f + 0.25f * width : 1.0f));
            delay.pushSample(ch, sample + wet * 0.35f);
            sample = sample * (1 - mix * 0.5f) + wet * mix * 0.5f;
        }
    }
    juce::dsp::Reverb::Parameters rv;
    rv.roomSize = value(Params::reverbSize) / 100; rv.damping = 0.55f; rv.wetLevel = value(Params::reverbMix) / 100;
    rv.dryLevel = 1 - rv.wetLevel * 0.5f; reverb.setParameters(rv); reverb.process(context);

    // Sub-sample micro-timing delay for stereo double-tracking
    microDelay.configure(value(Params::microDelay));
    if (buffer.getNumChannels() >= 2)
        microDelay.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());

    masterGain.setGainDecibels(value(Params::master)); masterGain.process(context); limiter.process(context);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto& sample = buffer.getWritePointer(ch)[i];
            sample = std::isfinite(sample) ? juce::jlimit(-1.0f, 1.0f, sample) : 0.0f;
        }
}
void AmpSuiteAudioProcessor::requestFile(bool isModel, const juce::File& file)
{
    const juce::ScopedLock lock(requestLock);
    (isModel ? desiredModel : desiredIr) = file.getFullPathName();
    (isModel ? modelPending : irPending) = true;
    message = "Loading " + file.getFileName() + "...";
    notify();
}
void AmpSuiteAudioProcessor::run()
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    while (!threadShouldExit())
    {
        juce::String modelFile, irFile, pedalFile; bool doModel, doIr, doPedal;
        {
            const juce::ScopedLock lock(requestLock);
            doModel = modelPending; doIr = irPending;
            modelFile = desiredModel; irFile = desiredIr;
            doPedal = pedalPending; pedalFile = desiredPedal; pedalPending = false;
            modelPending = irPending = false;
        }
        for (const int stage : {0, 1, 2})
        {
            const bool isModel = stage == 0, isPedal = stage == 2, isNam = isModel || isPedal;
            if (!(isPedal ? doPedal : isModel ? doModel : doIr)) continue;
            const auto path = isPedal ? pedalFile : isModel ? modelFile : irFile;
            try
            {
                const juce::File file(path);
                if (path.isNotEmpty() && !file.existsAsFile()) throw std::runtime_error("File not found: " + path.toStdString());
                std::unique_ptr<NamWrapper> next;
                if (isNam && path.isNotEmpty()) next = std::make_unique<NamWrapper>(file);
                if (!isNam && path.isNotEmpty())
                {
                    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                    if (!reader || reader->lengthInSamples == 0 || reader->numChannels > 2
                        || reader->lengthInSamples > reader->sampleRate * 10)
                        throw std::runtime_error("Choose a mono/stereo WAV impulse response up to 10 seconds.");
                }
                {
                    const juce::ScopedLock lock(dspLock);
                    if (isNam)
                    {
                        if (next) next->prepare(rate, maxBlock);
                        (isPedal ? pedalExpectedRate : ampExpectedRate).store(next ? next->expectedRate() : 0);
                        (isPedal ? pedal : model).swap(next);
                    }
                    else if (path.isEmpty()) cab.clear(); else cab.load(file);
                }
                const juce::ScopedLock lock(requestLock);
                (isPedal ? pedalPath : isModel ? modelPath : irPath) = path;
                if (!message.startsWith("Load failed:")) message = path.isEmpty() ? "Stage cleared" : "Loaded " + file.getFileName();
            }
            catch (const std::exception& e)
            {
                const juce::ScopedLock lock(requestLock);
                message = "Load failed: " + juce::String(e.what());
            }
        }
        wait(50);
    }
}
juce::var AmpSuiteAudioProcessor::status()
{
    auto result = std::make_unique<juce::DynamicObject>();
    {
        const juce::ScopedLock lock(requestLock);
        result->setProperty("model", juce::File(modelPath).getFileName());
        result->setProperty("pedal", juce::File(pedalPath).getFileName());
        result->setProperty("ir", juce::File(irPath).getFileName()); result->setProperty("message", message);
    }
    {
        // UI polling must never hold the DSP lock: the callback would emit silence.
        const auto currentRate = reportedRate.load(), expectedAmp = ampExpectedRate.load(), expectedPedal = pedalExpectedRate.load();
        result->setProperty("sampleRate", currentRate);
        result->setProperty("bufferSize", reportedBlock.load());
        // NAM assets are resampled to the host rate when their capture rate
        // differs, so a mismatch is useful telemetry rather than a bypass.
        result->setProperty("ampExpectedRate", expectedAmp);
        result->setProperty("pedalExpectedRate", expectedPedal);
        result->setProperty("ampResampled", expectedAmp > 0 && std::abs(expectedAmp - currentRate) >= 1);
        result->setProperty("pedalResampled", expectedPedal > 0 && std::abs(expectedPedal - currentRate) >= 1);
    }
    result->setProperty("input", inputPeak.load()); result->setProperty("output", outputPeak.load());
    result->setProperty("prePedal", prePedalPeak.load());
    result->setProperty("postPedal", postPedalPeak.load());
    result->setProperty("postAmp", postAmpPeak.load());
    result->setProperty("postCab", postCabPeak.load());
    result->setProperty("pedalFallback", juce::File(pedalPath).getFullPathName().isEmpty());
    result->setProperty("swapBypasses", swapBypasses.load());
    result->setProperty("gate", gateLevel.load());
    result->setProperty("cpu", processLoad.getLoadAsPercentage());
    result->setProperty("overruns", processLoad.getXRunCount());
    result->setProperty("inputClipped", inputClipped.load());

    // Tuner & Real-time Thall DSP telemetry
    result->setProperty("tunerActive", pitchTracker.isNoteActive());
    result->setProperty("tunerNote", PitchTracker::midiNoteToName(pitchTracker.getDetectedMidiNote()));
    result->setProperty("tunerCents", pitchTracker.getDetectedCents());
    result->setProperty("tunerHz", pitchTracker.getDetectedHz());
    result->setProperty("dynResCut", dynamicResonance.getCurrentCutDb());

    return juce::var(result.release());
}
void AmpSuiteAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = apvts.copyState();
    // Preserve requested paths during asynchronous recall, including missing files.
    { const juce::ScopedLock lock(requestLock); state.setProperty("modelPath", desiredModel, nullptr); state.setProperty("irPath", desiredIr, nullptr); state.setProperty("pedalPath", desiredPedal, nullptr); }
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}
void AmpSuiteAudioProcessor::setStateInformation(const void* data, int size)
{
    const auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(apvts.state.getType())) return;
    const auto state = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(state);
    { const juce::ScopedLock lock(requestLock);
      desiredModel = state.getProperty("modelPath").toString(); desiredIr = state.getProperty("irPath").toString();
      desiredPedal = state.getProperty("pedalPath").toString();
      modelPending = irPending = pedalPending = true; message = "Restoring assets..."; }
    notify();
}
juce::AudioProcessorEditor* AmpSuiteAudioProcessor::createEditor() { return new AmpSuiteAudioProcessorEditor(*this); }

void AmpSuiteAudioProcessor::requestPedal(const juce::File& file)
{
    const juce::ScopedLock lock(requestLock);
    desiredPedal = file.getFullPathName(); pedalPending = true;
    message = "Loading " + file.getFileName() + "..."; notify();
}

bool AmpSuiteAudioProcessor::selectAmpVoice(const juce::String& voice)
{
    if (voice != "Blue-I" && voice != "Red-I") return false;
    juce::String current;
    { const juce::ScopedLock lock(requestLock); current = desiredModel; }
    const juce::File loaded(current);
    if (!loaded.getFileName().startsWith("APP-5153-Ivory-")) return false;
    const auto file = loaded.getSiblingFile("APP-5153-Ivory-" + voice + ".nam");
    if (!file.existsAsFile()) return false;
    requestFile(true, file);
    return true;
}
