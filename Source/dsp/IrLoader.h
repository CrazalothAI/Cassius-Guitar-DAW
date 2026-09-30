#pragma once
#include <juce_dsp/juce_dsp.h>
class IrLoader
{
public:
    // Safe while audio runs: JUCE builds the new IR on its own background thread
    // and crossfades it in, so loading never holds the DSP lock.
    void load(juce::AudioBuffer<float>&& impulse, double sampleRate)
    {
        convolution.loadImpulseResponse(std::move(impulse), sampleRate, juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::yes);
        loaded.store(true);
    }
    void prepare(const juce::dsp::ProcessSpec& spec) { convolution.prepare(spec); }
    void process(juce::dsp::ProcessContextReplacing<float>& context)
    { if (loaded.load()) convolution.process(context); }
    void clear() { loaded.store(false); }
    bool isLoaded() const { return loaded.load(); }
private:
    juce::dsp::Convolution convolution;
    std::atomic<bool> loaded {false};
};
