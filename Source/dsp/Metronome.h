#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>

// Click track mixed after the rig, so it never reaches the gate, amp or effects.
// Standalone it runs its own clock; in a DAW that is playing it follows the host's
// tempo and bar grid instead. The first beat of each bar is accented (higher and
// louder); each click is a short tone with a soft attack, so it is crisp but never
// clicks in the digital sense.
class Metronome
{
public:
    struct Settings { bool on; double bpm; int beatsPerBar; float levelDb; };

    void prepare(double sampleRate)
    {
        rate = sampleRate;
        decay = static_cast<float>(std::exp(-1.0 / (0.012 * rate)));
        attack = static_cast<float>(1.0 - std::exp(-1.0 / (0.0007 * rate)));
        levelGain.reset(rate, .01);
        reset();
    }
    void reset() { phase = 0; beat = -1; envelope = 0; target = 0; wasOn = false; wasHostPlaying = false; levelGain.setCurrentAndTargetValue(0); }

    // Adds the clicks to `channels`. `host` is the DAW transport, if any.
    void process(float* const* channels, int numChannels, int numSamples, const Settings& s,
                 const juce::Optional<juce::AudioPlayHead::PositionInfo>& host)
    {
        levelGain.setTargetValue(s.on ? juce::Decibels::decibelsToGain(juce::jlimit(-60.0f, 0.0f, s.levelDb)) : 0.0f);
        if (!s.on && !levelGain.isSmoothing()) { wasOn = false; envelope = 0; target = 0; return; }
        const int beats = juce::jlimit(1, 12, s.beatsPerBar);
        double bpm = juce::jlimit(20.0, 400.0, s.bpm);
        // A playing host owns the grid: its tempo, its beat position, its bar length.
        double hostBeat = -1, hostBarStart = 0; int hostBeats = beats;
        const bool hostPlaying = host && host->getIsPlaying() && host->getPpqPosition() && host->getBpm();
        if (hostPlaying)
        {
            bpm = *host->getBpm();
            hostBeat = *host->getPpqPosition();
            hostBarStart = host->getPpqPositionOfLastBarStart().orFallback(0.0);
            if (const auto signature = host->getTimeSignature()) hostBeats = juce::jlimit(1, 12, signature->numerator);
        }
        const double beatsPerSample = bpm / 60.0 / rate;
        // Turning it on, or the transport starting or stopping, restarts the count; a
        // playing host's position is taken afresh every block, so loops and jumps follow.
        if (!wasOn || hostPlaying != wasHostPlaying) { phase = 0; beat = -1; }
        wasOn = true; wasHostPlaying = hostPlaying;
        if (hostPlaying) phase = hostBeat;
        const int barLength = hostPlaying ? hostBeats : beats;
        const auto barStart = hostPlaying ? static_cast<long long>(std::llround(hostBarStart)) : 0LL;
        for (int i = 0; i < numSamples; ++i)
        {
            // A click on every whole beat: crossed during this sample, or (a host
            // block starting exactly on one) not yet clicked.
            const double next = phase + beatsPerSample;
            long long whole = -1;
            if (std::floor(next) > std::floor(phase)) whole = static_cast<long long>(std::floor(next));
            else if (i == 0 && phase - std::floor(phase) < beatsPerSample && beat != static_cast<long long>(std::floor(phase)))
                whole = static_cast<long long>(std::floor(phase));
            if (s.on && whole >= 0 && whole != beat)
            {
                const auto inBar = static_cast<int>(((whole - barStart) % barLength + barLength) % barLength);
                trigger(inBar, barLength);
                beat = whole;
            }
            phase = next;
            envelope += attack * (target - envelope);
            target *= decay;
            const float tone = std::sin(oscillator) * envelope * levelGain.getNextValue() * (accent ? 1.0f : 0.6f);
            oscillator += step;
            if (oscillator > juce::MathConstants<float>::twoPi) oscillator -= juce::MathConstants<float>::twoPi;
            for (int ch = 0; ch < numChannels; ++ch) channels[ch][i] += tone;
        }
        // The free-running clock wraps on whole bars so its precision never decays.
        if (!hostPlaying && phase > 1e6)
        {
            const double wrap = static_cast<double>(beats) * std::floor(1e6 / beats);
            phase -= wrap; beat -= static_cast<long long>(wrap);
        }
    }
    // For the editor's beat light: the beat within the bar (0 = downbeat) and a count
    // that moves on every click.
    int currentBeat() const { return lastBeat.load(); }
    int clickCount() const { return clicks.load(); }
private:
    void trigger(int inBar, int beatsPerBar)
    {
        accent = inBar == 0 && beatsPerBar > 1;
        step = juce::MathConstants<float>::twoPi * (accent ? 1800.0f : 1200.0f) / static_cast<float>(rate);
        oscillator = 0; target = 1;
        lastBeat.store(inBar); clicks.fetch_add(1);
    }
    double rate = 48000, phase = 0;
    long long beat = -1;
    float decay = 0, attack = 0, envelope = 0, target = 0, oscillator = 0, step = 0;
    bool accent = false, wasOn = false, wasHostPlaying = false;
    juce::SmoothedValue<float> levelGain;
    std::atomic<int> lastBeat {0}, clicks {0};
};
