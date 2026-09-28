#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
#include <atomic>
#include <algorithm>

// Real-time zero-allocation pitch tracker and chromatic tuner engine.
// Uses McLeod Pitch Method (MPM) / NSDF normalized autocorrelation with
// parabolic interpolation to track guitar fundamentals from E0 (~20.6 Hz) to E5 (~660 Hz).
class PitchTracker
{
public:
    void prepare(double sampleRate)
    {
        rate = sampleRate;
        bufferSize = 4096;
        ringBuffer.assign(static_cast<size_t>(bufferSize), 0.0f);
        scratch.assign(static_cast<size_t>(bufferSize), 0.0f);
        writePos = 0;
        samplesSinceLastEstimate = 0;
        estimateInterval = static_cast<int>(rate * 0.012); // ~12 ms refresh
        detectedHz.store(0.0f);
        detectedCents.store(0.0f);
        detectedMidiNote.store(-1);
        noteActive.store(false);
        trackedPitchHz = 0.0f;
    }

    void processSample(float input)
    {
        ringBuffer[static_cast<size_t>(writePos)] = input;
        writePos = (writePos + 1) % bufferSize;

        if (++samplesSinceLastEstimate >= estimateInterval)
        {
            samplesSinceLastEstimate = 0;
            estimatePitch();
        }
    }

    float getTrackedPitchHz() const noexcept { return trackedPitchHz; }
    float getDetectedHz() const noexcept { return detectedHz.load(std::memory_order_relaxed); }
    float getDetectedCents() const noexcept { return detectedCents.load(std::memory_order_relaxed); }
    int getDetectedMidiNote() const noexcept { return detectedMidiNote.load(std::memory_order_relaxed); }
    bool isNoteActive() const noexcept { return noteActive.load(std::memory_order_relaxed); }

    static juce::String midiNoteToName(int midiNote)
    {
        if (midiNote < 0 || midiNote > 127) return "—";
        static const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int noteIndex = midiNote % 12;
        const int octave = (midiNote / 12) - 1;
        return juce::String(noteNames[noteIndex]) + juce::String(octave);
    }

private:
    void estimatePitch()
    {
        constexpr int windowSize = 1024;
        const int minLag = std::max(2, static_cast<int>(rate / 1000.0));
        const int maxLag = std::min(2350, static_cast<int>(rate / 20.0));
        const int totalNeeded = windowSize + maxLag + 2;

        // Copy linearly from ringBuffer to contiguous scratch buffer
        for (int i = 0; i < totalNeeded; ++i)
        {
            const int readIdx = (writePos - totalNeeded + i + bufferSize) % bufferSize;
            scratch[static_cast<size_t>(i)] = ringBuffer[static_cast<size_t>(readIdx)];
        }

        // 1. RMS Energy check
        float energy = 0.0f;
        for (int i = 0; i < windowSize; ++i)
            energy += scratch[static_cast<size_t>(i)] * scratch[static_cast<size_t>(i)];
        const float rms = std::sqrt(energy / static_cast<float>(windowSize));

        if (rms < 0.002f)
        {
            detectedHz.store(0.0f, std::memory_order_relaxed);
            detectedCents.store(0.0f, std::memory_order_relaxed);
            detectedMidiNote.store(-1, std::memory_order_relaxed);
            noteActive.store(false, std::memory_order_relaxed);
            trackedPitchHz = 0.0f;
            return;
        }

        // 2. Contiguous NSDF Autocorrelation
        struct Peak { int lag; float score; };
        std::vector<Peak> peaks;
        peaks.reserve(32);

        float maxScore = 0.0f;

        auto computeNsdfAt = [&](int lag) -> float {
            float num = 0.0f, den1 = 0.0f, den2 = 0.0f;
            const float* p1 = scratch.data();
            const float* p2 = scratch.data() + lag;
            for (int j = 0; j < windowSize; ++j)
            {
                const float s1 = p1[j];
                const float s2 = p2[j];
                num += s1 * s2;
                den1 += s1 * s1;
                den2 += s2 * s2;
            }
            const float den = den1 + den2;
            return (den > 1e-9f) ? (2.0f * num / den) : 0.0f;
        };

        float prevNsdf = computeNsdfAt(minLag);
        float currNsdf = computeNsdfAt(minLag + 1);

        for (int lag = minLag + 1; lag < maxLag; ++lag)
        {
            const float nextNsdf = computeNsdfAt(lag + 1);
            if (currNsdf > prevNsdf && currNsdf >= nextNsdf && currNsdf > 0.45f)
            {
                peaks.push_back({ lag, currNsdf });
                if (currNsdf > maxScore)
                    maxScore = currNsdf;
            }
            prevNsdf = currNsdf;
            currNsdf = nextNsdf;
        }

        if (peaks.empty() || maxScore < 0.5f)
        {
            detectedHz.store(0.0f, std::memory_order_relaxed);
            detectedCents.store(0.0f, std::memory_order_relaxed);
            detectedMidiNote.store(-1, std::memory_order_relaxed);
            noteActive.store(false, std::memory_order_relaxed);
            return;
        }

        // 3. MPM Peak Selection: first local maximum >= 0.82 * maxScore
        const float threshold = 0.82f * maxScore;
        int bestLag = peaks[0].lag;
        float bestScore = peaks[0].score;

        for (const auto& p : peaks)
        {
            if (p.score >= threshold)
            {
                bestLag = p.lag;
                bestScore = p.score;
                break;
            }
        }

        // 4. Parabolic Peak Interpolation
        const float y0 = computeNsdfAt(bestLag - 1);
        const float y1 = bestScore;
        const float y2 = computeNsdfAt(bestLag + 1);

        const float delta = (y2 - y0) / (2.0f * (2.0f * y1 - y0 - y2) + 1e-9f);
        const float fineLag = static_cast<float>(bestLag) + juce::jlimit(-0.5f, 0.5f, delta);
        const float pitchHz = static_cast<float>(rate) / fineLag;

        if (pitchHz >= 18.0f && pitchHz <= 1100.0f)
        {
            if (trackedPitchHz <= 0.0f)
                trackedPitchHz = pitchHz;
            else
                trackedPitchHz += 0.35f * (pitchHz - trackedPitchHz);

            const float midiExact = 69.0f + 12.0f * std::log2(pitchHz / 440.0f);
            const int nearestNote = static_cast<int>(std::round(midiExact));
            const float cents = (midiExact - static_cast<float>(nearestNote)) * 100.0f;

            detectedHz.store(pitchHz, std::memory_order_relaxed);
            detectedCents.store(juce::jlimit(-50.0f, 50.0f, cents), std::memory_order_relaxed);
            detectedMidiNote.store(juce::jlimit(0, 127, nearestNote), std::memory_order_relaxed);
            noteActive.store(true, std::memory_order_relaxed);
            return;
        }

        detectedHz.store(0.0f, std::memory_order_relaxed);
        detectedCents.store(0.0f, std::memory_order_relaxed);
        detectedMidiNote.store(-1, std::memory_order_relaxed);
        noteActive.store(false, std::memory_order_relaxed);
    }

    double rate = 48000.0;
    int bufferSize = 4096;
    std::vector<float> ringBuffer;
    std::vector<float> scratch;
    int writePos = 0;
    int samplesSinceLastEstimate = 0;
    int estimateInterval = 576;
    float trackedPitchHz = 0.0f;

    std::atomic<float> detectedHz {0.0f};
    std::atomic<float> detectedCents {0.0f};
    std::atomic<int> detectedMidiNote {-1};
    std::atomic<bool> noteActive {false};
};
