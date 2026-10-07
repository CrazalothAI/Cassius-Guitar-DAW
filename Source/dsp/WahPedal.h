#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

// Prepared per pedal instance. Stereo uses one linked detector and separate
// filter histories; processing neither allocates nor changes routing.
class WahPedal {
public:
    struct Settings { int mode = 0; float position = 50, sensitivity = 0, resonance = 45, mix = 100; };
    void prepare(const juce::dsp::ProcessSpec& spec, Settings initial) {
        rate = spec.sampleRate; channels = static_cast<int>(spec.numChannels);
        filter.prepare(spec); filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        attack = static_cast<float>(std::exp(-1. / (rate * .008)));
        release = static_cast<float>(std::exp(-1. / (rate * .12)));
        slew = static_cast<float>(1 - std::exp(-1. / (rate * .02)));
        for (auto* value : {&sensitivity, &resonance, &mix}) value->reset(rate, .02);
        configure(initial); sensitivity.setCurrentAndTargetValue(sensitivity.getTargetValue());
        resonance.setCurrentAndTargetValue(resonance.getTargetValue()); mix.setCurrentAndTargetValue(mix.getTargetValue());
        position = manual; envelope = 0; update = 0;
    }
    void configure(Settings settings) {
        automatic = settings.mode == 1; manual = juce::jlimit(0.f, 1.f, settings.position / 100);
        sensitivity.setTargetValue(4.f * juce::Decibels::decibelsToGain(juce::jlimit(-24.f, 24.f, settings.sensitivity)));
        resonance.setTargetValue(.7f + 2.8f * juce::jlimit(0.f, 1.f, settings.resonance / 100));
        mix.setTargetValue(juce::jlimit(0.f, 1.f, settings.mix / 100));
    }
    void process(juce::AudioBuffer<float>& audio) {
        for (int i = 0; i < audio.getNumSamples(); ++i) {
            float peak = 0; for (int ch = 0; ch < channels; ++ch) peak = juce::jmax(peak, std::abs(audio.getSample(ch, i)));
            const auto coefficient = peak > envelope ? attack : release;
            envelope = coefficient * envelope + (1 - coefficient) * peak;
            const auto sense = sensitivity.getNextValue(), q = resonance.getNextValue(), blend = mix.getNextValue();
            const auto target = automatic ? juce::jlimit(0.f, 1.f, envelope * sense) : manual;
            position += slew * (target - position);
            if (update++ % 8 == 0) {
                filter.setCutoffFrequency(juce::jmin(static_cast<float>(rate) * .45f, 350.f * std::pow(2600.f / 350.f, position)));
                filter.setResonance(q);
            }
            for (int ch = 0; ch < channels; ++ch) {
                const auto dry = audio.getSample(ch, i), wet = filter.processSample(ch, dry) * (1.4f / q);
                audio.setSample(ch, i, dry + blend * (wet - dry));
            }
        }
        filter.snapToZero();
    }
private:
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> sensitivity, resonance, mix;
    double rate = 48000; int channels = 1; unsigned update = 0;
    bool automatic = false; float manual = .5f, position = .5f, envelope = 0, attack = 0, release = 0, slew = 0;
};
