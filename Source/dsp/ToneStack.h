#pragma once
#include <juce_dsp/juce_dsp.h>
// Fixed storage biquads: coefficient updates do not allocate in the callback.
class ToneStack
{
    struct Filter
    {
        std::array<float, 6> c {1, 0, 0, 1, 0, 0};
        float z1 = 0, z2 = 0;
        float tick(float x)
        {
            const float y = c[0] / c[3] * x + z1;
            z1 = c[1] / c[3] * x - c[4] / c[3] * y + z2;
            z2 = c[2] / c[3] * x - c[5] / c[3] * y;
            return y;
        }
    };
public:
    void prepare(double rate, std::array<float, 4> values = {}) {
        sampleRate = rate; filters = {}; previous.fill(999); untilUpdate = 0;
        for (size_t i = 0; i < gains.size(); ++i) { gains[i].reset(rate, .03); gains[i].setCurrentAndTargetValue(values[i]); }
        rebuild();
    }
    void update(float bass, float mid, float treble, float presence)
    {
        const std::array values {bass, mid, treble, presence};
        for (size_t i = 0; i < gains.size(); ++i) gains[i].setTargetValue(values[i]);
    }
    void process(juce::AudioBuffer<float>& buffer)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            if (untilUpdate-- <= 0) { rebuild(); untilUpdate = 15; }
            for (auto& gain : gains) gain.skip(1);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (auto& filter : filters[static_cast<size_t>(ch)])
                    buffer.getWritePointer(ch)[i] = filter.tick(buffer.getReadPointer(ch)[i]);
        }
    }
private:
    void rebuild()
    {
        const std::array values {gains[0].getCurrentValue(), gains[1].getCurrentValue(), gains[2].getCurrentValue(), gains[3].getCurrentValue()};
        if (values == previous) return;
        previous = values;
        using C = juce::dsp::IIR::ArrayCoefficients<float>;
        for (auto& channel : filters)
        {
            channel[0].c = C::makeLowShelf(sampleRate, 120, 0.7071f, juce::Decibels::decibelsToGain(values[0]));
            channel[1].c = C::makePeakFilter(sampleRate, 750, 0.8f, juce::Decibels::decibelsToGain(values[1]));
            channel[2].c = C::makeHighShelf(sampleRate, 3500, 0.7071f, juce::Decibels::decibelsToGain(values[2]));
            channel[3].c = C::makePeakFilter(sampleRate, 4500, 0.7f, juce::Decibels::decibelsToGain(values[3]));
        }
    }
    std::array<juce::SmoothedValue<float>, 4> gains;
    int untilUpdate = 0;
    double sampleRate = 48000;
    std::array<float, 4> previous {};
    std::array<std::array<Filter, 4>, 2> filters {};
};
