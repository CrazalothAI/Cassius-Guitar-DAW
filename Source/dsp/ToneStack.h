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
    void prepare(double rate) { sampleRate = rate; filters = {}; previous.fill(999); }
    void update(float bass, float mid, float treble, float presence)
    {
        const std::array values {bass, mid, treble, presence};
        if (values == previous) return;
        previous = values;
        using C = juce::dsp::IIR::ArrayCoefficients<float>;
        for (auto& channel : filters)
        {
            channel[0].c = C::makeLowShelf(sampleRate, 120, 0.7071f, juce::Decibels::decibelsToGain(bass));
            channel[1].c = C::makePeakFilter(sampleRate, 750, 0.8f, juce::Decibels::decibelsToGain(mid));
            channel[2].c = C::makeHighShelf(sampleRate, 3500, 0.7071f, juce::Decibels::decibelsToGain(treble));
            channel[3].c = C::makePeakFilter(sampleRate, 4500, 0.7f, juce::Decibels::decibelsToGain(presence));
        }
    }
    void process(juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                for (auto& filter : filters[static_cast<size_t>(ch)])
                    buffer.getWritePointer(ch)[i] = filter.tick(buffer.getReadPointer(ch)[i]);
    }
private:
    double sampleRate = 48000;
    std::array<float, 4> previous {};
    std::array<std::array<Filter, 4>, 2> filters {};
};
