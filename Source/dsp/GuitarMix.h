#pragma once
#include "PedalEq.h"

// Listening-only guitar shaping. Return a delta so the recorder can capture
// the original rig before the player adds backing audio to the same buffer.
class GuitarMix
{
public:
    void prepare(double rate, int block, float level, float focus)
    {
        original.setSize(2, block);
        eq.prepare(rate, settings(focus));
        gain.prepare({rate, static_cast<juce::uint32>(block), 2});
        gain.setRampDurationSeconds(.03); gain.setGainDecibels(level); gain.reset();
    }
    void difference(const juce::AudioBuffer<float>& guitar, juce::AudioBuffer<float>& delta, float level, float focus)
    {
        const int frames = guitar.getNumSamples(), channels = guitar.getNumChannels();
        jassert(frames <= original.getNumSamples() && channels <= 2);
        for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < frames; ++i) {
            const float x = guitar.getSample(ch, i);
            original.setSample(ch, i, std::isfinite(x) ? x : 0);
            delta.setSample(ch, i, std::isfinite(x) ? x : 0);
        }
        eq.configure(settings(focus)); gain.setGainDecibels(level);
        eq.process(delta);
        juce::dsp::AudioBlock<float> block(delta); juce::dsp::ProcessContextReplacing<float> context(block);
        gain.process(context);
        for (int ch = 0; ch < channels; ++ch)
            delta.addFrom(ch, 0, original, ch, 0, frames, -1);
    }
private:
    static PedalEq::Settings settings(float focus)
    {
        const float amount = juce::jlimit(0.f, 1.f, focus / 100);
        // Broad definition at 1.2 kHz, less 120/350 Hz overlap with bass/drums,
        // and a small high shelf cut to avoid adding high-gain fizz.
        return {amount > 0, -2 * amount, -2 * amount, 4 * amount, -amount};
    }
    PedalEq eq;
    juce::dsp::Gain<float> gain;
    juce::AudioBuffer<float> original;
};
