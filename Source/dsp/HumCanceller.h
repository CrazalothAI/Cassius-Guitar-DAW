#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <algorithm>

// Removes mains hum (and its buzz harmonics) from the DI before anything amplifies
// it. A pickup near a transformer, a dimmer or a computer, a bridge single coil above
// all, adds a steady 50 or 60 Hz tone with harmonics; a high-gain amp turns it into
// buzz under every note, and the Bass control after the amp boosts it further.
//
// Each harmonic up to 2.4 kHz is cancelled by an adaptive sinusoid (an LMS canceller
// locked to the mains), which behaves as a narrow notch (about a hertz wide at the
// fundamental, widening with harmonic number to absorb the grid's drift): the hum
// goes and a note a few hertz away is untouched. It only engages once hum has been heard while
// the strings are quiet, so a rig without hum passes through bit for bit, and it
// watches both mains families, so it needs no setting anywhere in the world.
class HumCanceller
{
public:
    void prepare(double sampleRate)
    {
        rate = sampleRate;
        // Weights settle in about a quarter of a second: quick enough to follow the
        // hum as the player moves, slow enough to leave the notes alone.
        const float mu = static_cast<float>(2.0 / (0.25 * rate));
        families[0].prepare(rate, 60.0, mu);
        families[1].prepare(rate, 50.0, mu);
        active = 0; challenger = 0; present = false; amount = 0;
        fade = static_cast<float>(1.0 / (0.1 * rate));
        windowPower = slowPower = 0; windowSamples = steadyFor = 0;
    }
    void process(float* samples, int count)
    {
        auto& main = families[static_cast<size_t>(active)];
        auto& other = families[static_cast<size_t>(1 - active)];
        const float target = present ? 1.0f : 0.0f;
        for (int i = 0; i < count; ++i)
        {
            other.cancel(samples[i], 3); // only watched, the signal is untouched
            const float residual = main.cancel(samples[i], main.count);
            windowPower += residual * residual;
            amount += fade * (target - amount);
            samples[i] += amount * (residual - samples[i]);
        }
        main.renormalise(); other.renormalise();
        // The strings are quiet when what is left (noise, and any hum not yet
        // cancelled) is steady and well below playing level, judged over windows of
        // a full mains cycle. Only then is the hum level trusted: a note near a
        // harmonic would read as hum.
        windowSamples += count;
        if (windowSamples >= static_cast<int>(rate * 0.02))
        {
            const float p = windowPower / static_cast<float>(windowSamples);
            const bool steady = p < 2.0f * slowPower && p > 0.5f * slowPower;
            slowPower += 0.1f * (p - slowPower); // about 0.2 s
            steadyFor = steady ? steadyFor + 1 : 0;
            // Judged at the end of a window that was itself quiet, so a note's first
            // milliseconds never count as hum.
            if (p < quietBelow && steadyFor >= 5) present = main.level() > presentLevel;
            windowPower = 0; windowSamples = 0;
        }
        // Switch family when the other mains frequency has been clearly stronger for
        // a second (a guitar on 50 Hz power, for instance).
        challenger = other.level() > 4.0f * main.level() && other.level() > presentLevel ? challenger + count : 0;
        if (challenger > rate)
        {
            active = 1 - active; challenger = 0; present = false;
            families[static_cast<size_t>(active)].clear();
        }
    }
    // Hum power (first three harmonics), whether it is being removed, and the mains frequency.
    float humLevel() const { return families[static_cast<size_t>(active)].level(); }
    bool cancelling() const { return present; }
    float mainsHz() const { return active == 0 ? 60.0f : 50.0f; }
private:
    struct Family
    {
        static constexpr size_t maxHarmonics = 48;
        std::array<float, maxHarmonics> re {}, im {}, stepRe {}, stepIm {}, a {}, b {}, mu {};
        size_t count = 0;
        void prepare(double rate, double hz, float rateMu)
        {
            count = juce::jmin(maxHarmonics, static_cast<size_t>(2400.0 / hz));
            for (size_t k = 0; k < count; ++k)
            {
                const double w = juce::MathConstants<double>::twoPi * hz * static_cast<double>(k + 1) / rate;
                stepRe[k] = static_cast<float>(std::cos(w)); stepIm[k] = static_cast<float>(std::sin(w));
                // Higher harmonics get proportionally wider notches, so a grid a few
                // hundredths of a hertz off still lands inside every one of them.
                mu[k] = rateMu * std::max(1.0f, static_cast<float>(k + 1) / 4.0f);
            }
            re.fill(1); im.fill(0);
            clear();
        }
        void clear() { a = {}; b = {}; }
        // Subtracts the first `n` harmonics' estimate and adapts it.
        float cancel(float x, size_t n)
        {
            float hum = 0;
            for (size_t k = 0; k < n; ++k) hum += a[k] * re[k] + b[k] * im[k];
            const float e = x - hum;
            for (size_t k = 0; k < n; ++k)
            {
                a[k] += mu[k] * e * re[k]; b[k] += mu[k] * e * im[k];
                const float r = re[k] * stepRe[k] - im[k] * stepIm[k];
                im[k] = re[k] * stepIm[k] + im[k] * stepRe[k];
                re[k] = r;
            }
            return x - hum;
        }
        // Keeps the oscillators on the unit circle despite rounding.
        void renormalise()
        {
            for (size_t k = 0; k < count; ++k)
            {
                const float g = 1.5f - 0.5f * (re[k] * re[k] + im[k] * im[k]);
                re[k] *= g; im[k] *= g;
            }
        }
        float level() const
        {
            float p = 0;
            for (size_t k = 0; k < juce::jmin<size_t>(3, count); ++k) p += a[k] * a[k] + b[k] * b[k];
            return p;
        }
    };
    // Hum below about -90 dBFS on the first harmonics is left alone; playing sits
    // far above -40 dBFS RMS at the DI.
    static constexpr float presentLevel = 1e-9f, quietBelow = 1e-4f;
    double rate = 48000;
    std::array<Family, 2> families {};
    int active = 0, challenger = 0;
    bool present = false;
    float amount = 0, fade = 0, windowPower = 0, slowPower = 0;
    int windowSamples = 0, steadyFor = 0;
};
