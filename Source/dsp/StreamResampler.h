#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

// Streaming sample-rate converter for block-based audio. Push any number of
// input samples, then pull output samples at the other rate. The fractional read
// position carries across blocks, so there is no per-block rounding, drift or
// reading past the end of a block. 4-point Hermite interpolation; when the rate
// drops, a 4th-order low-pass at 45% of the output rate removes content that
// would otherwise alias into the audible band.
class StreamResampler
{
public:
    void prepare(double inputRate, double outputRate, int capacity)
    {
        step = inputRate / outputRate;
        size_t size = 64;
        while (size < static_cast<size_t>(capacity) * 2 + 16) size <<= 1;
        ring.assign(size, 0.0f); mask = size - 1;
        // One sample of history ahead of the first read (the Hermite x[-1] tap).
        written = 1; position = 1.0;
        filtering = step > 1.0;
        using C = juce::dsp::IIR::ArrayCoefficients<float>;
        const auto cutoff = static_cast<float>(outputRate * 0.45);
        filters[0].c = C::makeLowPass(inputRate, cutoff, 0.5412f);
        filters[1].c = C::makeLowPass(inputRate, cutoff, 1.3066f);
        for (auto& f : filters) f.z1 = f.z2 = 0;
    }
    void push(const float* input, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            float x = input[i];
            if (filtering) for (auto& f : filters) x = f.tick(x);
            ring[static_cast<size_t>(written) & mask] = x;
            ++written;
        }
    }
    void pushSilence(int count) { for (int i = 0; i < count; ++i) ring[static_cast<size_t>(written++) & mask] = 0.0f; }
    // Output samples that can be produced from what has been pushed.
    int available() const
    {
        const double last = static_cast<double>(written) - 3.0; // needs x[+1] and x[+2]
        return last < position ? 0 : static_cast<int>(std::floor((last - position) / step)) + 1;
    }
    void pull(float* output, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto index = static_cast<long long>(position);
            const float t = static_cast<float>(position - static_cast<double>(index));
            const float x0 = at(index - 1), x1 = at(index), x2 = at(index + 1), x3 = at(index + 2);
            const float c1 = 0.5f * (x2 - x0);
            const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
            const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
            output[i] = ((c3 * t + c2) * t + c1) * t + x1;
            position += step;
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
    float at(long long index) const { return ring[static_cast<size_t>(index) & mask]; }
    std::vector<float> ring;
    size_t mask = 63;
    long long written = 1;
    double position = 1.0, step = 1.0;
    bool filtering = false;
    std::array<Biquad, 2> filters {};
};
