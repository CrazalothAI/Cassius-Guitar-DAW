#pragma once
#include <juce_dsp/juce_dsp.h>
class IrLoader
{
public:
    // Called under the processor DSP lock; never concurrently with process.
    void load(const juce::File& file)
    {
        convolution.loadImpulseResponse(file, juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes, 0, juce::dsp::Convolution::Normalise::yes);
        loaded = true;
    }
    void prepare(const juce::dsp::ProcessSpec& spec) { convolution.prepare(spec); }
    void process(juce::dsp::ProcessContextReplacing<float>& context)
    { if (loaded) convolution.process(context); }
    void clear() { loaded = false; }
private:
    juce::dsp::Convolution convolution;
    bool loaded = false;
};
