#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>

// Post-cabinet EQ: reduce boom/mud and high-gain fizz without adding another
// nonlinear stage. Fixed storage and smoothed controls keep automation quiet.
class PedalEq
{
public:
    struct Settings { bool on; float body, mud, focus, fizz; };

    void prepare(double sampleRate, Settings settings)
    {
        rate = sampleRate; filters = {}; previous.fill(999); untilUpdate = 0;
        const std::array values {settings.body, settings.mud, settings.focus, settings.fizz};
        for (size_t i = 0; i < gains.size(); ++i) {
            gains[i].reset(rate, .03); gains[i].setCurrentAndTargetValue(values[i]);
        }
        blend.reset(rate, .02); blend.setCurrentAndTargetValue(settings.on ? 1.0f : 0.0f);
    }

    void configure(Settings settings)
    {
        const std::array values {settings.body, settings.mud, settings.focus, settings.fizz};
        for (size_t i = 0; i < gains.size(); ++i) gains[i].setTargetValue(values[i]);
        blend.setTargetValue(settings.on ? 1.0f : 0.0f);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (untilUpdate-- <= 0) { update(); untilUpdate = 15; }
            for (auto& gain : gains) gain.skip(1);
            const float mix = blend.getNextValue();
            for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
            {
                auto& sample = buffer.getWritePointer(ch)[i];
                const float dry = sample;
                float wet = dry;
                for (size_t band = 0; band < coefficients.size(); ++band)
                    wet = filters[static_cast<size_t>(ch)][band].tick(wet, coefficients[band]);
                sample = dry + mix * (wet - dry);
            }
        }
    }

private:
    struct Filter
    {
        float z1 = 0, z2 = 0;
        float tick(float x, const std::array<float, 6>& c)
        {
            const float y = c[0] / c[3] * x + z1;
            z1 = c[1] / c[3] * x - c[4] / c[3] * y + z2;
            z2 = c[2] / c[3] * x - c[5] / c[3] * y;
            return y;
        }
    };
    void update()
    {
        const std::array values {gains[0].getCurrentValue(), gains[1].getCurrentValue(), gains[2].getCurrentValue(), gains[3].getCurrentValue()};
        if (values == previous) return;
        previous = values;
        using C = juce::dsp::IIR::ArrayCoefficients<float>;
        const auto hz = [this](float frequency) { return juce::jmin(frequency, static_cast<float>(rate) * .4f); };
        coefficients[0] = C::makeLowShelf(rate, hz(120), .7071f, juce::Decibels::decibelsToGain(values[0]));
        coefficients[1] = C::makePeakFilter(rate, hz(350), .9f, juce::Decibels::decibelsToGain(values[1]));
        coefficients[2] = C::makePeakFilter(rate, hz(1200), .8f, juce::Decibels::decibelsToGain(values[2]));
        coefficients[3] = C::makeHighShelf(rate, hz(4800), .7071f, juce::Decibels::decibelsToGain(values[3]));
    }
    double rate = 48000;
    int untilUpdate = 0;
    std::array<juce::SmoothedValue<float>, 4> gains;
    juce::SmoothedValue<float> blend;
    std::array<float, 4> previous {};
    std::array<std::array<float, 6>, 4> coefficients {};
    std::array<std::array<Filter, 4>, 2> filters {};
};
