#pragma once
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <atomic>
#include <algorithm>
#include <array>

// Real-time zero-allocation pitch tracker and chromatic tuner engine.
// McLeod Pitch Method (NSDF normalized autocorrelation) tracks guitar fundamentals
// from E0 (~20.6 Hz) to ~1.1 kHz. The lag search runs on a 4x decimated signal,
// with the numerator the only per-lag cost; the winning lag is then refined at
// the full rate, so a 12 ms update costs a fraction of a millisecond instead of
// the several milliseconds that previously overran the audio callback.
// It only runs while active: the tuner is open or Thicken needs the pitch.
class PitchTracker
{
public:
    void prepare(double sampleRate)
    {
        rate = sampleRate;
        decimation = juce::jmax(1, static_cast<int>(std::round(rate / 12000.0)));
        lowRate = rate / decimation;
        fullRing.assign(fullSize, 0.0f); lowRing.assign(lowSize, 0.0f);
        scratch.assign(lowSize + fullSize, 0.0f);
        fullPos = lowPos = phase = 0;
        samplesSinceLastEstimate = 0;
        estimateInterval = static_cast<int>(rate * 0.012); // ~12 ms refresh
        // 4th-order Butterworth anti-aliasing low-pass ahead of decimation.
        using C = juce::dsp::IIR::ArrayCoefficients<float>;
        const auto cutoff = static_cast<float>(juce::jmin(1500.0, lowRate * 0.4));
        antiAlias[0].c = C::makeLowPass(rate, cutoff, 0.5412f);
        antiAlias[1].c = C::makeLowPass(rate, cutoff, 1.3066f);
        for (auto& f : antiAlias) f.z1 = f.z2 = 0;
        clear();
    }

    // Idle trackers cost nothing; re-activation starts from a clean history.
    void setActive(bool shouldRun)
    {
        if (shouldRun == active) return;
        active = shouldRun;
        std::fill(fullRing.begin(), fullRing.end(), 0.0f);
        std::fill(lowRing.begin(), lowRing.end(), 0.0f);
        for (auto& f : antiAlias) f.z1 = f.z2 = 0;
        clear();
    }
    bool isActive() const noexcept { return active; }

