#pragma once
#include <algorithm>
#include <cmath>
#include <juce_core/juce_core.h>

// Intelligent Dual-Stage Noise Gate & "Chug" Attack Transient Shaper.
// Detects dry guitar input, silences idle noise both before and after the amp,
// and features high-frequency pick scrape detection for enhanced staccato attack punch.
class GuitarGate
{
public:
    void prepare(double sampleRate)
    {
        rate = sampleRate; envelope = gain = 0; open = false; hold = 0;
        detectorDecay = std::exp(-1.0f / static_cast<float>(rate * 0.015));
        attack = std::exp(-1.0f / static_cast<float>(rate * 0.0005));
        pickHp = 0.0f;
        prevPickEnergy = 0.0f;
        attackBoost = 1.0f;
        chugAttackAmount = 0.0f;
        configure(-60, 140, true, 0.0f);
    }

    void configure(float thresholdDb, float releaseMs, bool isEnabled, float chugAttackPercent = 0.0f)
    {
        threshold = std::pow(10.0f, thresholdDb / 20.0f);
        release = std::exp(-6.907755f / static_cast<float>(rate * releaseMs * 0.001));
        enabled = isEnabled;
        chugAttackAmount = juce::jlimit(0.0f, 1.0f, chugAttackPercent / 100.0f);
    }

    float tick(float dry)
    {
        envelope = std::max(std::abs(dry), envelope * detectorDecay);
        if (envelope >= threshold) { open = true; hold = static_cast<int>(rate * 0.02); }
        else if (envelope < threshold * 0.501187f)
        {
            if (hold > 0) --hold;
            else open = false;
        }
        const float target = (!enabled || open) ? 1.0f : 0.0f;
        const float coefficient = target > gain ? attack : release;
        gain = target + coefficient * (gain - target);
        if (target == 0 && gain < 0.00001f) gain = 0;

        // Intelligent "Chug" Attack Transient Detection (isolates > 2.2 kHz pick scrape)
        if (chugAttackAmount > 0.001f)
        {
            const float hpCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 2200.0f / static_cast<float>(rate));
            pickHp += hpCoeff * (dry - pickHp);
            const float pickScrape = std::abs(dry - pickHp);
            const float energyDelta = std::max(0.0f, pickScrape - prevPickEnergy);
            prevPickEnergy = pickScrape * 0.95f;

            if (energyDelta > 0.015f && open)
            {
                // Trigger transient attack punch
                attackBoost = 1.0f + chugAttackAmount * 0.65f;
            }
            else
            {
                // Smooth decay back to unity
                attackBoost += 0.01f * (1.0f - attackBoost);
            }
        }
        else
        {
            attackBoost = 1.0f;
        }

        return gain * attackBoost;
    }

private:
    double rate = 48000;
    float envelope = 0, gain = 0, detectorDecay = 0, attack = 0, release = 0, threshold = .001f;
    int hold = 0;
    bool open = false, enabled = true;

    // Chug attack transient state
    float pickHp = 0.0f;
    float prevPickEnergy = 0.0f;
    float attackBoost = 1.0f;
    float chugAttackAmount = 0.0f;
};
