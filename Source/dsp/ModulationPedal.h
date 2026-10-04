#pragma once
#include <juce_dsp/juce_dsp.h>

// Post-cab modulation. Storage is allocated in prepare; processing and switching
// use fixed state and smoothed gains, without introducing latency to the dry path.
class ModulationPedal
{
public:
    struct Settings { bool enabled = false; int mode = 0; float rate = .8f, depth = 50, mix = 50, feedback = 20, stereo = 0; };
    static float syncedRate(float bpm, int division)
    {
        constexpr float beats[] {4, 2, 1, .5f, .75f};
        return juce::jlimit(.05f, 10.f, juce::jlimit(20.f, 400.f, bpm) / (60 * beats[juce::jlimit(0, 4, division)]));
    }
    void prepare(const juce::dsp::ProcessSpec& spec, Settings initial)
    {
        sampleRate = spec.sampleRate; phase = 0; active = false;
        delay.setMaximumDelayInSamples(static_cast<int>(sampleRate * .012) + 4); delay.prepare(spec);
        clear();
        for (auto* v : {&blend, &speed, &depth, &feedback, &width, &modes[0], &modes[1], &modes[2]}) v->reset(sampleRate, .03);
        configure(initial);
        for (auto* v : {&blend, &speed, &depth, &feedback, &width, &modes[0], &modes[1], &modes[2]}) v->setCurrentAndTargetValue(v->getTargetValue());
    }
    void configure(Settings s)
    {
        blend.setTargetValue(s.enabled ? juce::jlimit(0.f, 1.f, s.mix / 100) : 0);
        speed.setTargetValue(juce::jlimit(.05f, 10.f, s.rate)); depth.setTargetValue(juce::jlimit(0.f, 1.f, s.depth / 100));
        feedback.setTargetValue(juce::jlimit(0.f, .7f, s.feedback / 100)); width.setTargetValue(juce::jlimit(0.f, 1.f, s.stereo / 100));
        for (int i = 0; i < 3; ++i) modes[static_cast<size_t>(i)].setTargetValue(i == juce::jlimit(0, 2, s.mode) ? 1.f : 0.f);
    }
    void process(juce::AudioBuffer<float>& audio)
    {
        const int count = audio.getNumSamples();
        if (!blend.isSmoothing() && blend.getCurrentValue() == 0) {
            if (active) { clear(); active = false; }
            const float hz = speed.skip(count); depth.skip(count); feedback.skip(count); width.skip(count);
            for (auto& v : modes) v.skip(count);
            advance(juce::MathConstants<double>::twoPi * hz * count / sampleRate);
            return; // Settled bypass is sample-exact and skips the filters.
        }
        active = true;
        for (int i = 0; i < count; ++i) {
            const float amount = blend.getNextValue(), extent = depth.getNextValue(), fb = feedback.getNextValue(), spread = width.getNextValue();
            const float hz = speed.getNextValue();
            const std::array<float, 3> weights {modes[0].getNextValue(), modes[1].getNextValue(), modes[2].getNextValue()};
            for (int ch = 0; ch < juce::jmin(2, audio.getNumChannels()); ++ch) {
                const auto channel = static_cast<size_t>(ch);
                const float lfo = static_cast<float>(std::sin(phase + (ch == 1 ? spread * juce::MathConstants<double>::halfPi : 0)));
                const float dry = audio.getSample(ch, i);
                // Six all-pass stages; exponential frequency sweep, bounded feedback.
                const float cutoff = juce::jmin(static_cast<float>(sampleRate * .2), 570.f * std::pow(10.f, extent * lfo * .5f));
                const float tangent = std::tan(juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate));
                const float coefficient = (tangent - 1) / (tangent + 1);
                float shifted = dry + fb * std::tanh(phaseFeedback[channel]);
                for (auto& memory : allpass[channel]) { const float y = coefficient * shifted + memory; memory = shifted - coefficient * y; shifted = y; }
                phaseFeedback[channel] = shifted;
                const float phaser = .5f * (dry + shifted * (1 - fb));
                // 0.5–6.5 ms at full depth. Read before writing; minimum delay > 1 sample.
                const float delayed = delay.popSample(ch, static_cast<float>(sampleRate * .001) * (3.5f + 3 * extent * lfo));
                delay.pushSample(ch, dry + fb * std::tanh(delayed));
                const float flanger = .5f * (dry + delayed * (1 - fb));
                const float tremolo = dry * (1 - extent * .5f * (1 + lfo));
                const float wet = weights[0] * phaser + weights[1] * flanger + weights[2] * tremolo;
                audio.setSample(ch, i, dry + amount * (wet - dry));
            }
            advance(juce::MathConstants<double>::twoPi * hz / sampleRate);
        }
    }
private:
    void clear() { delay.reset(); allpass = {}; phaseFeedback = {}; }
    void advance(double step) { phase += step; if (phase >= juce::MathConstants<double>::twoPi) phase = std::fmod(phase, juce::MathConstants<double>::twoPi); }
    double sampleRate = 48000, phase = 0;
    bool active = false;
    juce::SmoothedValue<float> blend, speed, depth, feedback, width;
    std::array<juce::SmoothedValue<float>, 3> modes;
    std::array<std::array<float, 6>, 2> allpass {};
    std::array<float, 2> phaseFeedback {};
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay;
};
