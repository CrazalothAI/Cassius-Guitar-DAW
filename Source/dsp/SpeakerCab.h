#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>

// Built-in closed-back 4x12 voicing for rigs without a cabinet: no IR loaded and a
// capture that is amp-only (or no capture at all). Raw amp output with no speaker
// is mostly fizz above 5 kHz; this rolls it off at 24 dB/octave and adds the cab's
// low resonance and upper-mid presence. Fixed storage; mono.
class SpeakerCab
{
public:
    void prepare(double rate)
    {
        using C = juce::dsp::IIR::ArrayCoefficients<float>;
        const auto g = [](float db) { return juce::Decibels::decibelsToGain(db); };
        stages[0].c = C::makeHighPass(rate, 72.0f, 0.9f);            // cabinet low roll-off
        stages[1].c = C::makePeakFilter(rate, 110.0f, 1.4f, g(4.5f)); // closed-back thump
        stages[2].c = C::makePeakFilter(rate, 420.0f, 1.1f, g(-3.0f)); // box honk
        stages[3].c = C::makePeakFilter(rate, 2400.0f, 1.5f, g(3.0f)); // speaker presence
        stages[4].c = C::makeLowPass(rate, 6000.0f, 0.75f);          // cone breakup roll-off
        stages[5].c = C::makeLowPass(rate, 8000.0f, 0.6f);
        reset();
    }
    void reset() { for (auto& s : stages) s.z1 = s.z2 = 0; }
    void process(float* samples, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            float x = samples[i];
            for (auto& s : stages) x = s.tick(x);
            samples[i] = x;
        }
    }
private:
    struct Biquad
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
    std::array<Biquad, 6> stages {};
};
