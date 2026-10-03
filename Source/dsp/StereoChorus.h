#pragma once
#include <juce_dsp/juce_dsp.h>

// Independent quadrature modulation adds width to a mono guitar while preserving
// incoming stereo. Fixed storage, no feedback, and smoothed mix keep cleans quiet.
class StereoChorus
{
public:
    struct Settings { float mix, rate, depth; };
    void prepare(const juce::dsp::ProcessSpec& spec, Settings initial)
    {
        sampleRate = spec.sampleRate; phase = 0;
        delay.setMaximumDelayInSamples(static_cast<int>(sampleRate * .03)); delay.prepare(spec); delay.reset();
        for (auto* value : {&mix, &rate, &depth}) value->reset(sampleRate, .03);
        mix.setCurrentAndTargetValue(initial.mix / 100); rate.setCurrentAndTargetValue(initial.rate); depth.setCurrentAndTargetValue(initial.depth / 100);
    }
    void configure(Settings settings) { mix.setTargetValue(settings.mix / 100); rate.setTargetValue(settings.rate); depth.setTargetValue(settings.depth / 100); }
    void process(juce::dsp::ProcessContextReplacing<float>& context)
    {
        auto block = context.getOutputBlock();
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            const float blend = mix.getNextValue() * .5f, modulation = depth.getNextValue() * 5;
            const float speed = rate.getNextValue();
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
                auto& sample = block.getChannelPointer(ch)[i];
                const float time = static_cast<float>((12 + (blend > 0 ? modulation * std::sin(phase + (ch == 1 ? juce::MathConstants<double>::halfPi : 0)) : 0)) * sampleRate / 1000);
                delay.pushSample(static_cast<int>(ch), sample);
                const float wet = delay.popSample(static_cast<int>(ch), time);
                sample += blend * (wet - sample);
            }
            phase += juce::MathConstants<double>::twoPi * speed / sampleRate;
            if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        }
    }
private:
    double sampleRate = 48000, phase = 0;
    juce::SmoothedValue<float> mix, rate, depth;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay;
};
