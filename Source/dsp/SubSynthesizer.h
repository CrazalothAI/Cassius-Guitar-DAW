#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

// Low-Tuned Production DSP: "Thicken" Sub-Octave Parallel Synthesizer.
// Synthesizes a clean sub-octave sine wave (F0 / 2) dynamically keyed to the guitar's
// envelope, filtered below 85 Hz to provide immense low-end weight for Drop E/Z tunings.
class SubSynthesizer
{
public:
    void prepare(double sampleRate)
    {
        rate = sampleRate;
        phase = 0.0f;
        subFreq = 41.2f; // Low E
        envFollower = 0.0f;
        lpState1 = lpState2 = 0.0f;
        attCoeff = std::exp(-1.0f / static_cast<float>(rate * 0.005)); // 5 ms attack
        relCoeff = std::exp(-1.0f / static_cast<float>(rate * 0.060)); // 60 ms release
        mixSmoother.reset(rate, 0.03);
        mixSmoother.setCurrentAndTargetValue(0.0f);
    }

    void configure(bool isEnabled, float mixPercent)
    {
        enabled = isEnabled;
        mixSmoother.setTargetValue(enabled ? juce::jlimit(0.0f, 1.0f, mixPercent / 100.0f) : 0.0f);
    }

    float processSample(float dryInput, float trackedPitchHz)
    {
        const float mix = mixSmoother.getNextValue();
        if (mix <= 0.0001f && !mixSmoother.isSmoothing())
            return 0.0f;

        // 1. Envelope Follower tracking guitar dynamics
        const float rect = std::abs(dryInput);
        envFollower = rect > envFollower
            ? rect + attCoeff * (envFollower - rect)
            : rect + relCoeff * (envFollower - rect);

        // 2. Sub-octave frequency calculation (F0 / 2)
        if (trackedPitchHz >= 35.0f && trackedPitchHz <= 500.0f)
        {
            const float targetSub = trackedPitchHz * 0.5f;
            subFreq += 0.05f * (targetSub - subFreq);
        }

        // 3. Phase Accumulator for Sub-Octave Sine Wave
        const float phaseInc = juce::MathConstants<float>::twoPi * subFreq / static_cast<float>(rate);
        phase += phaseInc;
        if (phase >= juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;

        const float rawSine = std::sin(phase);

        // 4. Keyed Sub Synthesis (shaped by guitar envelope)
        const float keyedSub = rawSine * envFollower;

        // 5. Steep 85 Hz Low-Pass Filter (Cascaded 1-pole for stability and zero latency)
        const float lpCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 85.0f / static_cast<float>(rate));
        lpState1 += lpCoeff * (keyedSub - lpState1);
        lpState2 += lpCoeff * (lpState1 - lpState2);

        // Return the sub component scaled by mix
        return lpState2 * mix * 1.5f;
    }

private:
    double rate = 48000.0;
    bool enabled = false;
    float phase = 0.0f;
    float subFreq = 41.2f;
    float envFollower = 0.0f;
    float attCoeff = 0.0f;
    float relCoeff = 0.0f;
    float lpState1 = 0.0f;
    float lpState2 = 0.0f;
    juce::SmoothedValue<float> mixSmoother;
};
