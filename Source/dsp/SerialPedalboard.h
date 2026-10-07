#pragma once
#include "../PedalboardState.h"
#include "StudioCompressor.h"
#include "Overdrive.h"
#include "PedalEq.h"
#include "ModulationPedal.h"
#include "StereoChorus.h"
#include "NamWrapper.h"
#include "WahPedal.h"
#include "DistortionPedal.h"
#include "PlateReverb.h"
#include "SpringReverb.h"

// Construct/prepare/retire on the loader thread. The callback reads cached
// atomic parameters and uses bounded scratch storage; it never touches trees.
class SerialPedalboard {
    struct Node {
        int kind = 0, slot = 0, channels = 1; bool pre = true;
        std::vector<std::atomic<float>*> values;
        std::atomic<float>* enabled = nullptr; std::atomic<float>* trim = nullptr;
        juce::SmoothedValue<float> blend, gain, time, mix, feedback, width, predelay, captureInput, captureOutput;
        juce::AudioBuffer<float> wet, room;
        StudioCompressor compressor; Overdrive drive; PedalEq eq;
        ModulationPedal modulation; StereoChorus chorus;
        WahPedal wah; DistortionPedal distortion; PlateReverb plate; SpringReverb spring;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay, roomDelay;
        juce::dsp::Reverb reverb;
        std::unique_ptr<juce::dsp::Convolution> ambience;
        double rate = 48000;
        float v(size_t i) const { return values[i]->load(std::memory_order_relaxed); }
        void configure(float bpm) {
            switch (kind) {
                case 0: compressor.configure({v(0), v(1), v(2), v(3), v(4), v(5)}); break;
                case 1: drive.configure({true, v(1), v(2), v(3), v(4)}); break;
                case 2: captureInput.setTargetValue(juce::Decibels::decibelsToGain(v(1))); captureOutput.setTargetValue(juce::Decibels::decibelsToGain(v(2))); break;
                case 3: eq.configure({true, v(1), v(2), v(3), v(4)}); break;
                case 4: modulation.configure({true, juce::roundToInt(v(1)), v(7) >= .5f ? ModulationPedal::syncedRate(bpm, juce::roundToInt(v(8))) : v(2), v(3), v(4), v(5), v(6)}); break;
                case 5: chorus.configure({v(0), v(1), v(2)}); break;
                case 6: {
                    constexpr float divisions[] {1, .5f, .75f, .25f, 2, 4};
                    const auto milliseconds = v(4) >= .5f ? 60000.f * divisions[juce::jlimit(0, 5, juce::roundToInt(v(5)))] / juce::jlimit(20.f, 400.f, bpm) : v(0);
                    time.setTargetValue(milliseconds * static_cast<float>(rate) / 1000); mix.setTargetValue(v(1) / 100); width.setTargetValue(v(2) / 100); feedback.setTargetValue(v(3) / 100); break;
                }
                case 7: {
                    juce::dsp::Reverb::Parameters rv; const int voice = juce::roundToInt(v(2));
                    rv.roomSize = voice == 0 ? v(1) / 100 : voice == 1 ? .45f + v(1) * .004f : .7f + v(1) * .0029f;
                    rv.damping = v(3) / 100; rv.wetLevel = v(0) / 100; rv.dryLevel = 0; reverb.setParameters(rv);
                    mix.setTargetValue(v(0) / 100); predelay.setTargetValue(v(4) * static_cast<float>(rate) / 1000); break;
                }
                case 12: spring.configure({v(0), v(1), v(2), v(3), v(4)}); break;
                case 11: plate.configure({v(0), v(1), v(2), v(3), v(4)}); break;
                case 10: distortion.configure({v(0), v(1), v(2), v(3), v(4)}); break;
                case 9: wah.configure({juce::roundToInt(v(0)), v(1), v(2), v(3), v(4)}); break;
                default: break;
            }
        }
        void prepare(const juce::ValueTree& state, juce::AudioProcessorValueTreeState& apvts, const juce::dsp::ProcessSpec& spec, float bpm) {
            rate = spec.sampleRate; channels = static_cast<int>(spec.numChannels);
            const auto initial = [&](const juce::String& id) { return static_cast<float>(state.getChildWithProperty("id", id)["value"]); };
            enabled = apvts.getRawParameterValue(BoardParams::onId(kind, slot)); trim = apvts.getRawParameterValue(BoardParams::trimId(kind, slot));
            std::vector<float> saved;
            for (const auto& id : BoardParams::controls[static_cast<size_t>(kind)]) {
                const auto hostId = BoardParams::parameter(kind, slot, id); values.push_back(apvts.getRawParameterValue(hostId)); saved.push_back(initial(hostId));
            }
            wet.setSize(channels, static_cast<int>(spec.maximumBlockSize)); room.setSize(channels, static_cast<int>(spec.maximumBlockSize));
            for (auto* p : {&blend, &gain, &time, &mix, &feedback, &width, &predelay, &captureInput, &captureOutput}) p->reset(rate, .03);
            blend.setCurrentAndTargetValue(initial(BoardParams::onId(kind, slot)) >= .5f ? 1.f : 0.f);
            gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(initial(BoardParams::trimId(kind, slot))));
            switch (kind) {
                case 0: compressor.prepare(rate, {saved[0], saved[1], saved[2], saved[3], saved[4], saved[5]}); break;
                case 1: drive.prepare(rate, static_cast<int>(spec.maximumBlockSize), {true, saved[1], saved[2], saved[3], saved[4]}); break;
                case 2: captureInput.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(saved[1])); captureOutput.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(saved[2])); break;
                case 3: eq.prepare(rate, {true, saved[1], saved[2], saved[3], saved[4]}); break;
                case 4: modulation.prepare(spec, {true, juce::roundToInt(saved[1]), saved[7] >= .5f ? ModulationPedal::syncedRate(bpm, juce::roundToInt(saved[8])) : saved[2], saved[3], saved[4], saved[5], saved[6]}); break;
                case 5: chorus.prepare(spec, {saved[0], saved[1], saved[2]}); break;
                case 6: {
                    delay.setMaximumDelayInSamples(static_cast<int>(rate * 16)); delay.prepare(spec);
                    constexpr float divisions[] {1, .5f, .75f, .25f, 2, 4};
                    time.setCurrentAndTargetValue((saved[4] >= .5f ? 60000.f * divisions[juce::jlimit(0, 5, juce::roundToInt(saved[5]))] / juce::jlimit(20.f, 400.f, bpm) : saved[0]) * static_cast<float>(rate) / 1000);
                    mix.setCurrentAndTargetValue(saved[1] / 100); width.setCurrentAndTargetValue(saved[2] / 100); feedback.setCurrentAndTargetValue(saved[3] / 100); break;
                }
                case 7: {
                    roomDelay.setMaximumDelayInSamples(static_cast<int>(rate * .2)); roomDelay.prepare(spec); reverb.prepare(spec);
                    mix.setCurrentAndTargetValue(saved[0] / 100); predelay.setCurrentAndTargetValue(saved[4] * static_cast<float>(rate) / 1000); break;
                }
                case 8: {
                    // Keep the immediate response at zero added latency, but
                    // use larger tail partitions for multi-second recordings.
                    // A 256-sample tail repeats the full long-IR sum too often.
                    ambience = std::make_unique<juce::dsp::Convolution>(juce::dsp::Convolution::NonUniform {2048});
                    const auto path = state[slot == 0 ? "ambiencePath" : "ambience1Path"].toString();
                    if (path.isNotEmpty()) {
                        juce::AudioFormatManager formats; formats.registerBasicFormats();
                        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(path)));
                        if (!reader || reader->numChannels < 1 || reader->numChannels > 2 || reader->lengthInSamples <= 0 || reader->lengthInSamples > reader->sampleRate * 30)
                            throw std::runtime_error("Choose a mono/stereo ambience response up to 30 seconds");
                        juce::AudioBuffer<float> response(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                        if (!reader->read(&response, 0, response.getNumSamples(), 0, true, true)) throw std::runtime_error("Could not read ambience response");
                        ambience->loadImpulseResponse(std::move(response), reader->sampleRate, juce::dsp::Convolution::Stereo::yes,
                            juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
                    }
                    ambience->prepare(spec); mix.setCurrentAndTargetValue(saved[0] / 100); break;
                }
                case 12: spring.prepare(spec, {saved[0], saved[1], saved[2], saved[3], saved[4]}); break;
                case 11: plate.prepare(spec, {saved[0], saved[1], saved[2], saved[3], saved[4]}); break;
                case 10: distortion.prepare(rate, static_cast<int>(spec.maximumBlockSize), {saved[0], saved[1], saved[2], saved[3], saved[4]}); break;
                case 9: wah.prepare(spec, {juce::roundToInt(saved[0]), saved[1], saved[2], saved[3], saved[4]}); break;
                default: break;
            }
        }
        void process(juce::AudioBuffer<float>& audio, float bpm, NamWrapper* const* captures) {
            const int n = audio.getNumSamples(); blend.setTargetValue(enabled->load() >= .5f ? 1.f : 0.f); gain.setTargetValue(juce::Decibels::decibelsToGain(trim->load()));
            const bool bypassed = !blend.isSmoothing() && blend.getCurrentValue() == 0;
            if (bypassed && (kind < 5 || kind == 9 || kind == 10)) {
                // Captured pedals have no audible tails. Freeze their recurrent
                // engines after the bypass fade, as the original pedal path does.
                // Delay/reverb nodes still receive silence to drain their tails.
                if (kind == 2) { configure(bpm); captureInput.skip(n); captureOutput.skip(n); }
                gain.skip(n); return;
            }
            for (int ch = 0; ch < channels; ++ch) { if (bypassed) wet.clear(ch, 0, n); else wet.copyFrom(ch, 0, audio, ch, 0, n); }
            juce::AudioBuffer<float> chunk(wet.getArrayOfWritePointers(), channels, n);
            juce::dsp::AudioBlock<float> block(chunk); juce::dsp::ProcessContextReplacing<float> context(block);
            configure(bpm);
            switch (kind) {
                case 0: compressor.process(chunk.getArrayOfWritePointers(), channels, n); break;
                case 1: drive.process(chunk.getWritePointer(0), n); break;
                case 2: {
                    // Each kind/slot owns a separate recurrent model, including
                    // duplicates of the same content. Never collapse stereo.
                    for (int i = 0; i < n; ++i) chunk.getWritePointer(0)[i] *= captureInput.getNextValue();
                    if (captures[slot]) captures[slot]->process(chunk.getWritePointer(0), n); else chunk.clear();
                    for (int i = 0; i < n; ++i) chunk.getWritePointer(0)[i] *= captureOutput.getNextValue(); break;
                }
                case 3: eq.process(chunk); break;
                case 4: modulation.process(chunk); break;
                case 5: chorus.process(context); break;
                case 12: spring.process(chunk); break;
                case 11: plate.process(chunk); break;
                case 10: distortion.process(chunk.getWritePointer(0), n); break;
                case 9: wah.process(chunk); break;
                case 6:
                    for (int i = 0; i < n; ++i) { const float t = time.getNextValue(), m = mix.getNextValue(), w = width.getNextValue(), f = feedback.getNextValue();
                        for (int ch = 0; ch < channels; ++ch) { auto& sample = chunk.getWritePointer(ch)[i]; const auto echo = delay.popSample(ch, t * (ch == 1 ? 1 + .25f * w : 1)); delay.pushSample(ch, sample + echo * f); sample = sample * (1 - m * .5f) + echo * m * .5f; }
                    } break;
                case 7: {
                    for (int i = 0; i < n; ++i) { const auto preDelay = predelay.getNextValue(); for (int ch = 0; ch < channels; ++ch) { roomDelay.pushSample(ch, chunk.getSample(ch, i)); room.setSample(ch, i, roomDelay.popSample(ch, preDelay)); } }
                    juce::AudioBuffer<float> response(room.getArrayOfWritePointers(), channels, n); juce::dsp::AudioBlock<float> roomBlock(response); juce::dsp::ProcessContextReplacing<float> roomContext(roomBlock); reverb.process(roomContext);
                    for (int i = 0; i < n; ++i) { const auto m = mix.getNextValue(); for (int ch = 0; ch < channels; ++ch) chunk.setSample(ch, i, chunk.getSample(ch, i) * (1 - m * .5f) + response.getSample(ch, i)); } break;
                }
                case 8: {
                    mix.setTargetValue(v(0) / 100); ambience->process(context);
                    for (int i = 0; i < n; ++i) { const auto m = mix.getNextValue(); for (int ch = 0; ch < channels; ++ch)
                        chunk.setSample(ch, i, (1 - m) * audio.getSample(ch, i) + m * chunk.getSample(ch, i)); }
                    break;
                }
                default: break;
            }
            for (int i = 0; i < n; ++i) { const float b = blend.getNextValue(), g = gain.getNextValue(); for (int ch = 0; ch < channels; ++ch) {
                const auto sample = chunk.getSample(ch, i); const auto safe = std::isfinite(sample) ? sample : 0.f; auto* out = audio.getWritePointer(ch); out[i] += b * (safe * g - out[i]);
            } }
        }
    };
    std::vector<std::unique_ptr<Node>> nodes;
public:
    SerialPedalboard(const juce::ValueTree& state, juce::AudioProcessorValueTreeState& apvts, const juce::dsp::ProcessSpec& spec) {
        const auto failure = PedalboardState::validate(state); if (failure.isNotEmpty()) throw std::runtime_error(failure.toStdString());
        const auto bpm = static_cast<float>(state.getChildWithProperty("id", "METRO_BPM")["value"]);
        for (const auto& row : state.getChildWithName("PEDALBOARD")) if (static_cast<int>(row["deleted"]) == 0) {
            auto node = std::make_unique<Node>(); node->kind = PedalboardState::kind(row); node->slot = static_cast<int>(row["automationSlot"]); node->pre = row["lane"].toString() == "pre";
            node->prepare(state, apvts, {spec.sampleRate, spec.maximumBlockSize, node->pre ? 1u : spec.numChannels}, bpm); nodes.push_back(std::move(node));
        }
    }
    void process(juce::AudioBuffer<float>& audio, bool pre, float bpm, NamWrapper* const* captures) { for (auto& node : nodes) if (node->pre == pre) node->process(audio, bpm, captures); }
};
