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
        const auto timeConstant = [this](double seconds) { return static_cast<float>(std::exp(-1.0 / (rate * seconds))); };
        pickHpCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 2200.0f / static_cast<float>(rate));
        fastAttack = timeConstant(0.0003); fastRelease = timeConstant(0.005);
        slowCoeff = 1.0f - timeConstant(0.03);
        boostRise = timeConstant(0.0005); boostFall = timeConstant(0.025);
        pickHp = pickFast = pickSlow = 0.0f;
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

        // Pick attack: a transient shaper keyed on pick scrape above 2.2 kHz. A fast
        // envelope racing ahead of a slow one marks a new pick; the boost rises over
        // half a millisecond and falls over 25 ms, so it never steps the gain (a step
        // in front of a high-gain amp is a click), and the scrape must clear the gate
        // threshold, so noise alone cannot fire it.
        float boostTarget = 1.0f;
        if (chugAttackAmount > 0.001f)
        {
            pickHp += pickHpCoeff * (dry - pickHp);
            const float scrape = std::abs(dry - pickHp);
            pickFast = scrape > pickFast ? scrape + fastAttack * (pickFast - scrape) : scrape + fastRelease * (pickFast - scrape);
            pickSlow += slowCoeff * (scrape - pickSlow);
            const float rise = juce::jlimit(0.0f, 1.0f, (pickFast / (pickSlow + 1e-9f) - 1.5f) * 0.5f);
            if (open && pickFast > threshold) boostTarget = 1.0f + chugAttackAmount * 0.65f * rise;
        }
        attackBoost = boostTarget + (boostTarget > attackBoost ? boostRise : boostFall) * (attackBoost - boostTarget);
        return gain;
    }
    // Gain to apply ahead of the amp only, on top of tick(): the pick attack boost.
    float attackGain() const { return attackBoost; }
    bool isOpen() const { return open; }

private:
    double rate = 48000;
    float envelope = 0, gain = 0, detectorDecay = 0, attack = 0, release = 0, threshold = .001f;
    int hold = 0;
    bool open = false, enabled = true;

    // Pick attack transient state
    float pickHpCoeff = 0, fastAttack = 0, fastRelease = 0, slowCoeff = 0, boostRise = 0, boostFall = 0;
    float pickHp = 0.0f, pickFast = 0.0f, pickSlow = 0.0f;
    float attackBoost = 1.0f;
    float chugAttackAmount = 0.0f;
};
