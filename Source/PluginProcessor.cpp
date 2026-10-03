#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdlib>

static juce::String readRig(const juce::var& rig, juce::ValueTree& state)
{
    if (static_cast<int>(rig["schema"]) != 1 || !rig["state"].isString() || rig["state"].toString().length() > 4 * 1024 * 1024)
        return "Unsupported Cassian rig format.";
    auto xml = juce::XmlDocument::parse(rig["state"].toString());
    if (!xml || !xml->hasTagName("AmpSuiteState")) return "Invalid rig state.";
    state = juce::ValueTree::fromXml(*xml);
    juce::StringArray ids;
    for (const auto& child : state)
        if (child.hasType("PARAM")) {
            const auto id = child["id"].toString();
            if (ids.contains(id)) return "Duplicate rig parameter.";
            ids.add(id);
        }
    for (const auto& definition : Params::definitions)
    {
        const auto parameter = state.getChildWithProperty("id", definition.id);
        if (!parameter.hasType("PARAM")) return "Incomplete rig: " + juce::String(definition.id);
        const auto text = parameter["value"].toString().toStdString(); char* end = nullptr;
        const double amount = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0' || !std::isfinite(amount) || amount < definition.min || amount > definition.max)
            return "Invalid rig parameter: " + juce::String(definition.id);
        if ((juce::String(definition.id) == "AMP_SOURCE" || juce::String(definition.id) == "CAPTURE_KIND" || juce::String(definition.id) == "CAB_MODE") && amount != std::floor(amount))
            return "Invalid rig routing choice.";
    }
    return {};
}

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
    fallbackAmp.prepare(rate, maxBlock); speaker.prepare(rate); noiseShield.prepare(rate); humCanceller.prepare(rate); metronome.prepare(rate);
    cab.prepare(spec); tone.prepare(rate);
    pedalEq.prepare(rate, {value(Params::eqOn) >= .5f, value(Params::eqBody), value(Params::eqMud), value(Params::eqFocus), value(Params::eqFizz)});
    backingAudio.setSize(2, maxBlock);
    cleanAudio.resize(static_cast<size_t>(maxBlock));
    gateEnvelope.resize(static_cast<size_t>(maxBlock));
    pedalAudio.resize(static_cast<size_t>(maxBlock));
    subSynthAudio.resize(static_cast<size_t>(maxBlock));
    dryInput.resize(static_cast<size_t>(maxBlock));
    pedalBlend.reset(rate, 0.02); pedalBlend.setCurrentAndTargetValue(value(Params::pedalOn) >= .5f ? 1.0f : 0.0f);
    cleanLow = cleanHigh = metalLow = metalLow2 = 0;
    tightCoeffHz = highCoeffHz = cleanDriveGain = -1;
    highCutState = {};
    cleanCompressor.prepare({rate, static_cast<juce::uint32>(maxBlock), 1}); cleanCompressor.reset();
    cleanCompressor.setThreshold(-20); cleanCompressor.setRatio(2.5f);
    cleanCompressor.setAttack(15); cleanCompressor.setRelease(140);
    compressionMix.reset(rate, 0.03); compressionMix.setCurrentAndTargetValue(value(Params::cleanComp) / 100);
    highCutoff.reset(rate, 0.03); highCutoff.setCurrentAndTargetValue(value(Params::highCut));
    stereoWidth.reset(rate, 0.05); stereoWidth.setCurrentAndTargetValue(value(Params::delayWidth) / 100);
    cleanBlend.reset(rate, 0.03); cleanBlend.setCurrentAndTargetValue(value(Params::clean) >= 0.5f ? 1.0f : 0.0f);
    activeAmpSource = juce::roundToInt(value(Params::ampSource));
    ampSlotGain.reset(rate, .02); ampSlotGain.setCurrentAndTargetValue(1);
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
    postEqPeak.store(0);
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
    // Models are prepared and IRs built off the lock, so it is held only for a
    // pointer swap; on the rare contention, output silence rather than the raw
    // DI, which bypassed the whole rig at full interface level.
    const juce::ScopedTryLock lock(dspLock);
    if (!lock.isLocked())
    {
        buffer.clear();
        swapBypasses.fetch_add(1);
        outputPeak.store(0);
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
        // The mono guitar input plus a stereo auxiliary bus overlaps the stereo
        // output's second channel. Save the backing input before the rig writes it.
        const int backingChannels = backingBuffer.getNumChannels();
        if (backingChannels > 0)
            for (int ch = 0; ch < chunk.getNumChannels(); ++ch)
                backingAudio.copyFrom(ch, 0, backingBuffer, juce::jmin(ch, backingChannels - 1), offset, size);
        processChunk(chunk);
        // Backing audio bypasses all guitar processing, including the EQ and effects.
        if (backingChannels > 0)
            for (int ch = 0; ch < chunk.getNumChannels(); ++ch)
                chunk.addFrom(ch, 0, backingAudio, ch, 0, size);
    }
    // The click joins after everything, so the rig never processes it.
    {
        const auto position = getPlayHead() != nullptr ? getPlayHead()->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo> {};
        const bool follows = position && position->getIsPlaying() && position->getBpm() && position->getPpqPosition();
        metronomeFollowsHost.store(follows);
        metronomeBpm.store(follows ? *position->getBpm() : static_cast<double>(value(Params::metroBpm)));
        metronome.process(mainBuffer.getArrayOfWritePointers(), mainBuffer.getNumChannels(), mainBuffer.getNumSamples(),
            {value(Params::metroOn) >= 0.5f, value(Params::metroBpm), juce::roundToInt(value(Params::metroBeats)), value(Params::metroLevel)}, position);
    }
    // Protect the complete mix. Previously the click bypassed Master and the
    // limiter, and guitar + click + backing audio was only hard-clipped.
    juce::dsp::AudioBlock<float> outputBlock(mainBuffer);
    juce::dsp::ProcessContextReplacing<float> outputContext(outputBlock);
    masterGain.setGainDecibels(value(Params::master)); masterGain.process(outputContext);
    // Remove invalid input before it can poison the limiter's envelope state.
    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        for (int i = 0; i < mainBuffer.getNumSamples(); ++i)
            if (!std::isfinite(mainBuffer.getSample(ch, i))) mainBuffer.setSample(ch, i, 0);
    limiter.process(outputContext);
    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        juce::FloatVectorOperations::clip(mainBuffer.getWritePointer(ch), mainBuffer.getReadPointer(ch), -1.0f, 1.0f, mainBuffer.getNumSamples());
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
    const int requestedSource = juce::roundToInt(value(Params::ampSource));
    // Fade to silence before changing algorithms; backing/click are unaffected.
    if (requestedSource != activeAmpSource)
    {
        ampSlotGain.setTargetValue(0);
        if (!ampSlotGain.isSmoothing() && ampSlotGain.getCurrentValue() == 0)
        {
            activeAmpSource = requestedSource;
            cleanBlend.setCurrentAndTargetValue(value(Params::clean) >= .5f ? 1.0f : 0.0f);
            ampSlotGain.setTargetValue(1);
        }
    }
    else ampSlotGain.setTargetValue(1);
    // Mains hum out first, so neither the gate nor the amp ever sees it.
    if (activeAmpSource != 4) humCanceller.process(mono, buffer.getNumSamples());

    // 1. Pitch tracking for the tuner and Thicken. Idle otherwise: its analysis
    //    used to overrun the callback several times a second, heard as crackle.
    const bool thicken = value(Params::thickenOn) >= 0.5f;
    pitchTracker.setActive(tunerRequested.load(std::memory_order_relaxed) || thicken);
    if (pitchTracker.isActive())
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            pitchTracker.processSample(mono[i]);
    const float trackedPitch = pitchTracker.getTrackedPitchHz();

    // 2. Tim Henson Acoustic Piezo Resonator
    piezo.configure(value(Params::piezoOn) >= 0.5f, value(Params::piezoBlend));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        mono[i] = piezo.processSample(mono[i]);

    // 3. Noise Gate & Intelligent "Chug" Attack Dynamics
    gate.configure(value(Params::gate), value(Params::gateRelease), value(Params::gateOn) >= 0.5f, value(Params::chugAttack));
    std::copy_n(mono, buffer.getNumSamples(), dryInput.data());
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float gain = gate.tick(buffer.getSample(0, i));
        gateEnvelope[static_cast<size_t>(i)] = gain;
        mono[i] *= gain;
    }
    gateLevel.store(gateEnvelope[static_cast<size_t>(buffer.getNumSamples() - 1)]);

    // 4. Dynamic 200-400 Hz Resonance Suppression Notch
    dynamicResonance.configure(value(Params::dynResOn) >= 0.5f, value(Params::dynResAmount));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        mono[i] = dynamicResonance.processSample(mono[i]);

    // 5. "Thicken" Sub-Octave Parallel Synthesizer
    subSynth.configure(thicken, value(Params::thickenMix));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        subSynthAudio[static_cast<size_t>(i)] = subSynth.processSample(mono[i], trackedPitch);

    if (activeAmpSource > 0) processUniversalAmp(buffer);
    else
    {
    // Source 0 keeps older sessions' channel behavior and automation intact.
    driveGain.setTargetValue(juce::Decibels::decibelsToGain(value(Params::drive)));
    cleanBlend.setTargetValue(value(Params::clean) >= 0.5f ? 1.0f : 0.0f);
    tightCutoff.setTargetValue(value(Params::tight));
    compressionMix.setTargetValue(value(Params::cleanComp) / 100);
    // A channel that is fully faded out is not computed: on Clean the captures
    // and cabinet rest, on Metal the clean preamp does. Crossfades run both.
    const bool cleanAudible = cleanBlend.getCurrentValue() > 0 || cleanBlend.getTargetValue() > 0;
    const bool metalAudible = cleanBlend.getCurrentValue() < 1 || cleanBlend.getTargetValue() < 1;
    const float cleanHP = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 45.0f / static_cast<float>(rate));
    const float cleanLP = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 6500.0f / static_cast<float>(rate));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float gain = driveGain.getNextValue();
        const float compression = compressionMix.getNextValue();
        if (cleanAudible)
        {
            // Independent clean preamp: gentle saturation and cabinet-like rolloff.
            if (gain != cleanDriveGain) { cleanDriveGain = gain; cleanDrive = 1.0f + std::log2(gain) * 0.15f; }
            cleanLow += cleanHP * (mono[i] - cleanLow);
            const float dryClean = mono[i] - cleanLow;
            const float compressed = cleanCompressor.processSample(0, dryClean) * 1.41254f;
            const float cleanSample = std::tanh((dryClean + compression * (compressed - dryClean)) * cleanDrive) / cleanDrive;
            cleanHigh += cleanLP * (cleanSample - cleanHigh);
            cleanAudio[static_cast<size_t>(i)] = cleanHigh;
        }
        const float cutoff = tightCutoff.getNextValue();
        if (cutoff != tightCoeffHz)
        {
            tightCoeffHz = cutoff;
            tightCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoff / static_cast<float>(rate));
        }
        metalLow += tightCoeff * (mono[i] - metalLow);
        const float highPassed = mono[i] - metalLow;
        metalLow2 += tightCoeff * (highPassed - metalLow2);
        mono[i] += juce::jlimit(0.0f, 1.0f, (cutoff - 20.0f) / 10.0f) * (highPassed - metalLow2 - mono[i]);
        // Drive pushes the next stage: the pedal and the capture (or the built-in amp).
        mono[i] *= gain;
    }
    // Keep the DI noise floor out of the distortion, where it becomes fuzz under notes.
    if (metalAudible) noiseShield.process(mono, dryInput.data(), buffer.getNumSamples(), value(Params::gate), value(Params::gateOn) >= 0.5f);
    prePedalPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    pedalBlend.setTargetValue(value(Params::pedalOn) >= .5f && value(Params::clean) < .5f ? 1.0f : 0.0f);
    if (metalAudible && pedal && (pedalBlend.isSmoothing() || pedalBlend.getTargetValue() > 0))
    {
        std::copy_n(mono, buffer.getNumSamples(), pedalAudio.data());
        pedal->process(pedalAudio.data(), buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            mono[i] += pedalBlend.getNextValue() * (pedalAudio[static_cast<size_t>(i)] - mono[i]);
    }
    else pedalBlend.skip(buffer.getNumSamples());
    postPedalPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    // The amp: the loaded capture, or a real high-gain voice when there is none,
    // so the metal channel distorts out of the box instead of passing a clean boost.
    fallbackActive.store(!model);
    if (metalAudible)
    {
        if (model) model->process(mono, buffer.getNumSamples());
        else fallbackAmp.process(mono, buffer.getNumSamples());
    }
    postAmpPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    // Cabinet: the loaded IR; otherwise the built-in speaker when the amp has none
    // (no capture, or a capture whose metadata says amp-only). Raw amp output with
    // no speaker is mostly fizz.
    const bool builtInSpeaker = !cab.isLoaded() && (!model || (model->cabinetIsKnown() && !model->hasCabinet()));
    speakerActive.store(builtInSpeaker);
    if (metalAudible && builtInSpeaker) speaker.process(mono, buffer.getNumSamples());
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch) buffer.copyFrom(ch, 0, mono, buffer.getNumSamples());
    if (metalAudible) cab.process(context);
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
    }

    // Blend parallel sub-synthesis layer (bypasses pre-gain distortion)
    if (thicken)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.getWritePointer(ch)[i] += subSynthAudio[static_cast<size_t>(i)];
    }

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float fade = ampSlotGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.getWritePointer(ch)[i] *= fade;
    }

    ampGain.setGainDecibels(value(Params::ampOut)); ampGain.process(context);
    tone.update(value(Params::bass), value(Params::mid), value(Params::treble), value(Params::presence)); tone.process(buffer);
    highCutoff.setTargetValue(value(Params::highCut));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float cutoff = highCutoff.getNextValue();
        if (cutoff != highCoeffHz)
        {
            highCoeffHz = cutoff;
            highCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * juce::jmin(cutoff, static_cast<float>(rate) * 0.45f) / static_cast<float>(rate));
        }
        const float coefficient = highCoeff;
        const float amount = juce::jlimit(0.0f, 1.0f, (20000.0f - cutoff) / 1000.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto& sample = buffer.getWritePointer(ch)[i];
            auto& memory = highCutState[static_cast<size_t>(ch)];
            memory[0] += coefficient * (sample - memory[0]);
            memory[1] += coefficient * (memory[0] - memory[1]);
            sample += amount * (memory[1] - sample);
            sample *= gateEnvelope[static_cast<size_t>(i)];
        }
    }
    pedalEq.configure({value(Params::eqOn) >= .5f, value(Params::eqBody), value(Params::eqMud), value(Params::eqFocus), value(Params::eqFizz)});
    pedalEq.process(buffer);
    postEqPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
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

}
void AmpSuiteAudioProcessor::processUniversalAmp(juce::AudioBuffer<float>& buffer)
{
    auto* mono = buffer.getWritePointer(0);
    const int size = buffer.getNumSamples();
    const bool lumen = activeAmpSource == 1, ferrum = activeAmpSource == 2, natural = activeAmpSource == 4;
    driveGain.setTargetValue(juce::Decibels::decibelsToGain(value(Params::drive)));
    tightCutoff.setTargetValue(value(Params::tight));
    compressionMix.setTargetValue(value(Params::cleanComp) / 100);
    for (int i = 0; i < size; ++i)
    {
        const auto gain = driveGain.getNextValue(), cutoff = tightCutoff.getNextValue();
        if (lumen) cleanAudio[static_cast<size_t>(i)] = 1 + std::log2(gain) * .15f;
        if (!natural && !lumen)
        {
            if (cutoff != tightCoeffHz) { tightCoeffHz = cutoff; tightCoeff = 1 - std::exp(-juce::MathConstants<float>::twoPi * cutoff / static_cast<float>(rate)); }
            metalLow += tightCoeff * (mono[i] - metalLow);
            const float high = mono[i] - metalLow;
            metalLow2 += tightCoeff * (high - metalLow2);
            mono[i] += juce::jlimit(0.0f, 1.0f, (cutoff - 20) / 10) * (high - metalLow2 - mono[i]);
            mono[i] *= gain;
        }
    }
    if (ferrum) noiseShield.process(mono, dryInput.data(), size, value(Params::gate), value(Params::gateOn) >= .5f);
    prePedalPeak.store(buffer.getMagnitude(0, 0, size));
    pedalBlend.setTargetValue(value(Params::pedalOn) >= .5f ? 1.0f : 0.0f);
    if (pedal && (pedalBlend.isSmoothing() || pedalBlend.getTargetValue() > 0))
    {
        std::copy_n(mono, size, pedalAudio.data()); pedal->process(pedalAudio.data(), size);
        for (int i = 0; i < size; ++i) mono[i] += pedalBlend.getNextValue() * (pedalAudio[static_cast<size_t>(i)] - mono[i]);
    }
    else pedalBlend.skip(size);
    postPedalPeak.store(buffer.getMagnitude(0, 0, size));
    if (lumen)
    {
        const float hp = 1 - std::exp(-juce::MathConstants<float>::twoPi * 45 / static_cast<float>(rate));
        const float lp = 1 - std::exp(-juce::MathConstants<float>::twoPi * 6500 / static_cast<float>(rate));
        for (int i = 0; i < size; ++i)
        {
            const float drive = cleanAudio[static_cast<size_t>(i)];
            const float mix = compressionMix.getNextValue();
            cleanLow += hp * (mono[i] - cleanLow); const float dry = mono[i] - cleanLow;
            const float compressed = cleanCompressor.processSample(0, dry) * 1.41254f;
            const float x = std::tanh((dry + mix * (compressed - dry)) * drive) / drive;
            cleanHigh += lp * (x - cleanHigh); mono[i] = cleanHigh;
        }
    }
    else
    {
        compressionMix.skip(size);
        if (ferrum) fallbackAmp.process(mono, size);
        else if (!natural) { if (model) model->process(mono, size); else juce::FloatVectorOperations::clear(mono, size); }
    }
    fallbackActive.store(ferrum);
    postAmpPeak.store(buffer.getMagnitude(0, 0, size));
    const int kind = juce::roundToInt(value(Params::captureKind)), cabinetMode = juce::roundToInt(value(Params::cabMode));
    const bool capture = !lumen && !ferrum && !natural;
    const bool fullRig = capture && (kind == 3 || (kind == 0 && model && model->hasCabinet()));
    const bool hasNoCab = ferrum || (capture && (kind == 1 || kind == 2 || (kind == 0 && model && model->cabinetIsKnown() && !model->hasCabinet())));
    const bool external = cab.isLoaded() && (cabinetMode == 1 || (cabinetMode == 0 && !fullRig && !natural));
    const bool builtIn = cabinetMode == 2 || (cabinetMode == 0 && !external && !fullRig && hasNoCab);
    speakerActive.store(builtIn);
    if (builtIn) speaker.process(mono, size);
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch) buffer.copyFrom(ch, 0, mono, size);
    if (external)
    {
        juce::dsp::AudioBlock<float> block(buffer); juce::dsp::ProcessContextReplacing<float> context(block); cab.process(context);
    }
    postCabPeak.store(buffer.getMagnitude(0, 0, size));
}
void AmpSuiteAudioProcessor::requestFile(bool isModel, const juce::File& file)
{
    const juce::ScopedLock lock(requestLock);
    (isModel ? desiredModel : desiredIr) = file.getFullPathName();
    (isModel ? modelPending : irPending) = true;
    message = file == juce::File() ? "Removing stage..." : "Loading " + file.getFileName() + "...";
    notify();
}
void AmpSuiteAudioProcessor::run()
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    while (!threadShouldExit())
    {
        juce::String modelFile, irFile, pedalFile; bool doModel, doIr, doPedal;
        std::vector<std::pair<juce::File, juce::String>> imports;
        {
            const juce::ScopedLock lock(requestLock);
            imports.swap(pendingImports);
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
                if (isNam && path.isNotEmpty())
                {
                    next = std::make_unique<NamWrapper>(file);
                    // Prepare (and prewarm) outside the DSP lock: this takes long enough
                    // that holding the lock meant many silent callbacks per load.
                    double preparedRate; int preparedBlock;
                    { const juce::ScopedLock lock(dspLock); preparedRate = rate; preparedBlock = maxBlock; }
                    next->prepare(preparedRate, preparedBlock);
                    if (isModel) next->setOutputGain(juce::Decibels::decibelsToGain(static_cast<float>(next->levelMatchDb())));
                    const juce::ScopedLock lock(dspLock);
                    if (rate != preparedRate || maxBlock != preparedBlock) next->prepare(rate, maxBlock);
                    (isPedal ? pedalExpectedRate : ampExpectedRate).store(next->expectedRate());
                    (isPedal ? pedal : model).swap(next);
                }
                else if (isNam)
                {
                    // Clearing: detach under the lock, destroy after it (via `next`).
                    const juce::ScopedLock lock(dspLock);
                    (isPedal ? pedalExpectedRate : ampExpectedRate).store(0);
                    (isPedal ? pedal : model).swap(next);
                }
                else if (path.isEmpty()) cab.clear();
                else
                {
                    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                    if (!reader || reader->lengthInSamples == 0 || reader->numChannels > 2
                        || reader->lengthInSamples > reader->sampleRate * 10)
                        throw std::runtime_error("Choose a mono/stereo WAV impulse response up to 10 seconds.");
                    juce::AudioBuffer<float> impulse(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                    reader->read(&impulse, 0, impulse.getNumSamples(), 0, true, true);
                    cab.load(std::move(impulse), reader->sampleRate);
                }
                // `next` now holds the previous model; describe the one just installed.
                juce::String gear; double levelDb = 0; bool levelled = false, hasCab = false, cabKnown = false;
                if (isModel)
                {
                    const juce::ScopedLock dsp(dspLock);
                    if (model)
                    {
                        gear = model->gear(); levelled = model->hasLoudness(); levelDb = model->levelMatchDb();
                        hasCab = model->hasCabinet(); cabKnown = model->cabinetIsKnown();
                    }
                }
                const auto asset = path.isNotEmpty() ? AssetLibrary::describe(file, isModel ? "amp" : isPedal ? "pedal" : "cab") : juce::ValueTree();
                const juce::ScopedLock lock(requestLock);
                if (asset.isValid()) library.upsert(asset);
                (isPedal ? pedalPath : isModel ? modelPath : irPath) = path;
                if (isModel) { ampGear = gear; ampLevelDb = levelDb; ampLevelled = levelled; ampHasCab = hasCab; ampCabKnown = cabKnown; }
                if (!message.startsWith("Load failed:")) message = path.isEmpty() ? "Stage cleared" : "Loaded " + file.getFileName();
            }
            catch (const std::exception& e)
            {
                const juce::ScopedLock lock(requestLock);
                message = "Load failed: " + juce::String(e.what());
            }
        }
        int imported = 0; juce::StringArray errors;
        for (const auto& item : imports)
        {
            if (threadShouldExit()) break;
            const auto& file = item.first; const auto& kind = item.second;
            try {
                if (!file.existsAsFile() || !file.hasFileExtension(kind == "cab" ? "wav" : "nam"))
                    throw std::runtime_error("Invalid asset file");
                // Validate imports off the audio thread without changing the playing rig.
                if (kind != "cab") { NamWrapper validation(file); }
                else {
                    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                    if (!reader || reader->lengthInSamples == 0 || reader->numChannels > 2 || reader->lengthInSamples > reader->sampleRate * 10)
                        throw std::runtime_error("Choose a mono/stereo WAV impulse response up to 10 seconds");
                }
                const auto asset = AssetLibrary::describe(file, kind);
                const juce::ScopedLock lock(requestLock); library.upsert(asset); ++imported;
            } catch (const std::exception& e) { errors.add(file.getFileName() + ": " + juce::String(e.what())); }
        }
        if (!imports.empty()) {
            const juce::ScopedLock lock(requestLock);
            message = errors.isEmpty() ? "Imported " + juce::String(imported) + " files into the library"
                : "Load failed: " + errors.joinIntoString("; ").substring(0, 1200) + " (" + juce::String(imported) + " imported)";
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
        result->setProperty("libraryRevision", library.revision);
        result->setProperty("modelId", library.idForPath("amp", desiredModel));
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
    result->setProperty("postEq", postEqPeak.load());
    result->setProperty("swapBypasses", swapBypasses.load());
    result->setProperty("gate", gateLevel.load());
    result->setProperty("cpu", processLoad.getLoadAsPercentage());
    result->setProperty("overruns", processLoad.getXRunCount());
    result->setProperty("inputClipped", inputClipped.load());
    // Driver-level dropouts (standalone only; -1 when the device cannot report them).
    result->setProperty("dropouts", deviceDropouts ? deviceDropouts() : -1);
    if (deviceBufferSizes)
    {
        juce::Array<juce::var> sizes;
        for (const auto size : deviceBufferSizes()) sizes.add(size);
        result->setProperty("bufferSizes", sizes);
    }
    result->setProperty("metronomeBeat", metronome.currentBeat());
    result->setProperty("metronomeClicks", metronome.clickCount());
    result->setProperty("metronomeFollowsHost", metronomeFollowsHost.load());
    result->setProperty("metronomeBpm", metronomeBpm.load());
    result->setProperty("humCancelling", humCanceller.cancelling());
    result->setProperty("mainsHz", humCanceller.mainsHz());

    // Tuner & Real-time DSP telemetry
    result->setProperty("tunerActive", pitchTracker.isNoteActive());
    result->setProperty("tunerNote", PitchTracker::midiNoteToName(pitchTracker.getDetectedMidiNote()));
    result->setProperty("tunerCents", pitchTracker.getDetectedCents());
    result->setProperty("tunerHz", pitchTracker.getDetectedHz());
    result->setProperty("dynResCut", dynamicResonance.getCurrentCutDb());
    result->setProperty("speakerSim", speakerActive.load());
    result->setProperty("fallbackAmp", fallbackActive.load());
    result->setProperty("ampSource", juce::roundToInt(value(Params::ampSource)));
    {
        const juce::ScopedLock lock(requestLock);
        result->setProperty("ampGear", ampGear);
        result->setProperty("ampLevelled", ampLevelled);
        result->setProperty("ampLevelDb", ampLevelDb);
        result->setProperty("ampHasCab", ampHasCab);
        result->setProperty("ampCabKnown", ampCabKnown);
    }

    return juce::var(result.release());
}
void AmpSuiteAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if (auto xml = copyRigState(true).createXml()) copyXmlToBinary(*xml, destination);
}
juce::ValueTree AmpSuiteAudioProcessor::copyRigState(bool includeSavedRigs)
{
    auto state = apvts.copyState();
    // Preserve requested paths during asynchronous recall, including missing files.
    {
        const juce::ScopedLock lock(requestLock);
        state.setProperty("modelPath", desiredModel, nullptr); state.setProperty("irPath", desiredIr, nullptr); state.setProperty("pedalPath", desiredPedal, nullptr);
        state.setProperty("modelId", library.idForPath("amp", desiredModel), nullptr);
        state.setProperty("irId", library.idForPath("cab", desiredIr), nullptr);
        state.setProperty("pedalId", library.idForPath("pedal", desiredPedal), nullptr);
        auto catalog = library.tree.createCopy();
        if (!includeSavedRigs)
            for (int i = catalog.getNumChildren(); --i >= 0;) if (catalog.getChild(i).hasType("RIG")) catalog.removeChild(i, nullptr);
        const auto previous = state.getChildWithName("LIBRARY");
        if (previous.isValid()) state.removeChild(previous, nullptr);
        state.addChild(catalog, -1, nullptr);
    }
    return state;
}
void AmpSuiteAudioProcessor::setStateInformation(const void* data, int size)
{
    const auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml(*xml);
    {
        const juce::ScopedLock lock(requestLock);
        library.merge(state.getChildWithName("LIBRARY"));
        for (const auto& stage : {"model", "ir", "pedal"})
        {
            const auto key = juce::String(stage) + "Path";
            const auto reference = library.find(state[juce::String(stage) + "Id"].toString());
            if (!juce::File(state[key].toString()).existsAsFile() && reference.isValid()) state.setProperty(key, reference["path"], nullptr);
        }
    }
    // Older sessions have no EQ parameters. Recall their original sound even
    // when an EQ was active before loading that session.
    for (size_t i = Params::eqOn; i < Params::definitions.size(); ++i)
    {
        const auto& definition = Params::definitions[i];
        if (!state.getChildWithProperty("id", definition.id).isValid())
        {
            juce::ValueTree parameter("PARAM");
            parameter.setProperty("id", definition.id, nullptr);
            parameter.setProperty("value", definition.initial, nullptr);
            state.addChild(parameter, -1, nullptr);
        }
    }
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
    message = file == juce::File() ? "Removing stage..." : "Loading " + file.getFileName() + "..."; notify();
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

void AmpSuiteAudioProcessor::setParameterValue(const char* id, float amount)
{
    if (auto* parameter = apvts.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(amount));
}
juce::var AmpSuiteAudioProcessor::getLibrary()
{
    const juce::ScopedLock lock(requestLock); return library.list();
}
void AmpSuiteAudioProcessor::importAssets(const juce::Array<juce::File>& files, const juce::String& kind)
{
    if (kind != "amp" && kind != "pedal" && kind != "cab") return;
    const juce::ScopedLock lock(requestLock);
    for (const auto& file : files) pendingImports.emplace_back(file, kind);
    if (!files.isEmpty()) message = "Importing library files...";
    notify();
}
juce::var AmpSuiteAudioProcessor::getRig()
{
    {
        const juce::ScopedLock lock(requestLock);
        if (desiredModel != modelPath || desiredIr != irPath || desiredPedal != pedalPath)
        {
            auto result = std::make_unique<juce::DynamicObject>(); result->setProperty("error", "Finish loading or relinking the assets before saving/comparing this rig.");
            return juce::var(result.release());
        }
    }
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("schema", 1); result->setProperty("state", copyRigState(false).createXml()->toString());
    return juce::var(result.release());
}
juce::String AmpSuiteAudioProcessor::applyRig(const juce::var& rig, bool preserveGlobals)
{
    juce::ValueTree state;
    if (const auto error = readRig(rig, state); error.isNotEmpty()) return error;
    // Resolve stable IDs before changing any parameters. Missing references leave
    // the current rig untouched, rather than playing new settings through old files.
    {
        const juce::ScopedLock lock(requestLock);
        for (const auto& stage : {"model", "ir", "pedal"})
        {
            const auto key = juce::String(stage) + "Path";
            auto path = state[key].toString();
            if (path.isEmpty()) continue;
            auto reference = library.find(state[juce::String(stage) + "Id"].toString());
            if (!reference.isValid()) reference = state.getChildWithName("LIBRARY").getChildWithProperty("id", state[juce::String(stage) + "Id"]);
            if (!juce::File::isAbsolutePath(path) || !juce::File(path).existsAsFile())
            {
                path = reference["path"].toString();
                if (!juce::File::isAbsolutePath(path) || !juce::File(path).existsAsFile()) return "Missing " + juce::String(stage) + " asset. Relink it in the Library first.";
                state.setProperty(key, path, nullptr);
            }
            if (!juce::File(path).hasFileExtension(juce::String(stage) == "ir" ? "wav" : "nam")) return "Invalid asset format in rig.";
            const auto id = state[juce::String(stage) + "Id"].toString();
            const auto kind = juce::String(stage) == "model" ? "amp" : juce::String(stage) == "ir" ? "cab" : "pedal";
            if (id.isNotEmpty() && id != juce::String(kind) + ":" + juce::SHA256(juce::File(path)).toHexString())
                return "The " + juce::String(stage) + " file has changed. Relink the original asset or import the changed file as a new asset.";
        }
    }
    for (const auto& definition : Params::definitions)
    {
        auto parameter = state.getChildWithProperty("id", definition.id);
        if (!parameter.isValid()) continue;
        if (preserveGlobals && (juce::String(definition.id) == "INPUT_GAIN" || juce::String(definition.id) == "MASTER_VOL" || juce::String(definition.id).startsWith("METRO_")))
            parameter.setProperty("value", apvts.getRawParameterValue(definition.id)->load(), nullptr);
    }
    juce::MemoryBlock binary; copyXmlToBinary(*state.createXml(), binary);
    setStateInformation(binary.getData(), static_cast<int>(binary.getSize())); return {};
}
juce::String AmpSuiteAudioProcessor::saveRig(const juce::String& name)
{
    const auto title = name.trim().substring(0, 80);
    if (title.isEmpty()) return "Give the rig a name.";
    const auto rig = getRig(); if (rig.hasProperty("error")) return rig["error"].toString();
    const juce::ScopedLock lock(requestLock);
    juce::ValueTree entry("RIG");
    entry.setProperty("id", juce::Uuid().toString(), nullptr); entry.setProperty("name", title, nullptr);
    entry.setProperty("state", rig["state"], nullptr); entry.setProperty("favorite", false, nullptr);
    library.tree.addChild(entry, -1, nullptr); ++library.revision; return {};
}
juce::String AmpSuiteAudioProcessor::loadRig(const juce::String& id)
{
    juce::var state;
    { const juce::ScopedLock lock(requestLock); const auto rig = library.find(id); if (!rig.hasType("RIG")) return "Rig not found."; state = rig["state"]; }
    auto rig = std::make_unique<juce::DynamicObject>(); rig->setProperty("schema", 1); rig->setProperty("state", state);
    return applyRig(juce::var(rig.release()));
}
juce::String AmpSuiteAudioProcessor::importRig(const juce::String& name, const juce::var& rig)
{
    juce::ValueTree state;
    if (const auto error = readRig(rig, state); error.isNotEmpty()) return error;
    auto imported = state.getChildWithName("LIBRARY");
    for (int i = imported.getNumChildren(); --i >= 0;) if (!imported.getChild(i).hasType("ASSET")) imported.removeChild(i, nullptr);
    const juce::ScopedLock lock(requestLock); library.merge(imported);
    juce::ValueTree entry("RIG"); entry.setProperty("id", juce::Uuid().toString(), nullptr);
    entry.setProperty("name", name.trim().substring(0, 80), nullptr); entry.setProperty("state", state.createXml()->toString(), nullptr);
    library.tree.addChild(entry, -1, nullptr); ++library.revision;
    return {};
}
bool AmpSuiteAudioProcessor::removeRig(const juce::String& id)
{
    const juce::ScopedLock lock(requestLock); auto rig = library.find(id); if (!rig.hasType("RIG")) return false;
    library.tree.removeChild(rig, nullptr); ++library.revision; return true;
}
bool AmpSuiteAudioProcessor::editAsset(const juce::String& id, const juce::var& changes)
{
    const juce::ScopedLock lock(requestLock); auto asset = library.find(id); if (!asset.isValid()) return false;
    for (const auto& key : {"name", "creator", "tags", "sourceURL", "notes"})
        if (changes.hasProperty(key) && changes[key].isString()) asset.setProperty(key, changes[key].toString().substring(0, 1000), nullptr);
    if (changes["favorite"].isBool()) asset.setProperty("favorite", changes["favorite"], nullptr);
    ++library.revision; return true;
}
bool AmpSuiteAudioProcessor::selectAsset(const juce::String& id)
{
    juce::ValueTree asset;
    { const juce::ScopedLock lock(requestLock); asset = library.find(id).createCopy(); }
    if (!asset.hasType("ASSET") || !juce::File(asset["path"].toString()).existsAsFile()) return false;
    const auto kind = asset["kind"].toString(); const juce::File file(asset["path"].toString());
    if (kind == "amp") { requestFile(true, file); setParameterValue("CAPTURE_KIND", static_cast<float>(asset["captureKind"])); setParameterValue("AMP_SOURCE", 3); }
    else if (kind == "pedal") { requestPedal(file); setParameterValue("PEDAL_ON", 1); }
    else if (kind == "cab") { requestFile(false, file); setParameterValue("CAB_MODE", 1); }
    else return false;
    return true;
}
juce::String AmpSuiteAudioProcessor::relinkAsset(const juce::String& id, const juce::File& file)
{
    juce::String kind;
    { const juce::ScopedLock lock(requestLock); const auto asset = library.find(id); if (!asset.hasType("ASSET")) return "Asset not found."; kind = asset["kind"].toString(); }
    if (!file.existsAsFile()) return "File not found.";
    if (kind + ":" + juce::SHA256(file).toHexString() != id) return "That file has different content. Choose the original asset, or import it as a new one.";
    const juce::ScopedLock lock(requestLock); auto asset = library.find(id);
    const bool activeAmp = kind == "amp" && library.idForPath(kind, desiredModel) == id;
    const bool activeCab = kind == "cab" && library.idForPath(kind, desiredIr) == id;
    const bool activePedal = kind == "pedal" && library.idForPath(kind, desiredPedal) == id;
    AssetLibrary::rememberPath(asset, file.getFullPathName()); ++library.revision;
    if (activeAmp) { desiredModel = file.getFullPathName(); modelPending = true; }
    if (activeCab) { desiredIr = file.getFullPathName(); irPending = true; }
    if (activePedal) { desiredPedal = file.getFullPathName(); pedalPending = true; }
    notify(); return {};
}
