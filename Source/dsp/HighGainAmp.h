#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

// Built-in high-gain amp for the metal channel when no capture is loaded.
// Three cascaded preamp stages with asymmetric tube-like clipping, interstage
// coupling/Miller filtering that keep palm mutes tight and the top end smooth,
// and a soft power stage. The clipping runs at 8x oversampling so its harmonics
// do not fold back into the audible band as fizz. Drive is applied upstream.
class HighGainAmp
{
public:
    void prepare(double sampleRate, int maxBlock)
    {
        rate = sampleRate;
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(1, 3,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampling->initProcessing(static_cast<size_t>(maxBlock));
        const double os = rate * 8;
        using C = juce::dsp::IIR::ArrayCoefficients<float>;
        midPush.c = C::makePeakFilter(rate, 800.0f, 0.7f, juce::Decibels::decibelsToGain(3.5f));
        // Power-amp resonance: weight restored after the clipping, so chugs stay tight.
        depth.c = C::makeLowShelf(rate, 110.0f, 0.7f, juce::Decibels::decibelsToGain(5.0f));
        const auto onePole = [os](double hz) { return static_cast<float>(1.0 - std::exp(-juce::MathConstants<double>::twoPi * hz / os)); };
        hp1 = onePole(90); lp1 = onePole(10000); hp2 = onePole(30); lp2 = onePole(7000);
        dcBlock = static_cast<float>(1.0 - std::exp(-juce::MathConstants<double>::twoPi * 12.0 / rate));
        reset();
    }
    void reset()
    {
        if (oversampling) oversampling->reset();
        midPush.z1 = midPush.z2 = depth.z1 = depth.z2 = 0;
        s1hp = s1lp = s2hp = s2lp = dc = 0;
    }
    void process(float* samples, int count)
    {
        for (int i = 0; i < count; ++i) samples[i] = midPush.tick(samples[i]);
        float* channels[] {samples};
        juce::dsp::AudioBlock<float> block(channels, 1, static_cast<size_t>(count));
        auto up = oversampling->processSamplesUp(block);
        float* x = up.getChannelPointer(0);
        const int n = static_cast<int>(up.getNumSamples());
        for (int i = 0; i < n; ++i)
        {
            // V1: asymmetric grid clipping; the bias adds even harmonics.
            float v = std::tanh(x[i] * stage1Gain + bias1) - tanhBias1;
            s1hp += hp1 * (v - s1hp); v -= s1hp;   // coupling cap: tight lows
            s1lp += lp1 * (v - s1lp); v = s1lp;    // Miller roll-off
            // V2: the main distortion stage, biased the other way.
            v = std::tanh(v * stage2Gain - bias2) + tanhBias2;
            s2hp += hp2 * (v - s2hp); v -= s2hp;
            s2lp += lp2 * (v - s2lp); v = s2lp;
            // V3 and power stage: compression and a rounded ceiling.
            v = std::tanh(v * stage3Gain);
            x[i] = std::tanh(0.9f * v) * outputLevel;
        }
        oversampling->processSamplesDown(block);
        for (int i = 0; i < count; ++i) { dc += dcBlock * (samples[i] - dc); samples[i] = depth.tick(samples[i] - dc); }
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
    static constexpr float stage1Gain = 6.0f, stage2Gain = 9.0f, stage3Gain = 2.5f;
    static constexpr float bias1 = 0.15f, bias2 = 0.15f;
    // Output trim: a -27 dBFS RMS DI take lands near the -18 dB level of normalized captures.
    static constexpr float outputLevel = 0.085f;
    const float tanhBias1 = std::tanh(bias1), tanhBias2 = std::tanh(bias2);
    double rate = 48000;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    Biquad midPush, depth;
    float hp1 = 0, lp1 = 0, hp2 = 0, lp2 = 0, dcBlock = 0;
    float s1hp = 0, s1lp = 0, s2hp = 0, s2lp = 0, dc = 0;
};
