#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

// Keeps the DI noise floor out of the distortion, where it turns into fuzz under
// every note. A gate cannot help while a note sounds, so this works on the band
// instead: content above ~7 kHz in a guitar DI is hiss, and as a note decays toward
// the gate threshold its own treble fades first, so a low-pass ahead of the amp
// closes with it (smoothly, 7.5 kHz down to 1.8 kHz) while loud picking stays open.
class NoiseShield
{
public:
    void prepare(double sampleRate)
    {
        rate = sampleRate;
        using C = juce::dsp::IIR::ArrayCoefficients<float>;
        bandLimit[0].c = C::makeLowPass(rate, 7000.0f, 0.5412f);
        bandLimit[1].c = C::makeLowPass(rate, 7000.0f, 1.3066f);
        attack = std::exp(-1.0f / static_cast<float>(rate * 0.002));
        release = std::exp(-1.0f / static_cast<float>(rate * 0.060));
        reset();
    }
    void reset()
    {
        for (auto& f : bandLimit) f.z1 = f.z2 = 0;
        envelope = 0; lp1 = lp2 = 0; cutoffSmooth = maxCutoff;
    }
    // `thresholdDb` is the gate's opening level: the closer the note gets to it,
    // the further the filter closes. It follows the gate switch: with the gate off
    // only the fixed band-limit remains, so the response is the raw one.
    void process(float* samples, const float* detector, int count, float thresholdDb, bool dynamic)
    {
        const float threshold = juce::Decibels::decibelsToGain(thresholdDb);
        const float twoPiOverRate = juce::MathConstants<float>::twoPi / static_cast<float>(rate);
        for (int i = 0; i < count; ++i)
        {
            float x = samples[i];
            for (auto& f : bandLimit) x = f.tick(x);
            const float level = std::abs(detector[i]);
            envelope = level > envelope ? level + attack * (envelope - level) : level + release * (envelope - level);
            // 0 at the threshold, 1 from 40 dB above it.
            const float headroomDb = 20.0f * std::log10((envelope + 1e-9f) / threshold);
            const float open = dynamic ? juce::jlimit(0.0f, 1.0f, headroomDb / 40.0f) : 1.0f;
            const float target = minCutoff * std::pow(maxCutoff / minCutoff, open);
            cutoffSmooth += 0.002f * (target - cutoffSmooth);
            const float g = 1.0f - std::exp(-twoPiOverRate * cutoffSmooth);
            lp1 += g * (x - lp1);
            lp2 += g * (lp1 - lp2);
            samples[i] = lp2;
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
    static constexpr float minCutoff = 1800.0f, maxCutoff = 7500.0f;
    double rate = 48000;
    std::array<Biquad, 2> bandLimit {};
    float attack = 0, release = 0, envelope = 0, lp1 = 0, lp2 = 0, cutoffSmooth = maxCutoff;
};