    void processSample(float input)
    {
        if (!active) return;
        fullRing[static_cast<size_t>(fullPos)] = input;
        fullPos = (fullPos + 1) % fullSize;
        float filtered = input;
        for (auto& f : antiAlias) filtered = f.tick(filtered);
        if (++phase >= decimation)
        {
            phase = 0;
            lowRing[static_cast<size_t>(lowPos)] = filtered;
            lowPos = (lowPos + 1) % lowSize;
        }
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

    void clear()
    {
        detectedHz.store(0.0f, std::memory_order_relaxed);
        detectedCents.store(0.0f, std::memory_order_relaxed);
        detectedMidiNote.store(-1, std::memory_order_relaxed);
        noteActive.store(false, std::memory_order_relaxed);
        trackedPitchHz = 0.0f;
    }

    // Copies the newest `count` samples of a ring into `destination`, oldest first.
    static void unroll(const std::vector<float>& ring, int writePos, int count, float* destination)
    {
        const int size = static_cast<int>(ring.size());
        for (int i = 0; i < count; ++i)
            destination[i] = ring[static_cast<size_t>((writePos - count + i + size) % size)];
    }

    // NSDF of `x` at one lag over `window` samples.
    static float nsdf(const float* x, int lag, int window)
    {
        float num = 0.0f, den = 0.0f;
        for (int j = 0; j < window; ++j) { num += x[j] * x[j + lag]; den += x[j] * x[j] + x[j + lag] * x[j + lag]; }
        return den > 1e-12f ? 2.0f * num / den : 0.0f;
    }

    void estimatePitch()
    {
        // Coarse search at the decimated rate.
        constexpr int lowWindow = 512;
        const int minLag = juce::jmax(2, static_cast<int>(lowRate / 1100.0));
        const int maxLag = juce::jmin(lowSize - lowWindow - 4, static_cast<int>(lowRate / 20.0));
        const int lowNeeded = lowWindow + maxLag + 2;
        float* x = scratch.data();
        unroll(lowRing, lowPos, lowNeeded, x);

        float energy = 0.0f;
        for (int i = 0; i < lowWindow; ++i) energy += x[i] * x[i];
        if (std::sqrt(energy / static_cast<float>(lowWindow)) < 0.002f) { clear(); return; }

        // den(lag) = sum x[j]^2 + sum x[j+lag]^2, the second term updated as the lag slides.
        float shifted = 0.0f;
        for (int j = 0; j < lowWindow; ++j) shifted += x[j + minLag] * x[j + minLag];
        auto score = [&](int lag) {
            float num = 0.0f;
            for (int j = 0; j < lowWindow; ++j) num += x[j] * x[j + lag];
            const float den = energy + shifted;
            return den > 1e-12f ? 2.0f * num / den : 0.0f;
        };
        auto slide = [&](int lag) { shifted += x[lag + lowWindow] * x[lag + lowWindow] - x[lag] * x[lag]; };

        std::array<int, 64> peakLag {}; std::array<float, 64> peakScore {};
        int peaks = 0; float maxScore = 0.0f;
        float prev = score(minLag); slide(minLag);
        float curr = score(minLag + 1); slide(minLag + 1);
        for (int lag = minLag + 1; lag < maxLag; ++lag)
        {
            const float next = score(lag + 1); slide(lag + 1);
            if (curr > prev && curr >= next && curr > 0.45f && peaks < static_cast<int>(peakLag.size()))
            {
                peakLag[static_cast<size_t>(peaks)] = lag; peakScore[static_cast<size_t>(peaks)] = curr; ++peaks;
                maxScore = std::max(maxScore, curr);
            }
            prev = curr; curr = next;
        }
        if (peaks == 0 || maxScore < 0.5f) { clear(); return; }

        // MPM peak selection: first local maximum within 82% of the strongest.
        int coarseLag = peakLag[0];
        for (int p = 0; p < peaks; ++p)
            if (peakScore[static_cast<size_t>(p)] >= 0.82f * maxScore) { coarseLag = peakLag[static_cast<size_t>(p)]; break; }

        // Refine at the full rate around the coarse estimate.
        constexpr int fullWindow = 1024;
        const int centre = coarseLag * decimation, reach = decimation + 1;
        const int fullMaxLag = centre + reach + 1;
        if (fullWindow + fullMaxLag + 2 > fullSize) { clear(); return; }
        float* y = scratch.data() + lowSize;
        unroll(fullRing, fullPos, fullWindow + fullMaxLag + 2, y);
        int bestLag = centre; float best = -1.0f;
        for (int lag = juce::jmax(2, centre - reach); lag <= centre + reach; ++lag)
        {
            const float s = nsdf(y, lag, fullWindow);
            if (s > best) { best = s; bestLag = lag; }
        }
        const float y0 = nsdf(y, bestLag - 1, fullWindow), y2 = nsdf(y, bestLag + 1, fullWindow);
        const float delta = (y2 - y0) / (2.0f * (2.0f * best - y0 - y2) + 1e-9f);
        const float pitchHz = static_cast<float>(rate) / (static_cast<float>(bestLag) + juce::jlimit(-0.5f, 0.5f, delta));

        if (pitchHz >= 18.0f && pitchHz <= 1100.0f)
        {
            trackedPitchHz = trackedPitchHz <= 0.0f ? pitchHz : trackedPitchHz + 0.35f * (pitchHz - trackedPitchHz);
            const float midiExact = 69.0f + 12.0f * std::log2(pitchHz / 440.0f);
            const int nearestNote = static_cast<int>(std::round(midiExact));
            const float cents = (midiExact - static_cast<float>(nearestNote)) * 100.0f;
            detectedHz.store(pitchHz, std::memory_order_relaxed);
            detectedCents.store(juce::jlimit(-50.0f, 50.0f, cents), std::memory_order_relaxed);
            detectedMidiNote.store(juce::jlimit(0, 127, nearestNote), std::memory_order_relaxed);
            noteActive.store(true, std::memory_order_relaxed);
            return;
        }
        clear();
    }

    static constexpr int fullSize = 8192, lowSize = 2048; // covers ~20 Hz up to 96 kHz
    double rate = 48000.0, lowRate = 12000.0;
    int decimation = 4, phase = 0;
    bool active = false;
    std::vector<float> fullRing, lowRing, scratch;
    std::array<Biquad, 2> antiAlias {};
    int fullPos = 0, lowPos = 0;
    int samplesSinceLastEstimate = 0;
    int estimateInterval = 576;
    float trackedPitchHz = 0.0f;

    std::atomic<float> detectedHz {0.0f};
    std::atomic<float> detectedCents {0.0f};
    std::atomic<int> detectedMidiNote {-1};
    std::atomic<bool> noteActive {false};
};
