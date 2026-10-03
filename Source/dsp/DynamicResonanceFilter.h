#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

// Cassian DSP: Dynamic 200-400 Hz Resonance Suppression Notch.
// Carves heavy low-mid flub dynamically during palm-muted chugs while
// relaxing completely (0 dB cut) on single-note leads and sustained chords.
class DynamicResonanceFilter
{
public:
    void prepare(double sampleRate)
    {
        rate = sampleRate;
        detectorLow = detectorHigh = 0.0f;
        envelope = 0.0f;
        z1 = z2 = 0.0f;
        attackCoeff = std::exp(-1.0f / static_cast<float>(rate * 0.0015)); // ~1.5 ms attack
        releaseCoeff = std::exp(-1.0f / static_cast<float>(rate * 0.040)); // ~40 ms release
        currentCutDb = 0.0f;
        detectorLP = 1 - std::exp(-juce::MathConstants<float>::twoPi * 400 / static_cast<float>(rate));
        detectorHP = 1 - std::exp(-juce::MathConstants<float>::twoPi * 180 / static_cast<float>(rate));
        untilUpdate = 0; coefficientFreq = -1; publishedCut.store(0);
        updateCoefficients(280.0f, 0.0f);
    }

    void configure(bool isEnabled, float amountPercent, float centerFreqHz = 280.0f)
    {
        enabled = isEnabled;
        maxCutDb = -18.0f * juce::jlimit(0.0f, 1.0f, amountPercent / 100.0f);
        targetFreq = juce::jlimit(150.0f, 500.0f, centerFreqHz);
    }

    float processSample(float input)
    {
        if (!enabled || maxCutDb >= -0.1f)
        {
            publishedCut.store(0, std::memory_order_relaxed);
            return input;
        }

        // 1. Sidechain Bandpass Filter (isolates 200-400 Hz resonance energy)
        // Two cascaded 1-pole filters (LP then HP) for zero-latency detection
        detectorLow += detectorLP * (input - detectorLow);
        detectorHigh += detectorHP * (detectorLow - detectorHigh);
        const float bandpassed = detectorLow - detectorHigh;

        // 2. Envelope Follower
        const float rect = std::abs(bandpassed);
        envelope = rect > envelope
            ? rect + attackCoeff * (envelope - rect)
            : rect + releaseCoeff * (envelope - rect);

        // 3. Dynamic Cut Calculation
        // Threshold around -32 dB (approx 0.025 amplitude)
        constexpr float threshold = 0.025f;
        float desiredCut = 0.0f;
        if (envelope > threshold)
        {
            const float ratio = juce::jlimit(0.0f, 1.0f, (envelope - threshold) / 0.15f);
            desiredCut = maxCutDb * ratio; // e.g. up to -18 dB
        }

        // Smooth cut transitions
        currentCutDb += 0.05f * (desiredCut - currentCutDb);
        if (untilUpdate-- <= 0) { updateCoefficients(targetFreq, currentCutDb); untilUpdate = 15; publishedCut.store(currentCutDb, std::memory_order_relaxed); }

        // 4. Parametric Notch Filter (Direct Form II Transposed)
        const float y = b0 * input + z1;
        z1 = b1 * input - a1 * y + z2;
        z2 = b2 * input - a2 * y;
        return y;
    }

    float getCurrentCutDb() const noexcept { return publishedCut.load(std::memory_order_relaxed); }

private:
    void updateCoefficients(float freq, float gainDb)
    {
        // Peaking/notch biquad
        if (coefficientFreq != freq) {
            coefficientFreq = freq;
            const float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float>(rate);
            cosW = std::cos(w0); alpha = std::sin(w0) / 3.6f;
        }
        const float A = std::pow(10.0f, gainDb / 40.0f); // sqrt(gain)

        const float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * cosW) / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = (-2.0f * cosW) / a0;
        a2 = (1.0f - alpha / A) / a0;
    }

    double rate = 48000.0;
    bool enabled = false;
    float maxCutDb = 0.0f;
    float targetFreq = 280.0f;
    float currentCutDb = 0.0f;
    std::atomic<float> publishedCut {0};
    float detectorLP = 0, detectorHP = 0, coefficientFreq = -1, cosW = 1, alpha = 0;
    int untilUpdate = 0;

    float detectorLow = 0.0f;
    float detectorHigh = 0.0f;
    float envelope = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    // Biquad coefficients & state
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};
