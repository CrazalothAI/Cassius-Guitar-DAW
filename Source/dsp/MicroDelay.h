#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Sub-sample micro-timing delay for stereo double-tracking simulation.
// Applies 0.0 to 1.0 ms fractional delay to the right channel with Hermite
// cubic interpolation to shape comb-filtering patterns and widen the stereo field.
class MicroDelay
{
public:
    void prepare(double sampleRate)
    {
        rate = sampleRate;
        bufferSize = 512;
        delayBuffer.assign(static_cast<size_t>(bufferSize), 0.0f);
        writeIndex = 0;
        targetDelaySamples = 0.0f;
        currentDelaySamples = 0.0f;
    }

    void configure(float delayMs)
    {
        targetDelaySamples = juce::jlimit(0.0f, 1.0f, delayMs) * static_cast<float>(rate) / 1000.0f;
    }

    void processStereo(float* /*left*/, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            // Left channel stays immediate
            // Right channel is fed into the fractional micro-delay line
            currentDelaySamples += 0.005f * (targetDelaySamples - currentDelaySamples);

            delayBuffer[static_cast<size_t>(writeIndex)] = right[i];

            if (currentDelaySamples < 0.1f)
            {
                // Passthrough
                writeIndex = (writeIndex + 1) % bufferSize;
                continue;
            }

            // Read with Hermite cubic interpolation
            const float readPos = static_cast<float>(writeIndex) - currentDelaySamples;
            const float unwrappedPos = readPos < 0.0f ? readPos + static_cast<float>(bufferSize) : readPos;

            const int i1 = static_cast<int>(unwrappedPos);
            const float frac = unwrappedPos - static_cast<float>(i1);

            const int i0 = (i1 - 1 + bufferSize) % bufferSize;
            const int i2 = (i1 + 1) % bufferSize;
            const int i3 = (i1 + 2) % bufferSize;

            const float y0 = delayBuffer[static_cast<size_t>(i0)];
            const float y1 = delayBuffer[static_cast<size_t>(i1)];
            const float y2 = delayBuffer[static_cast<size_t>(i2)];
            const float y3 = delayBuffer[static_cast<size_t>(i3)];

            // 4-point Hermite cubic interpolation
            const float c0 = y1;
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

            right[i] = ((c3 * frac + c2) * frac + c1) * frac + c0;
            writeIndex = (writeIndex + 1) % bufferSize;
        }
    }

private:
    double rate = 48000.0;
    int bufferSize = 512;
    std::vector<float> delayBuffer;
    int writeIndex = 0;
    float targetDelaySamples = 0.0f;
    float currentDelaySamples = 0.0f;
};
