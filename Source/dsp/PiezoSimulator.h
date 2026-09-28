#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>
#include <algorithm>

// Tim Henson Progressive DSP: Acoustic Body & Piezo Resonator Simulator.
// Transforms raw electric guitar magnetic pickup signals into resonant, articulate
// acoustic/piezo tones via modal body filters and high-frequency exciter network.
class PiezoSimulator
{
public:
    void prepare(double sampleRate)
    {
        rate = sampleRate;
        bodyModes = {};
        exciterLow = 0.0f;
        blendSmoother.reset(rate, 0.03); // 30 ms parameter smoothing
        blendSmoother.setCurrentAndTargetValue(0.0f);
        setupModes();
    }

    void configure(bool isEnabled, float blendPercent)
    {
        enabled = isEnabled;
        blendSmoother.setTargetValue(enabled ? juce::jlimit(0.0f, 1.0f, blendPercent / 100.0f) : 0.0f);
    }

    float processSample(float input)
    {
        const float blend = blendSmoother.getNextValue();
        if (blend <= 0.0001f && !blendSmoother.isSmoothing())
            return input;

        // 1. Parallel Modal Body Resonances (Air cavity 105 Hz, soundboard 215 Hz, saddle 3.2 kHz)
        float bodyEnergy = 0.0f;
        for (auto& mode : bodyModes)
            bodyEnergy += mode.process(input);

        // 2. High-Frequency Piezo Exciter Network (> 3.5 kHz non-linear harmonic sparkle)
        const float hpCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 3500.0f / static_cast<float>(rate));
        exciterLow += hpCoeff * (input - exciterLow);
        const float highPassed = input - exciterLow;
        // Asymmetric soft-saturation adds even and odd crystal piezo harmonics
        const float excited = highPassed + 0.25f * (highPassed * highPassed) - 0.1f * (highPassed * highPassed * highPassed);

        // 3. Composite Acoustic Response
        const float acousticOutput = 0.65f * input + 0.35f * bodyEnergy + 0.30f * excited;

        // 4. Equal-Power / Smooth Crossfade with Dry Electric Input
        return input + blend * (acousticOutput - input);
    }

private:
    struct ResonantMode
    {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void configureBandpass(double rate, float freq, float Q, float gain)
        {
            const float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float>(rate);
            const float sinW = std::sin(w0);
            const float cosW = std::cos(w0);
            const float alpha = sinW / (2.0f * Q);

            const float a0 = 1.0f + alpha;
            b0 = (alpha * gain) / a0;
            b1 = 0.0f;
            b2 = (-alpha * gain) / a0;
            a1 = (-2.0f * cosW) / a0;
            a2 = (1.0f - alpha) / a0;
            z1 = z2 = 0.0f;
        }

        float process(float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void setupModes()
    {
        // Mode 1: 105 Hz Helmholtz air cavity mode (warmth, resonance)
        bodyModes[0].configureBandpass(rate, 105.0f, 3.5f, 1.4f);
        // Mode 2: 215 Hz Soundboard wood plate mode (body transient)
        bodyModes[1].configureBandpass(rate, 215.0f, 4.0f, 1.2f);
        // Mode 3: 3200 Hz Treble saddle crispness
        bodyModes[2].configureBandpass(rate, 3200.0f, 2.5f, 1.5f);
    }

    double rate = 48000.0;
    bool enabled = false;
    juce::SmoothedValue<float> blendSmoother;
    std::array<ResonantMode, 3> bodyModes {};
    float exciterLow = 0.0f;
};
