#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SoundPack.h"
#include <cstdlib>

static juce::String readRig(const juce::var& rig, juce::ValueTree& state)
{
    const auto schema = rig["schema"];
    if ((!schema.isInt() && !schema.isInt64()) || (static_cast<juce::int64>(schema) < 1 || static_cast<juce::int64>(schema) > 3)
        || !rig["state"].isString() || rig["state"].toString().length() > 4 * 1024 * 1024)
        return "Unsupported Cassian rig format.";
    auto xml = juce::XmlDocument::parse(rig["state"].toString());
    if (!xml || !xml->hasTagName("AmpSuiteState")) return "Invalid rig state.";
    state = juce::ValueTree::fromXml(*xml);
    if (static_cast<int>(schema) >= 2 && !state.getChildWithName("PEDALBOARD").isValid()) return "Incomplete rig pedalboard state.";
    if (const auto failure = PedalboardState::migrate(state); failure.isNotEmpty()) return failure;
    if (const auto failure = PerformanceScenes::migrate(state); failure.isNotEmpty()) return failure;
    juce::StringArray ids;
    int sceneBanks = 0;
    for (const auto& child : state)
        if (child.hasType("SCENES") && ++sceneBanks > 1) return "Duplicate scene bank.";
    for (const auto& child : state)
        if (child.hasType("PARAM")) {
            const auto id = child["id"].toString();
            if (ids.contains(id)) return "Duplicate rig parameter.";
            if (static_cast<int>(schema) >= 2) {
                bool known = false; BoardParams::each([&](const auto& definition) { if (id == definition.id) known = true; });
                if (!known) return "Unsupported rig parameter.";
            }
            ids.add(id);
        }
    for (size_t index = 0; index < Params::definitions.size(); ++index)
    {
        const auto& definition = Params::definitions[index];
        auto parameter = state.getChildWithProperty("id", definition.id);
        // Schema-1 rigs predating this update contain exactly the first 41 controls.
        if (!parameter.isValid() && static_cast<int>(schema) == 1 && index >= 41) {
            parameter = juce::ValueTree("PARAM"); parameter.setProperty("id", definition.id, nullptr);
            parameter.setProperty("value", definition.initial, nullptr); state.addChild(parameter, -1, nullptr);
        }
        if (!parameter.hasType("PARAM")) return "Incomplete rig: " + juce::String(definition.id);
        const auto text = parameter["value"].toString().toStdString(); char* end = nullptr;
        const double amount = std::strtod(text.c_str(), &end);
        // XML also shortens float endpoints. Use native parameter precision for
        // bounds, as with scene JSON, so a saved minimum can be recalled.
        const auto nativeAmount = static_cast<float>(amount);
        if (end == text.c_str() || *end != '\0' || !std::isfinite(amount) || nativeAmount < definition.min || nativeAmount > definition.max)
            return "Invalid rig parameter: " + juce::String(definition.id);
        if (definition.unit[0] == 0 && amount != std::floor(amount))
            return "Invalid rig routing choice.";
    }
    if (static_cast<int>(schema) < 3 && PedalboardState::serial(state)) return "Serial boards require rig schema 3.";
    return BoardParams::addDefaults(state, static_cast<int>(schema) == 3);
}

AmpSuiteAudioProcessor::AmpSuiteAudioProcessor(bool sharedLibrary, juce::File libraryRoot)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::mono(), true)
                                     .withInput("Backing track", juce::AudioChannelSet::stereo(), false)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      Thread("AmpSuite asset loader"), takes(sharedLibrary ? libraryRoot.getChildFile("takes.xml") : juce::File(), takeReview),
      practice(262144, sharedLibrary ? libraryRoot.getChildFile("practice") : juce::File()),
      apvts(*this, nullptr, "AmpSuiteState", BoardParams::layout()), sharedStore(sharedLibrary ? libraryRoot : juce::File())
{
    for (size_t i = 0; i < parameters.size(); ++i)
        parameters[i] = apvts.getRawParameterValue(Params::definitions[i].id);
    PedalboardState::migrate(apvts.state);
    try { library.merge(sharedStore.load()); } catch (const std::exception& e) { message = "Load failed: " + juce::String(e.what()); }
    practice.onTakeFinished = [&store = takes](const juce::File& folder) { store.importFolder(folder); };
    midiControl.start([this](const auto& mapping, int amount) { return handleMidiAction(mapping, amount); });
    startThread();
}
AmpSuiteAudioProcessor::~AmpSuiteAudioProcessor() { midiControl.shutdown(); signalThreadShouldExit(); notify(); stopThread(-1); }

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
    outputLimitHold = 0; outputPeakWarning.store(false);
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
    cab->prepare(spec, cabinetSettings());
    preCompressor.prepare(rate, compressorSettings(value(Params::compMode) == 1));
    postCompressor.prepare(rate, compressorSettings(value(Params::compMode) == 2));
    overdrive.prepare(rate, maxBlock, {value(Params::odOn) >= .5f, value(Params::odDrive), value(Params::odTone), value(Params::odLevel), value(Params::odTight)});
    reportedOverdriveLatency.store(overdrive.latencySamples());
    tone.prepare(rate, {value(Params::bass), value(Params::mid), value(Params::treble), value(Params::presence)});
    pedalEq.prepare(rate, {value(Params::eqOn) >= .5f, value(Params::eqBody), value(Params::eqMud), value(Params::eqFocus), value(Params::eqFizz)});
    backingAudio.setSize(2, maxBlock);
    guitarMixDelta.setSize(2, maxBlock);
    guitarMix.prepare(rate, maxBlock, value(Params::guitarMixLevel), value(Params::guitarMixFocus));
    recordingDry.resize(static_cast<size_t>(maxBlock));
    practice.prepare(rate);
    takes.stopReview();
    takeReview.prepare(rate);
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
    cleanCompressor.setThreshold(value(Params::compThreshold)); cleanCompressor.setRatio(value(Params::compRatio));
    cleanCompressor.setAttack(value(Params::compAttack)); cleanCompressor.setRelease(value(Params::compRelease));
    compressionMix.reset(rate, 0.03); compressionMix.setCurrentAndTargetValue(!serialBoard && value(Params::compMode) == 0 ? value(Params::cleanComp) / 100 : 0);
    highCutoff.reset(rate, 0.03); highCutoff.setCurrentAndTargetValue(value(Params::highCut));
    stereoWidth.reset(rate, 0.05); stereoWidth.setCurrentAndTargetValue(value(Params::delayWidth) / 100);
    cleanBlend.reset(rate, 0.03); cleanBlend.setCurrentAndTargetValue(value(Params::clean) >= 0.5f ? 1.0f : 0.0f);
    activeAmpSource = juce::roundToInt(value(Params::ampSource));
    ampSlotGain.reset(rate, .02); ampSlotGain.setCurrentAndTargetValue(1);
    rigSwitchGain.reset(rate, .02); rigSwitchGain.setCurrentAndTargetValue(1);
    tightCutoff.reset(rate, 0.03); tightCutoff.setCurrentAndTargetValue(value(Params::tight));
    delay.setMaximumDelayInSamples(static_cast<int>(rate * 16)); delay.prepare(spec); delay.reset();
    for (auto* smooth : {&pedalInputGain, &pedalOutputGain, &delayFeedbackGain, &reverbPreDelay}) smooth->reset(rate, .03);
    pedalInputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(value(Params::pedalInput)));
    pedalOutputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(value(Params::pedalOutput)));
    delayFeedbackGain.setCurrentAndTargetValue(value(Params::delayFeedback) / 100);
    reverbPreDelay.setCurrentAndTargetValue(value(Params::reverbPredelay) * static_cast<float>(rate) / 1000);
    chorus.prepare(spec, {value(Params::chorusMix), value(Params::chorusRate), value(Params::chorusDepth)});
    metronomeBpm.store(static_cast<double>(value(Params::metroBpm)));
    modulation.prepare(spec, modulationSettings());
    roomDelay.setMaximumDelayInSamples(static_cast<int>(rate * .2)); roomDelay.prepare(spec); roomDelay.reset(); roomAudio.setSize(2, maxBlock);
    driveGain.reset(rate, 0.02); driveGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(value(Params::drive)));
    delayTime.reset(rate, 0.05); delayTime.setCurrentAndTargetValue(value(Params::delayTime) * static_cast<float>(rate) / 1000);
    delayMix.reset(rate, 0.02); delayMix.setCurrentAndTargetValue(value(Params::delayMix) / 100);
    juce::dsp::Reverb::Parameters initialRoom; initialRoom.dryLevel = 0;
    initialRoom.wetLevel = value(Params::reverbMix) / 100; reverb.setParameters(initialRoom);
    reverb.prepare(spec); reverb.reset(); roomDry.reset(rate, .01); roomDry.setCurrentAndTargetValue(2 - initialRoom.wetLevel);
    limiter.prepare(spec); limiter.setThreshold(-0.5f); limiter.setRelease(60);
    if (model) model->prepare(rate, maxBlock);
    if (pedal) pedal->prepare(rate, maxBlock);
    if (pedal1) pedal1->prepare(rate, maxBlock);
    serialBoard = PedalboardState::serial(apvts.state) ? std::make_unique<SerialPedalboard>(apvts.copyState(), apvts, spec) : nullptr;
    reportedRate.store(rate); reportedBlock.store(hostBlock); reportedChannels.store(getTotalNumOutputChannels());
    prePedalPeak.store(0); postPedalPeak.store(0); postAmpPeak.store(0); postCabPeak.store(0);
    postEqPeak.store(0);
    guitarPower.store(0); inputPower.store(0);
    swapBypasses.store(0);
    ampExpectedRate.store(model ? model->expectedRate() : 0);
    pedalExpectedRate.store(pedal ? pedal->expectedRate() : 0);
}
void AmpSuiteAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midiControl.receive(midi); midi.clear(); lastAudioTick.store(juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
    auto mainBuffer = getBusBuffer(buffer, false, 0);
    auto backingBuffer = getBusBuffer(buffer, true, 1);
    // Asset replacement never makes the audio thread wait or destroy a model.
    // Models are prepared and IRs built off the lock, so it is held only for a
    // pointer swap; on rare contention, mute only the guitar and keep the
    // backing track, click, Master, and output protection running.
    const juce::ScopedTryLock lock(dspLock);
    if (!lock.isLocked())
    {
        // An interrupted guitar path cannot produce a valid paired take.
        practice.interrupted();
        for (int i = 0; i < mainBuffer.getNumSamples(); ++i) {
            // Read both auxiliary channels before writing overlapping outputs.
            const float left = backingBuffer.getNumChannels() > 0 ? backingBuffer.getSample(0, i) : 0;
            const float right = backingBuffer.getNumChannels() > 1 ? backingBuffer.getSample(1, i) : left;
            if (mainBuffer.getNumChannels() > 0) mainBuffer.setSample(0, i, left);
            if (mainBuffer.getNumChannels() > 1) mainBuffer.setSample(1, i, right);
        }
        swapBypasses.fetch_add(1);
        if (mainBuffer.getNumChannels() > 0 && mainBuffer.getNumSamples() > 0) {
            const bool counted = practice.process(mainBuffer, mainBuffer.getReadPointer(0), false);
            const bool reviewing = takeReview.transportActive();
            if (reviewing) mainBuffer.clear();
            takeReview.process(mainBuffer, mainBuffer.getReadPointer(0));
            finishOutputMix(mainBuffer, counted || reviewing);
        }
        return;
    }
    if (mainBuffer.getNumChannels() == 0 || mainBuffer.getNumSamples() == 0) return;
    const auto hostPosition = getPlayHead() != nullptr ? getPlayHead()->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo> {};
    metronomeBpm.store(hostPosition && hostPosition->getIsPlaying() && hostPosition->getBpm() ? *hostPosition->getBpm() : static_cast<double>(value(Params::metroBpm)));
    const juce::AudioProcessLoadMeasurer::ScopedTimer timing(processLoad, mainBuffer.getNumSamples());
    inputPeak.store(mainBuffer.getMagnitude(0, 0, mainBuffer.getNumSamples()));
    clipHoldSamples = inputPeak.load() >= .995f ? static_cast<int>(rate)
        : juce::jmax(0, clipHoldSamples - mainBuffer.getNumSamples());
    inputClipped.store(clipHoldSamples > 0);
    bool counted = false;
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
        juce::FloatVectorOperations::copy(recordingDry.data(), chunk.getReadPointer(0), size);
        processChunk(chunk);
        float* deltaChannels[] {guitarMixDelta.getWritePointer(0), guitarMixDelta.getWritePointer(1)};
        juce::AudioBuffer<float> delta(deltaChannels, chunk.getNumChannels(), size);
        guitarMix.difference(chunk, delta, value(Params::guitarMixLevel), value(Params::guitarMixFocus));
        counted = practice.process(chunk, recordingDry.data()) || counted;
        // Listening balance follows take capture and precedes backing/click mix.
        for (int ch = 0; ch < chunk.getNumChannels(); ++ch)
            chunk.addFrom(ch, 0, delta, ch, 0, size);
        // Backing audio bypasses all guitar processing, including the EQ and effects.
        if (backingChannels > 0)
            for (int ch = 0; ch < chunk.getNumChannels(); ++ch)
                chunk.addFrom(ch, 0, backingAudio, ch, 0, size);
        const bool reviewing = takeReview.transportActive();
        if (reviewing) chunk.clear();
        takeReview.process(chunk, recordingDry.data());
        counted = counted || reviewing;
    }
    finishOutputMix(mainBuffer, counted);
}
void AmpSuiteAudioProcessor::renderGuitarOffline(juce::AudioBuffer<float>& buffer, int frames)
{
    jassert(isNonRealtime());
    const juce::ScopedLock lock(dspLock);
    juce::ScopedNoDenormals noDenormals;
    // Offline reamping has no playing host clock; use the snapshot's tempo for
    // both delay and modulation rather than the live instance's last host BPM.
    metronomeBpm.store(static_cast<double>(value(Params::metroBpm)));
    for (int offset = 0; offset < frames; offset += maxBlock) {
        const int n = juce::jmin(maxBlock, frames - offset);
        float* channels[] {buffer.getWritePointer(0, offset), buffer.getWritePointer(1, offset)};
        juce::AudioBuffer<float> chunk(channels, 2, n); processChunk(chunk);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i)
            if (!std::isfinite(chunk.getSample(ch, i))) chunk.setSample(ch, i, 0);
    }
}
void AmpSuiteAudioProcessor::finishOutputMix(juce::AudioBuffer<float>& mainBuffer, bool suppressClick)
{
    // The click joins after everything, so the rig never processes it.
    {
        const auto position = getPlayHead() != nullptr ? getPlayHead()->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo> {};
        const bool follows = position && position->getIsPlaying() && position->getBpm() && position->getPpqPosition();
        metronomeFollowsHost.store(follows);
        metronomeBpm.store(follows ? *position->getBpm() : static_cast<double>(value(Params::metroBpm)));
        if (suppressClick) metronome.reset();
        metronome.process(mainBuffer.getArrayOfWritePointers(), mainBuffer.getNumChannels(), mainBuffer.getNumSamples(),
            {value(Params::metroOn) >= 0.5f && !suppressClick, value(Params::metroBpm), juce::roundToInt(value(Params::metroBeats)), value(Params::metroLevel)}, position);
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
    outputLimitHold = mainBuffer.getMagnitude(0, mainBuffer.getNumSamples()) >= juce::Decibels::decibelsToGain(-.5f)
        ? static_cast<int>(rate) : juce::jmax(0, outputLimitHold - mainBuffer.getNumSamples());
    outputPeakWarning.store(outputLimitHold > 0);
    limiter.process(outputContext);
    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        juce::FloatVectorOperations::clip(mainBuffer.getWritePointer(ch), mainBuffer.getReadPointer(ch), -1.0f, 1.0f, mainBuffer.getNumSamples());
    outputPeak.store(mainBuffer.getMagnitude(0, 0, mainBuffer.getNumSamples()));
}
void AmpSuiteAudioProcessor::processChunk(juce::AudioBuffer<float>& buffer)
{
    pedalInputGain.setTargetValue(juce::Decibels::decibelsToGain(value(Params::pedalInput)));
    pedalOutputGain.setTargetValue(juce::Decibels::decibelsToGain(value(Params::pedalOutput)));
    cleanCompressor.setThreshold(value(Params::compThreshold)); cleanCompressor.setRatio(value(Params::compRatio));
    cleanCompressor.setAttack(value(Params::compAttack)); cleanCompressor.setRelease(value(Params::compRelease));
    compressionMakeupGain = 1.41254f * juce::Decibels::decibelsToGain(value(Params::compMakeup) - 3);
    if (model) model->setOutputGain(value(Params::captureMatch) >= .5f ? juce::Decibels::decibelsToGain(static_cast<float>(model->levelMatchDb())) : 1);
    // Guitar uses the first routed input. Standalone maps the physical channel
    // selected in device settings; DAWs route their chosen input to this bus.
    // Ignore a second bus channel instead of mixing unused-preamp noise into it.
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
    reportedHum.store(activeAmpSource != 4 && humCanceller.cancelling(), std::memory_order_relaxed);
    reportedMains.store(humCanceller.mainsHz(), std::memory_order_relaxed);

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

    if (!serialBoard) {
        preCompressor.configure(compressorSettings(value(Params::compMode) == 1));
        float* guitarChannels[] {mono}; preCompressor.process(guitarChannels, 1, buffer.getNumSamples());
    }
    cab->configure(cabinetSettings());

    // 4. Dynamic 200-400 Hz Resonance Suppression Notch
    dynamicResonance.configure(value(Params::dynResOn) >= 0.5f, value(Params::dynResAmount));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        mono[i] = dynamicResonance.processSample(mono[i]);

    // 5. "Thicken" Sub-Octave Parallel Synthesizer
    subSynth.configure(thicken, value(Params::thickenMix));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        subSynthAudio[static_cast<size_t>(i)] = subSynth.processSample(mono[i], trackedPitch);

    if (serialBoard) {
        float* channel[] {mono}; juce::AudioBuffer<float> pre(channel, 1, buffer.getNumSamples());
        NamWrapper* captures[] {pedal.get(), pedal1.get()}; serialBoard->process(pre, true, static_cast<float>(metronomeBpm.load()), captures);
    } else {
        overdrive.configure({value(Params::odOn) >= .5f, value(Params::odDrive), value(Params::odTone), value(Params::odLevel), value(Params::odTight)});
        overdrive.process(mono, buffer.getNumSamples());
    }
    if (activeAmpSource > 0) processUniversalAmp(buffer);
    else
    {
    // Source 0 keeps older sessions' channel behavior and automation intact.
    driveGain.setTargetValue(juce::Decibels::decibelsToGain(value(Params::drive)));
    cleanBlend.setTargetValue(value(Params::clean) >= 0.5f ? 1.0f : 0.0f);
    tightCutoff.setTargetValue(value(Params::tight));
    compressionMix.setTargetValue(!serialBoard && value(Params::compMode) == 0 ? value(Params::cleanComp) / 100 : 0);
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
            const float compressed = cleanCompressor.processSample(0, dryClean) * compressionMakeupGain;
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
    if (!serialBoard && metalAudible && pedal && (pedalBlend.isSmoothing() || pedalBlend.getTargetValue() > 0))
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i) pedalAudio[static_cast<size_t>(i)] = mono[i] * pedalInputGain.getNextValue();
        pedal->process(pedalAudio.data(), buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            mono[i] += pedalBlend.getNextValue() * (pedalAudio[static_cast<size_t>(i)] * pedalOutputGain.getNextValue() - mono[i]);
    }
    else { pedalBlend.skip(buffer.getNumSamples()); pedalInputGain.skip(buffer.getNumSamples()); pedalOutputGain.skip(buffer.getNumSamples()); }
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
    const bool builtInSpeaker = !cab->hasExternal() && (!model || (model->cabinetIsKnown() && !model->hasCabinet()));
    speakerActive.store(builtInSpeaker);
    if (metalAudible && builtInSpeaker) speaker.process(mono, buffer.getNumSamples());
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch) buffer.copyFrom(ch, 0, mono, buffer.getNumSamples());
    if (metalAudible) cab->process(context);
    if (metalAudible && builtInSpeaker) cab->filter(block);
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

    if (!serialBoard) {
        postCompressor.configure(compressorSettings(value(Params::compMode) == 2));
        postCompressor.process(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    }
    reportedCompression.store(juce::jmax(preCompressor.reductionDb(), postCompressor.reductionDb()), std::memory_order_relaxed);

    // Blend parallel sub-synthesis layer (bypasses pre-gain distortion)
    if (thicken)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.getWritePointer(ch)[i] += subSynthAudio[static_cast<size_t>(i)];
    }

    rigSwitchGain.setTargetValue(rigSwapReady.load() ? 0.0f : 1.0f);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float fade = ampSlotGain.getNextValue() * rigSwitchGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.getWritePointer(ch)[i] *= fade;
    }

    rigMuted.store(rigSwitchGain.getCurrentValue() <= 0 && rigSwapReady.load());
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
    if (serialBoard) {
        NamWrapper* captures[] {pedal.get(), pedal1.get()}; serialBoard->process(buffer, false, static_cast<float>(metronomeBpm.load()), captures);
        postEqPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    } else {
    pedalEq.configure({value(Params::eqOn) >= .5f, value(Params::eqBody), value(Params::eqMud), value(Params::eqFocus), value(Params::eqFizz)});
    pedalEq.process(buffer);
    postEqPeak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    modulation.configure(modulationSettings()); modulation.process(buffer);
    chorus.configure({value(Params::chorusMix), value(Params::chorusRate), value(Params::chorusDepth)});
    chorus.process(context);
    constexpr float divisions[] {1, .5f, .75f, .25f, 2, 4};
    const float milliseconds = value(Params::delaySync) >= .5f ? 60000 * divisions[juce::jlimit(0, 5, juce::roundToInt(value(Params::delayDivision)))] / static_cast<float>(juce::jlimit(20.0, 400.0, metronomeBpm.load())) : value(Params::delayTime);
    delayTime.setTargetValue(milliseconds * static_cast<float>(rate) / 1000);
    delayFeedbackGain.setTargetValue(value(Params::delayFeedback) / 100);
    delayMix.setTargetValue(value(Params::delayMix) / 100);
    stereoWidth.setTargetValue(value(Params::delayWidth) / 100);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto time = delayTime.getNextValue(), mix = delayMix.getNextValue();
        const auto width = stereoWidth.getNextValue();
        const float feedback = delayFeedbackGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto& sample = buffer.getWritePointer(ch)[i];
            const float wet = delay.popSample(ch, time * (ch == 1 ? 1.0f + 0.25f * width : 1.0f));
            delay.pushSample(ch, sample + wet * feedback);
            sample = sample * (1 - mix * 0.5f) + wet * mix * 0.5f;
        }
    }
    juce::dsp::Reverb::Parameters rv;
    const int voice = juce::roundToInt(value(Params::reverbStyle));
    rv.roomSize = voice == 0 ? value(Params::reverbSize) / 100 : voice == 1 ? .45f + value(Params::reverbSize) * .004f : .7f + value(Params::reverbSize) * .0029f;
    rv.damping = value(Params::reverbDamp) / 100; rv.wetLevel = value(Params::reverbMix) / 100; rv.dryLevel = 0;
    reverbPreDelay.setTargetValue(value(Params::reverbPredelay) * static_cast<float>(rate) / 1000);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float pre = reverbPreDelay.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            roomDelay.pushSample(ch, buffer.getSample(ch, i)); roomAudio.setSample(ch, i, roomDelay.popSample(ch, pre));
        }
    }
    juce::AudioBuffer<float> roomChunk(roomAudio.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    juce::dsp::AudioBlock<float> roomBlock(roomChunk); juce::dsp::ProcessContextReplacing<float> roomContext(roomBlock);
    reverb.setParameters(rv); reverb.process(roomContext);
    // Keep JUCE's legacy dry scale (2 * dryLevel), including old saved tones.
    roomDry.setTargetValue(2 - rv.wetLevel);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float dry = roomDry.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, i, buffer.getSample(ch, i) * dry + roomChunk.getSample(ch, i));
    }

    } // Fixed compatibility effects.

    // Sub-sample micro-timing delay for stereo double-tracking
    microDelay.configure(value(Params::microDelay));
    if (buffer.getNumChannels() >= 2)
        microDelay.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());

    double guitar = 0, input = 0;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        input += dryInput[static_cast<size_t>(i)] * dryInput[static_cast<size_t>(i)];
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) guitar += buffer.getSample(ch, i) * buffer.getSample(ch, i);
    }
    const float retain = static_cast<float>(std::exp(-buffer.getNumSamples() / (rate * .5)));
    guitarPower.store(retain * guitarPower.load() + (1 - retain) * static_cast<float>(guitar / (buffer.getNumSamples() * buffer.getNumChannels())));
    inputPower.store(retain * inputPower.load() + (1 - retain) * static_cast<float>(input / buffer.getNumSamples()));

}
void AmpSuiteAudioProcessor::processUniversalAmp(juce::AudioBuffer<float>& buffer)
{
    auto* mono = buffer.getWritePointer(0);
    const int size = buffer.getNumSamples();
    const bool lumen = activeAmpSource == 1, ferrum = activeAmpSource == 2, natural = activeAmpSource == 4;
    driveGain.setTargetValue(juce::Decibels::decibelsToGain(value(Params::drive)));
    tightCutoff.setTargetValue(value(Params::tight));
    compressionMix.setTargetValue(!serialBoard && value(Params::compMode) == 0 ? value(Params::cleanComp) / 100 : 0);
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
    if (!serialBoard && pedal && (pedalBlend.isSmoothing() || pedalBlend.getTargetValue() > 0))
    {
        for (int i = 0; i < size; ++i) pedalAudio[static_cast<size_t>(i)] = mono[i] * pedalInputGain.getNextValue();
        pedal->process(pedalAudio.data(), size);
        for (int i = 0; i < size; ++i) mono[i] += pedalBlend.getNextValue() * (pedalAudio[static_cast<size_t>(i)] * pedalOutputGain.getNextValue() - mono[i]);
    }
    else { pedalBlend.skip(size); pedalInputGain.skip(size); pedalOutputGain.skip(size); }
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
            const float compressed = cleanCompressor.processSample(0, dry) * compressionMakeupGain;
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
    const bool external = cab->hasExternal() && (cabinetMode == 1 || (cabinetMode == 0 && !fullRig && !natural));
    const bool builtIn = cabinetMode == 2 || (cabinetMode == 0 && !external && !fullRig && hasNoCab);
    speakerActive.store(builtIn);
    if (builtIn) speaker.process(mono, size);
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch) buffer.copyFrom(ch, 0, mono, size);
    if (external)
    {
        juce::dsp::AudioBlock<float> block(buffer); juce::dsp::ProcessContextReplacing<float> context(block); cab->process(context);
    }
    if (builtIn) { juce::dsp::AudioBlock<float> block(buffer); cab->filter(block); }
    postCabPeak.store(buffer.getMagnitude(0, 0, size));
}
void AmpSuiteAudioProcessor::requestFile(bool isModel, const juce::File& file)
{
    const juce::ScopedLock lock(requestLock);
    ++requestGeneration; pendingRig = {}; rigLoading.store(false); rigSwapReady.store(false);
    ++stageRequestGeneration[isModel ? 0 : 1];
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
        juce::String modelFile, irFile, pedalFile, irBFile; bool doModel, doIr, doPedal, doIrB;
        std::vector<std::pair<juce::File, juce::String>> imports;
        std::vector<PackJob> packs;
        juce::ValueTree completeRig; bool preserveGlobals; juce::uint64 generation;
        std::array<juce::uint64, 4> stageGenerations {};
        {
            const juce::ScopedLock lock(requestLock);
            imports.swap(pendingImports);
            packs.swap(pendingPacks);
            completeRig = pendingRig; pendingRig = {}; preserveGlobals = pendingRigPreservesGlobals; generation = requestGeneration.load();
            for (size_t i = 0; i < stageGenerations.size(); ++i) stageGenerations[i] = stageRequestGeneration[i].load();
            doModel = modelPending; doIr = irPending;
            modelFile = desiredModel; irFile = desiredIr; irBFile = desiredIrB; doIrB = irBPending; irBPending = false;
            doPedal = pedalPending; pedalFile = desiredPedal; pedalPending = false;
            modelPending = irPending = false;
        }
        if (completeRig.isValid()) prepareCompleteRig(completeRig, preserveGlobals, generation);
        for (const int stage : {0, 1, 2, 3})
        {
            const bool isModel = stage == 0, isPedal = stage == 2, isNam = isModel || isPedal;
            if (!(isPedal ? doPedal : isModel ? doModel : stage == 3 ? doIrB : doIr) || stageGenerations[stage] != stageRequestGeneration[stage].load()) continue;
            const auto path = isPedal ? pedalFile : isModel ? modelFile : stage == 3 ? irBFile : irFile;
            try
            {
                const juce::File file(path);
                if (path.isNotEmpty() && !file.existsAsFile()) throw std::runtime_error("File not found: " + path.toStdString());
                const auto asset = path.isNotEmpty() ? AssetLibrary::describe(file, isModel ? "amp" : isPedal ? "pedal" : "cab") : juce::ValueTree();
                // Storage errors must leave the previous audible stage intact.
                if (asset.isValid()) sharedStore.manage(asset);
                std::unique_ptr<NamWrapper> next;
                if (isNam && path.isNotEmpty())
                {
                    next = std::make_unique<NamWrapper>(file, assetSourceName(asset));
                    // Prepare (and prewarm) outside the DSP lock: this takes long enough
                    // that holding the lock meant many silent callbacks per load.
                    for (;;) {
                        if (threadShouldExit() || stageGenerations[stage] != stageRequestGeneration[stage].load()) break;
                        double preparedRate; int preparedBlock;
                        { const juce::ScopedLock lock(dspLock); preparedRate = rate; preparedBlock = maxBlock; }
                        next->prepare(preparedRate, preparedBlock);
                        if (isModel) next->setOutputGain(juce::Decibels::decibelsToGain(static_cast<float>(next->levelMatchDb())));
                        const juce::ScopedLock publishing(requestLock);
                        const juce::ScopedLock lock(dspLock);
                        if (rate != preparedRate || maxBlock != preparedBlock) continue;
                        if (stageGenerations[stage] != stageRequestGeneration[stage].load()) break;
                        (isPedal ? pedalExpectedRate : ampExpectedRate).store(next->expectedRate());
                        (isPedal ? pedal : model).swap(next);
                        break;
                    }
                }
                else if (isNam)
                {
                    // Clearing: detach under the lock, destroy after it (via `next`).
                    const juce::ScopedLock publishing(requestLock);
                    if (stageGenerations[stage] != stageRequestGeneration[stage].load()) continue;
                    const juce::ScopedLock lock(dspLock);
                    (isPedal ? pedalExpectedRate : ampExpectedRate).store(0);
                    (isPedal ? pedal : model).swap(next);
                }
                else if (path.isEmpty()) {
                    const juce::ScopedLock publishing(requestLock);
                    if (stageGenerations[stage] != stageRequestGeneration[stage].load()) continue;
                    cab->clear(stage == 3 ? 1 : 0);
                }
                else
                {
                    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                    if (!reader || reader->lengthInSamples <= 0 || reader->numChannels == 0 || reader->numChannels > 2
                        || reader->lengthInSamples > reader->sampleRate * 10)
                        throw std::runtime_error("Choose a mono/stereo WAV impulse response up to 10 seconds.");
                    juce::AudioBuffer<float> impulse(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                    if (!reader->read(&impulse, 0, impulse.getNumSamples(), 0, true, true)) throw std::runtime_error("Could not read the cabinet response");
                    const juce::ScopedLock publishing(requestLock);
                    if (stageGenerations[stage] != stageRequestGeneration[stage].load()) continue;
                    cab->load(std::move(impulse), reader->sampleRate, stage == 3 ? 1 : 0);
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
                const juce::ScopedLock lock(requestLock);
                if (stageGenerations[stage] != stageRequestGeneration[stage].load()) continue;
                if (asset.isValid()) library.upsert(asset);
                (isPedal ? pedalPath : isModel ? modelPath : stage == 3 ? irBPath : irPath) = path;
                if (isModel) { ampGear = gear; ampLevelDb = levelDb; ampLevelled = levelled; ampHasCab = hasCab; ampCabKnown = cabKnown; }
                if (!message.startsWith("Load failed:")) message = path.isEmpty() ? "Stage cleared" : "Loaded " + file.getFileName();
                if (asset.isValid()) persistLibrary();
            }
            catch (const std::exception& e)
            {
                const juce::ScopedLock lock(requestLock);
                if (stageGenerations[stage] == stageRequestGeneration[stage].load()) message = "Load failed: " + juce::String(e.what());
            }
        }
        int imported = 0; juce::StringArray errors;
        for (const auto& item : imports)
        {
            if (threadShouldExit()) break;
            const auto& file = item.first; const auto& kind = item.second;
            if (kind == "pack") {
                const auto result = SoundPack::read(file, [&](auto asset) {
                    sharedStore.manage(asset);
                    if (static_cast<bool>(asset["managed"])) asset.setProperty("aliases", "[]", nullptr);
                    const juce::ScopedLock lock(requestLock); library.upsert(asset);
                }); imported += result.imported; errors.addArray(result.errors); continue;
            }
            try {
                if (!file.existsAsFile() || !file.hasFileExtension((kind == "cab" || kind == "ambience") ? "wav" : "nam"))
                    throw std::runtime_error("Invalid asset file");
                // Validate imports off the audio thread without changing the playing rig.
                if (kind != "cab" && kind != "ambience") { NamWrapper validation(file); }
                else {
                    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                    if (!reader || reader->lengthInSamples <= 0 || reader->numChannels == 0 || reader->numChannels > 2 || reader->lengthInSamples > reader->sampleRate * (kind == "ambience" ? 30 : 10))
                        throw std::runtime_error(kind == "ambience" ? "Choose a mono/stereo ambience response up to 30 seconds" : "Choose a mono/stereo cabinet response up to 10 seconds");
                }
                const auto asset = AssetLibrary::describe(file, kind);
                sharedStore.manage(asset);
                const juce::ScopedLock lock(requestLock); library.upsert(asset); ++imported;
            } catch (const std::exception& e) { errors.add(file.getFileName() + ": " + juce::String(e.what())); }
        }
        if (!imports.empty()) {
            const juce::ScopedLock lock(requestLock);
            message = errors.isEmpty() ? "Imported " + juce::String(imported) + " files into the library"
                : "Load failed: " + errors.joinIntoString("; ").substring(0, 1200) + " (" + juce::String(imported) + " imported)";
            persistLibrary();
        }
        for (const auto& job : packs) {
            const auto error = job.save ? exportRigPack(job.file, job.snapshot) : importRigPack(job.file);
            const juce::ScopedLock lock(requestLock); message = error.isEmpty() ? (job.save ? "Portable rig pack exported" : "Rig pack imported · open Library Presets to load it") : "Load failed: " + error;
        }
        wait(50);
    }
}
juce::var AmpSuiteAudioProcessor::status()
{
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("practice", practice.status());
    result->setProperty("takes", takes.status());
    result->setProperty("review", takeReview.status());
    result->setProperty("midi", midiControl.status());
    result->setProperty("scenes", scenes.status(apvts));
    {
        const juce::ScopedLock lock(requestLock);
        const auto displayName = [&](const juce::String& kind, const juce::String& path) {
            const auto asset = library.find(library.idForPath(kind, path));
            return sharedStore.enabled() && path.isNotEmpty() && asset["name"].toString().isNotEmpty()
                ? asset["name"].toString() : juce::File(path).getFileName();
        };
        result->setProperty("model", displayName("amp", modelPath));
        result->setProperty("pedal", displayName("pedal", pedalPath));
        result->setProperty("irB", displayName("cab", irBPath));
        result->setProperty("ir", displayName("cab", irPath)); result->setProperty("message", message);
        result->setProperty("libraryRevision", library.revision);
        result->setProperty("modelId", library.idForPath("amp", desiredModel));
        result->setProperty("rigLoading", sceneAssetsLoading());
        result->setProperty("board", boardStatus());
        result->setProperty("activeRigId", activeRig.id); result->setProperty("activeRigName", activeRig.name);
        const bool saved = library.find(activeRig.id).hasType("RIG");
        result->setProperty("activeRigSaved", saved);
        result->setProperty("activeRigEdited", activeRig.edited(apvts, {desiredModel, desiredIr, desiredPedal, desiredIrB, desiredPedal1, desiredAmbience, desiredAmbience1},
            {library.idForPath("amp", desiredModel), library.idForPath("cab", desiredIr), library.idForPath("pedal", desiredPedal), library.idForPath("cab", desiredIrB), library.idForPath("pedal", desiredPedal1), library.idForPath("ambience", desiredAmbience), library.idForPath("ambience", desiredAmbience1)}, scenes.save()) || (!saved && activeRig.name.isNotEmpty()));
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
    result->setProperty("compressionDb", reportedCompression.load());
    result->setProperty("overdriveLatencySamples", value(Params::odOn) >= .5f ? reportedOverdriveLatency.load() : 0);
    result->setProperty("swapBypasses", swapBypasses.load());
    result->setProperty("gate", gateLevel.load());
    result->setProperty("cpu", processLoad.getLoadAsPercentage());
    result->setProperty("overruns", processLoad.getXRunCount());
    result->setProperty("inputClipped", inputClipped.load());
    result->setProperty("outputPeakWarning", outputPeakWarning.load());
    // Driver-level dropouts (standalone only; -1 when the device cannot report them).
    result->setProperty("dropouts", deviceDropouts ? deviceDropouts() : -1);
    result->setProperty("deviceSettingsAvailable", static_cast<bool>(showDeviceSettings));
    if (deviceInputChannels && deviceSelectedInput) {
        juce::Array<juce::var> names; for (const auto& name : deviceInputChannels()) names.add(name);
        result->setProperty("inputChannels", names); result->setProperty("selectedInput", deviceSelectedInput());
    }
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
    result->setProperty("humCancelling", reportedHum.load(std::memory_order_relaxed));
    result->setProperty("mainsHz", reportedMains.load(std::memory_order_relaxed));

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
    auto state = copyRigState(true); juce::ValueTree midi("MIDICONTROL");
    midi.setProperty("json", juce::JSON::toString(midiControl.configuration()), nullptr); state.addChild(midi, -1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}
juce::ValueTree AmpSuiteAudioProcessor::copyRigState(bool includeSavedRigs)
{
    const juce::ScopedLock snapshotGuard(requestLock);
    auto state = apvts.copyState();
    PedalboardState::migrate(state);
    if (const auto oldIdentity = state.getChildWithName("ACTIVE_RIG"); oldIdentity.isValid()) state.removeChild(oldIdentity, nullptr);
    state.addChild(activeRig.save(), -1, nullptr);
    if (const auto oldMidi = state.getChildWithName("MIDICONTROL"); oldMidi.isValid()) state.removeChild(oldMidi, nullptr);
    if (const auto oldScenes = state.getChildWithName("SCENES"); oldScenes.isValid()) state.removeChild(oldScenes, nullptr);
    state.addChild(scenes.save(), -1, nullptr);
    // Preserve requested paths during asynchronous recall, including missing files.
    {
        const juce::ScopedLock lock(requestLock);
        state.setProperty("irBPath", desiredIrB, nullptr);
        state.setProperty("irBId", library.idForPath("cab", desiredIrB), nullptr);
        state.setProperty("modelPath", desiredModel, nullptr); state.setProperty("irPath", desiredIr, nullptr); state.setProperty("pedalPath", desiredPedal, nullptr);
        state.setProperty("modelId", library.idForPath("amp", desiredModel), nullptr);
        state.setProperty("irId", library.idForPath("cab", desiredIr), nullptr);
        state.setProperty("pedalId", library.idForPath("pedal", desiredPedal), nullptr);
        state.setProperty("ambiencePath", desiredAmbience, nullptr); state.setProperty("ambienceId", library.idForPath("ambience", desiredAmbience), nullptr);
        state.setProperty("ambience1Path", desiredAmbience1, nullptr); state.setProperty("ambience1Id", library.idForPath("ambience", desiredAmbience1), nullptr);
        state.setProperty("pedal1Path", desiredPedal1, nullptr); state.setProperty("pedal1Id", library.idForPath("pedal", desiredPedal1), nullptr);
        for (const auto* stage : {"model", "ir", "pedal", "irB", "pedal1", "ambience", "ambience1"}) {
            const auto reference = library.find(state[juce::String(stage) + "Id"].toString());
            if (reference.isValid() && static_cast<bool>(reference["managed"]) && AssetLibrary::exists(reference["path"].toString()))
                state.setProperty(juce::String(stage) + "Path", reference["path"], nullptr);
        }
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
    const juce::ScopedLock stateGuard(requestLock);
    auto state = juce::ValueTree::fromXml(*xml);
    // Sparse legacy host states remain supported. Present routing metadata must
    // be understood before identities, scenes, MIDI, assets or parameters change.
    auto failure = PedalboardState::migrate(state);
    int sceneBanks = 0;
    for (const auto& child : state) if (child.hasType("SCENES") && ++sceneBanks > 1) failure = "Duplicate scene bank.";
    if (failure.isEmpty()) failure = PerformanceScenes::validateBoards(state.getChildWithName("SCENES"));
    if (failure.isNotEmpty()) { message = "Session not restored: " + failure; return; }
    bool completeFixed = PedalboardState::serial(apvts.state);
    for (const auto& p : Params::definitions) if (!state.getChildWithProperty("id", p.id).isValid()) completeFixed = false;
    if (PedalboardState::serial(state) || (completeFixed && PerformanceScenes::migrate(state).isEmpty())) {
        if (!PedalboardState::serial(state)) BoardParams::addDefaults(state, false);
        auto rig = std::make_unique<juce::DynamicObject>(); rig->setProperty("schema", 3); rig->setProperty("state", state.toXmlString());
        const auto result = applyRig(juce::var(rig.release()), false);
        if (result.isNotEmpty()) message = "Session not restored: " + result;
        return;
    }
    if (const auto result = BoardParams::addDefaults(state, false); result.isNotEmpty()) { message = "Session not restored: " + result; return; }
    // Malformed old scene banks still clear/report their own error as before.
    // Valid banks are upgraded on this isolated tree, without rewriting files.
    PerformanceScenes::migrate(state);
    activeRig.restore(state.getChildWithName("ACTIVE_RIG"));
    scenes.restore(state.getChildWithName("SCENES"));
    const auto midi = state.getChildWithName("MIDICONTROL");
    const auto midiJson = midi["json"].toString();
    const auto parsedMidi = midiJson.length() <= 32768 ? juce::JSON::parse(midiJson) : juce::var();
    const auto midiFailure = midiControl.restore(midi.isValid() ? (parsedMidi.isObject() ? parsedMidi : juce::var("invalid")) : juce::var());
    juce::ignoreUnused(midiFailure);
    if (midi.isValid()) state.removeChild(midi, nullptr);
    {
        const juce::ScopedLock lock(requestLock);
        library.merge(state.getChildWithName("LIBRARY"));
        activeRig.refreshSavedBaseline(library.find(activeRig.id));
        for (const auto& stage : {"model", "ir", "pedal", "irB", "pedal1", "ambience", "ambience1"})
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
    std::unique_ptr<SerialPedalboard> retiredBoard; std::unique_ptr<NamWrapper> retiredPedal;
    { const juce::ScopedLock audioGuard(dspLock); retiredBoard.swap(serialBoard); retiredPedal.swap(pedal1); }
    desiredPedal1.clear(); pedal1Path.clear(); desiredAmbience.clear(); ambiencePath.clear(); desiredAmbience1.clear(); ambience1Path.clear(); boardUndo.clear(); boardRedo.clear(); pendingBoardBefore = {}; pendingScene = -1;
    apvts.replaceState(state);
    { const juce::ScopedLock lock(requestLock);
      ++requestGeneration; pendingRig = {}; rigLoading.store(false); rigSwapReady.store(false);
      for (auto& sequence : stageRequestGeneration) ++sequence;
      desiredModel = state.getProperty("modelPath").toString(); desiredIr = state.getProperty("irPath").toString();
      desiredPedal = state.getProperty("pedalPath").toString(); desiredIrB = state.getProperty("irBPath").toString();
      modelPending = irPending = pedalPending = irBPending = true; message = "Restoring assets..."; }
    notify();
}
juce::AudioProcessorEditor* AmpSuiteAudioProcessor::createEditor() { return new AmpSuiteAudioProcessorEditor(*this); }
juce::String AmpSuiteAudioProcessor::handleMidiAction(const MidiControl::Mapping& mapping, int amount)
{
    if (mapping.action == "rig") return loadRig(mapping.rig);
    if (mapping.action == "scene") return recallScene(mapping.scene);
    if (rigLoading.load()) return "Rig is preparing; try the control again when it is ready.";
    const char* id = nullptr; float target = 0;
    const auto& action = mapping.action;
    if (action == "master" || action == "drive" || action == "reverb" || action == "delay") {
        const float fraction = (mapping.inverted ? 127 - amount : amount) / 127.f;
        if (action == "master") { id = "MASTER_VOL"; target = -60 + fraction * 60; }
        else if (action == "drive") { id = "DRIVE_GAIN"; target = fraction * 24; }
        else { id = action == "reverb" ? "REVERB_MIX" : "DELAY_MIX"; target = fraction * 100; }
    } else {
        id = action == "overdrive" ? "OD_ON" : action == "pedal" ? "PEDAL_ON" : action == "eq" ? "EQ_ON" : action == "gate" ? "GATE_ON" : action == "metronome" ? "METRO_ON" : action == "modulation" ? "MOD_ON" : nullptr;
        if (id != nullptr) target = apvts.getRawParameterValue(id)->load() >= .5f ? 0.f : 1.f;
    }
    if (id == nullptr) return "Unknown MIDI action.";
    auto* parameter = apvts.getParameter(id);
    parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(target)); parameter->endChangeGesture(); return {};
}

bool AmpSuiteAudioProcessor::sceneAssetsLoading() const
{
    return rigLoading.load() || modelPending || pedalPending || irPending || irBPending
        || desiredModel != modelPath || desiredPedal != pedalPath || desiredIr != irPath || desiredIrB != irBPath || desiredPedal1 != pedal1Path || desiredAmbience != ambiencePath || desiredAmbience1 != ambience1Path;
}
juce::String AmpSuiteAudioProcessor::storeScene(int slot, const juce::String& name)
{
    const juce::ScopedLock guard(requestLock);
    if (sceneAssetsLoading()) return "Finish loading the rig before storing a scene.";
    return scenes.store(slot, name, apvts);
}
juce::String AmpSuiteAudioProcessor::recallScene(int slot)
{
    const juce::ScopedLock guard(requestLock);
    if (sceneAssetsLoading()) return "Finish loading the rig before recalling a scene.";
    auto state = copyRigState(false);
    if (const auto failure = scenes.applySnapshot(slot, state); failure.isNotEmpty()) return failure;
    if (PedalboardState::equal(state, apvts.state) || (!PedalboardState::serial(state) && !PedalboardState::serial(apvts.state))) return scenes.recall(slot, apvts);
    auto rig = std::make_unique<juce::DynamicObject>(); rig->setProperty("schema", 3); rig->setProperty("state", state.toXmlString());
    if (const auto failure = applyRig(juce::var(rig.release())); failure.isNotEmpty()) return failure;
    pendingScene = slot; return {};
}
juce::String AmpSuiteAudioProcessor::clearScene(int slot)
{
    const juce::ScopedLock guard(requestLock);
    if (sceneAssetsLoading()) return "Finish loading the rig before clearing a scene.";
    return scenes.clear(slot);
}
void AmpSuiteAudioProcessor::requestPedal(const juce::File& file)
{
    const juce::ScopedLock lock(requestLock);
    ++requestGeneration; pendingRig = {}; rigLoading.store(false); rigSwapReady.store(false);
    ++stageRequestGeneration[2];
    desiredPedal = file.getFullPathName(); pedalPending = true;
    message = file == juce::File() ? "Removing stage..." : "Loading " + file.getFileName() + "..."; notify();
}

bool AmpSuiteAudioProcessor::selectAmpVoice(const juce::String& voice)
{
    if (voice != "Blue-I" && voice != "Red-I") return false;
    juce::String current;
    {
        const juce::ScopedLock lock(requestLock); current = desiredModel;
        const auto asset = library.find(library.idForPath("amp", current));
        const auto aliases = juce::JSON::parse(asset["aliases"].toString());
        if (aliases.isArray()) for (const auto& alias : *aliases.getArray())
            if (juce::File(alias.toString()).getFileName().startsWith("APP-5153-Ivory-") && AssetLibrary::exists(alias.toString())) { current = alias.toString(); break; }
        if (asset["name"].toString().startsWith("APP-5153-Ivory-") || juce::File(current).getFileName().startsWith("APP-5153-Ivory-"))
            for (const auto& candidate : library.tree)
                if (candidate["kind"].toString() == "amp" && candidate["name"].toString() == "APP-5153-Ivory-" + voice && AssetLibrary::exists(candidate["path"].toString())) {
                    requestFile(true, juce::File(candidate["path"].toString())); return true;
                }
    }
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
    const juce::ScopedLock lock(requestLock);
    try {
        const auto shared = sharedStore.load();
        if (shared.isValid()) {
            juce::StringArray removed; removed.addTokens(shared["removedIds"].toString(), ",", "");
            for (const auto& id : removed) { const auto item = library.find(id); if (item.isValid()) library.tree.removeChild(item, nullptr); }
            for (const auto& item : shared) {
                const auto existing = library.find(item["id"].toString());
                if (existing.isValid()) library.tree.removeChild(existing, nullptr);
            }
            library.merge(shared);
        }
    } catch (const std::exception& e) { message = "Load failed: " + juce::String(e.what()); }
    return library.list();
}
juce::String AmpSuiteAudioProcessor::persistLibrary(const juce::StringArray& removed)
{
    const juce::ScopedLock lock(requestLock);
    try { sharedStore.save(library.tree, removed); return {}; }
    catch (const std::exception& e) { message = "Load failed: " + juce::String(e.what()); return e.what(); }
}
juce::String AmpSuiteAudioProcessor::assetSourceName(const juce::ValueTree& asset, const juce::ValueTree& incoming)
{
    const juce::ScopedLock lock(requestLock);
    auto reference = library.find(asset["id"].toString());
    if (!reference.isValid()) reference = incoming.getChildWithProperty("id", asset["id"]);
    if (!reference.isValid()) reference = asset;
    if (reference.hasProperty("sourceName")) return reference["sourceName"].toString();
    // Older catalogs kept original paths as aliases but allowed editing the name.
    const auto aliases = juce::JSON::parse(reference["aliases"].toString());
    if (aliases.isArray()) for (const auto& alias : *aliases.getArray()) {
        const auto name = juce::File(alias.toString()).getFileNameWithoutExtension();
        if (name.length() != 64 || name.removeCharacters("0123456789abcdef").isNotEmpty()) return name;
    }
    return reference["name"].toString();
}
juce::String AmpSuiteAudioProcessor::validateRigDocument(const juce::var& rig) { juce::ValueTree state; return readRig(rig, state); }
juce::String AmpSuiteAudioProcessor::migrateRigDocument(const juce::var& rig, juce::ValueTree& state) { return readRig(rig, state); }
void AmpSuiteAudioProcessor::importAssets(const juce::Array<juce::File>& files, const juce::String& kind)
{
    if (kind != "amp" && kind != "pedal" && kind != "cab" && kind != "ambience" && kind != "pack") return;
    const juce::ScopedLock lock(requestLock);
    for (const auto& file : files) pendingImports.emplace_back(file, kind);
    if (!files.isEmpty()) message = "Importing library files...";
    notify();
}
juce::var AmpSuiteAudioProcessor::getRig()
{
    {
        const juce::ScopedLock lock(requestLock);
        if (rigLoading.load() || desiredModel != modelPath || desiredIr != irPath || desiredIrB != irBPath || desiredPedal != pedalPath || desiredPedal1 != pedal1Path || desiredAmbience != ambiencePath || desiredAmbience1 != ambience1Path)
        {
            auto result = std::make_unique<juce::DynamicObject>(); result->setProperty("error", "Finish loading or relinking the assets before saving/comparing this rig.");
            return juce::var(result.release());
        }
    }
    auto result = std::make_unique<juce::DynamicObject>();
    const auto state = copyRigState(false);
    if (const auto failure = PedalboardState::validate(state); failure.isNotEmpty()) {
        result->setProperty("error", failure); return juce::var(result.release());
    }
    result->setProperty("schema", 3); result->setProperty("state", state.createXml()->toString());
    const auto guitar = guitarPower.load(), input = inputPower.load();
    if (guitar > 1e-6f && input > 1e-6f) {
        result->setProperty("guitarRmsDb", 10 * std::log10(guitar)); result->setProperty("inputRmsDb", 10 * std::log10(input));
    }
    return juce::var(result.release());
}
juce::String AmpSuiteAudioProcessor::applyRig(const juce::var& rig, bool preserveGlobals, bool matchLoudness)
{
    juce::ValueTree state;
    if (const auto error = readRig(rig, state); error.isNotEmpty()) return error;
    if (matchLoudness && rig.hasProperty("guitarRmsDb") && rig.hasProperty("inputRmsDb")) {
        const float guitar = guitarPower.load(), input = inputPower.load();
        const double original = static_cast<double>(rig["guitarRmsDb"]) - static_cast<double>(rig["inputRmsDb"]);
        if (guitar > 1e-6f && input > 1e-6f && std::isfinite(original)) {
            auto output = state.getChildWithProperty("id", "AMP_OUT");
            const float correction = juce::jlimit(-12.0f, 12.0f, 10 * std::log10(guitar / input) - static_cast<float>(original));
            output.setProperty("value", juce::jlimit(-24.0f, 12.0f, static_cast<float>(output["value"]) + correction), nullptr);
        }
    }
    // Resolve stable IDs before changing any parameters. Missing references leave
    // the current rig untouched, rather than playing new settings through old files.
    {
        const juce::ScopedLock lock(requestLock);
        for (const auto& stage : {"model", "ir", "pedal", "irB", "pedal1", "ambience", "ambience1"})
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
            if (!juce::File(path).hasFileExtension((juce::String(stage) == "ir" || juce::String(stage) == "irB" || juce::String(stage).startsWith("ambience")) ? "wav" : "nam")) return "Invalid asset format in rig.";
            const auto id = state[juce::String(stage) + "Id"].toString();
            const auto kind = juce::String(stage).startsWith("ambience") ? "ambience" : juce::String(stage) == "model" ? "amp" : (juce::String(stage) == "ir" || juce::String(stage) == "irB") ? "cab" : "pedal";
            if (id.isNotEmpty() && id != juce::String(kind) + ":" + juce::SHA256(juce::File(path)).toHexString())
                return "The " + juce::String(stage) + " file has changed. Relink the original asset or import the changed file as a new asset.";
        }
    }
    {
        const juce::ScopedLock lock(requestLock);
        pendingBoardBefore = {}; pendingScene = -1;
        pendingRig = state; pendingRigPreservesGlobals = preserveGlobals; ++requestGeneration;
        for (auto& sequence : stageRequestGeneration) ++sequence;
        modelPending = irPending = pedalPending = irBPending = false; rigLoading.store(true);
        message = "Preparing complete rig...";
    }
    notify(); return {};
}
juce::String AmpSuiteAudioProcessor::saveRig(const juce::String& name)
{
    const auto title = name.trim().substring(0, 80);
    if (title.isEmpty()) return "Give the rig a name.";
    const auto rig = getRig(); if (rig.hasProperty("error")) return rig["error"].toString();
    const juce::ScopedLock lock(requestLock);
    juce::ValueTree entry("RIG");
    entry.setProperty("id", juce::Uuid().toString(), nullptr); entry.setProperty("name", title, nullptr);
    entry.setProperty("schema", 3, nullptr);
    entry.setProperty("state", rig["state"], nullptr); entry.setProperty("favorite", false, nullptr);
    library.tree.addChild(entry, -1, nullptr); ++library.revision;
    const auto error = persistLibrary();
    if (error.isEmpty()) activeRig.set(entry["id"].toString(), title, juce::ValueTree::fromXml(*juce::XmlDocument::parse(rig["state"].toString())));
    else library.tree.removeChild(entry, nullptr);
    return error;
}
juce::String AmpSuiteAudioProcessor::updateActiveRig()
{
    const auto snapshot = getRig(); if (snapshot.hasProperty("error")) return snapshot["error"].toString();
    const juce::ScopedLock lock(requestLock); auto entry = library.find(activeRig.id);
    if (!entry.hasType("RIG")) return "Save this tone as a new complete rig first.";
    const auto previous = entry["state"], previousSchema = entry["schema"]; const bool hadSchema = entry.hasProperty("schema");
    entry.setProperty("state", snapshot["state"], nullptr); entry.setProperty("schema", 3, nullptr); ++library.revision;
    const auto error = persistLibrary();
    if (error.isEmpty()) activeRig.set(activeRig.id, entry["name"].toString(), juce::ValueTree::fromXml(*juce::XmlDocument::parse(snapshot["state"].toString())));
    else { entry.setProperty("state", previous, nullptr); if (hadSchema) entry.setProperty("schema", previousSchema, nullptr); else entry.removeProperty("schema", nullptr); }
    return error;
}
juce::String AmpSuiteAudioProcessor::loadRig(const juce::String& id)
{
    juce::var state; int schema = 1;
    { const juce::ScopedLock lock(requestLock); const auto rig = library.find(id); if (!rig.hasType("RIG")) return "Rig not found.";
      if (rig.hasProperty("schema")) {
          const auto stored = rig["schema"].toString(); if (stored != "1" && stored != "2" && stored != "3") return "Unsupported saved rig format.";
          schema = stored.getIntValue();
      }
      const auto xml = juce::XmlDocument::parse(rig["state"].toString()); if (!xml) return "Invalid saved rig.";
      auto document = juce::ValueTree::fromXml(*xml);
      auto saved = std::make_unique<juce::DynamicObject>(); saved->setProperty("schema", schema); saved->setProperty("state", document.toXmlString());
      if (const auto failure = readRig(juce::var(saved.release()), document); failure.isNotEmpty()) return failure;
      ActiveRig identity; identity.set(id, rig["name"].toString(), document);
      const auto old = document.getChildWithName("ACTIVE_RIG"); if (old.isValid()) document.removeChild(old, nullptr);
      document.addChild(identity.save(), -1, nullptr); state = document.createXml()->toString(); }
    auto rig = std::make_unique<juce::DynamicObject>(); rig->setProperty("schema", schema); rig->setProperty("state", state);
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
    entry.setProperty("schema", 3, nullptr);
    entry.setProperty("name", name.trim().substring(0, 80), nullptr); entry.setProperty("state", state.createXml()->toString(), nullptr);
    library.tree.addChild(entry, -1, nullptr); ++library.revision;
    return persistLibrary();
}
bool AmpSuiteAudioProcessor::removeRig(const juce::String& id)
{
    const juce::ScopedLock lock(requestLock); auto rig = library.find(id); if (!rig.hasType("RIG")) return false;
    library.tree.removeChild(rig, nullptr); ++library.revision; return persistLibrary({id}).isEmpty();
}
bool AmpSuiteAudioProcessor::editAsset(const juce::String& id, const juce::var& changes)
{
    const juce::ScopedLock lock(requestLock); auto asset = library.find(id); if (!asset.isValid()) return false;
    for (const auto& key : {"name", "creator", "tags", "sourceURL", "notes"})
        if (changes.hasProperty(key) && changes[key].isString()) asset.setProperty(key, changes[key].toString().substring(0, 1000), nullptr);
    if (changes["favorite"].isBool()) asset.setProperty("favorite", changes["favorite"], nullptr);
    ++library.revision; return persistLibrary().isEmpty();
}
bool AmpSuiteAudioProcessor::selectAsset(const juce::String& id, bool cabinetB)
{
    juce::ValueTree asset;
    { const juce::ScopedLock lock(requestLock); asset = library.find(id).createCopy(); }
    if (!asset.hasType("ASSET") || !juce::File(asset["path"].toString()).existsAsFile()) return false;
    const auto kind = asset["kind"].toString(); const juce::File file(asset["path"].toString());
    if (kind == "amp") { requestFile(true, file); setParameterValue("CAPTURE_KIND", static_cast<float>(asset["captureKind"])); setParameterValue("AMP_SOURCE", 3); }
    else if (kind == "pedal") {
        const juce::ScopedLock guard(requestLock);
        if (PedalboardState::serial(apvts.state)) {
            for (const auto& row : apvts.state.getChildWithName("PEDALBOARD")) if (PedalboardState::kind(row) == 2 && static_cast<int>(row["deleted"]) == 0) {
                auto args = std::make_unique<juce::DynamicObject>(); args->setProperty("id", row["id"]); args->setProperty("assetId", id); args->setProperty("enable", true);
                return boardCommand("capture", juce::var(args.release())).isEmpty();
            }
            return false;
        }
        requestPedal(file); setParameterValue("PEDAL_ON", 1);
    }
    else if (kind == "ambience") {
        const juce::ScopedLock guard(requestLock);
        if (!PedalboardState::serial(apvts.state)) return false;
        for (const auto& row : apvts.state.getChildWithName("PEDALBOARD")) if (PedalboardState::kind(row) == 8 && static_cast<int>(row["deleted"]) == 0) {
            auto args = std::make_unique<juce::DynamicObject>(); args->setProperty("id", row["id"]); args->setProperty("assetId", id);
            return boardCommand("capture", juce::var(args.release())).isEmpty();
        }
        return false;
    }
    else if (kind == "cab") { if (cabinetB) { requestCabB(file); setParameterValue("CAB_B_ON", 1); } else requestFile(false, file); setParameterValue("CAB_MODE", 1); }
    else return false;
    return true;
}
juce::String AmpSuiteAudioProcessor::relinkAsset(const juce::String& id, const juce::File& file)
{
    juce::String kind;
    { const juce::ScopedLock lock(requestLock); const auto asset = library.find(id); if (!asset.hasType("ASSET")) return "Asset not found."; kind = asset["kind"].toString(); }
    if (!file.existsAsFile()) return "File not found.";
    if (kind + ":" + juce::SHA256(file).toHexString() != id) return "That file has different content. Choose the original asset, or import it as a new one.";
    const juce::ScopedLock lock(requestLock); auto asset = library.find(id).createCopy();
    const bool activeAmp = kind == "amp" && library.idForPath(kind, desiredModel) == id;
    const bool activeCab = kind == "cab" && library.idForPath(kind, desiredIr) == id;
    const bool activeCabB = kind == "cab" && library.idForPath(kind, desiredIrB) == id;
    const bool activePedal = kind == "pedal" && library.idForPath(kind, desiredPedal) == id;
    const bool activePedal1 = kind == "pedal" && library.idForPath(kind, desiredPedal1) == id;
    const bool activeAmbience = kind == "ambience" && library.idForPath(kind, desiredAmbience) == id;
    const bool activeAmbience1 = kind == "ambience" && library.idForPath(kind, desiredAmbience1) == id;
    AssetLibrary::rememberPath(asset, file.getFullPathName());
    try { sharedStore.manage(asset); } catch (const std::exception& e) { return e.what(); }
    library.upsert(asset);
    if (const auto error = persistLibrary(); error.isNotEmpty()) return error;
    if (activePedal1 || activeAmbience || activeAmbience1) {
        auto state = copyRigState(false);
        if (activePedal) state.setProperty("pedalPath", asset["path"], nullptr);
        if (activePedal1) state.setProperty("pedal1Path", asset["path"], nullptr);
        if (activeAmbience) state.setProperty("ambiencePath", asset["path"], nullptr);
        if (activeAmbience1) state.setProperty("ambience1Path", asset["path"], nullptr);
        auto rig = std::make_unique<juce::DynamicObject>(); rig->setProperty("schema", 3); rig->setProperty("state", state.toXmlString());
        return applyRig(juce::var(rig.release()));
    }
    if (activeAmp || activeCab || activeCabB || activePedal) { ++requestGeneration; pendingRig = {}; rigLoading.store(false); rigSwapReady.store(false); }
    if (activeAmp) ++stageRequestGeneration[0];
    if (activeCab) ++stageRequestGeneration[1];
    if (activeCabB) ++stageRequestGeneration[3];
    if (activePedal) ++stageRequestGeneration[2];
    if (activeAmp) { desiredModel = file.getFullPathName(); modelPending = true; }
    if (activeCab) { desiredIr = file.getFullPathName(); irPending = true; }
    if (activeCabB) { desiredIrB = file.getFullPathName(); irBPending = true; }
    if (activePedal) { desiredPedal = file.getFullPathName(); pedalPending = true; }
    notify(); return {};
}

void AmpSuiteAudioProcessor::requestCabB(const juce::File& file)
{
    const juce::ScopedLock lock(requestLock);
    ++requestGeneration; pendingRig = {}; rigLoading.store(false); rigSwapReady.store(false);
    ++stageRequestGeneration[3]; desiredIrB = file.getFullPathName(); irBPending = true;
    message = file == juce::File() ? "Removing cabinet B..." : "Loading cabinet B..."; notify();
}
DualCab::Settings AmpSuiteAudioProcessor::cabinetSettings() const
{
    return {value(Params::cabBOn) >= .5f, value(Params::cabBlend), value(Params::cabALevel), value(Params::cabBLevel),
        value(Params::cabAPan), value(Params::cabBPan), value(Params::cabAInvert) >= .5f, value(Params::cabBInvert) >= .5f,
        value(Params::cabADelay), value(Params::cabBDelay), value(Params::cabLowCut), value(Params::cabHighCut)};
}
ModulationPedal::Settings AmpSuiteAudioProcessor::modulationSettings() const
{
    const float hz = value(Params::modSync) >= .5f
        ? ModulationPedal::syncedRate(static_cast<float>(metronomeBpm.load()), juce::roundToInt(value(Params::modDivision)))
        : value(Params::modRate);
    return {value(Params::modOn) >= .5f, juce::roundToInt(value(Params::modType)), hz,
        value(Params::modDepth), value(Params::modMix), value(Params::modFeedback), value(Params::modStereo)};
}
StudioCompressor::Settings AmpSuiteAudioProcessor::compressorSettings(bool enabled) const
{
    return {enabled ? value(Params::cleanComp) : 0, value(Params::compThreshold), value(Params::compRatio),
        value(Params::compAttack), value(Params::compRelease), value(Params::compMakeup)};
}
