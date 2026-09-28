#pragma once
#include <NAM/get_dsp.h>
#include <juce_core/juce_core.h>
class NamWrapper
{
public:
    explicit NamWrapper(const juce::File& file)
        : engine(nam::get_dsp(std::filesystem::path(file.getFullPathName().toWideCharPointer()), nam::DspLoadOptions {false}))
    {
        if (!engine) throw std::runtime_error("The capture did not produce a NAM engine.");
        if (engine->NumInputChannels() != 1 || engine->NumOutputChannels() != 1)
            throw std::runtime_error("This version of Cassian requires a mono-input, mono-output amp capture.");
    }
    void prepare(double rate, int blockSize)
    {
        const auto expected = engine->GetExpectedSampleRate();
        compatible = expected <= 0 || std::abs(expected - rate) < 1;
        output.resize(static_cast<size_t>(blockSize));
        engine->SetPrewarmOnReset(true);
        engine->Reset(rate, blockSize);
    }
    bool isCompatible() const { return compatible; }
    double expectedRate() const { return engine->GetExpectedSampleRate(); }
    void process(float* samples, int size)
    {
        if (!compatible) return;
        float* inputs[] {samples};
        float* outputs[] {output.data()};
        engine->process(inputs, outputs, size);
        std::copy_n(output.data(), size, samples);
    }
private:
    std::unique_ptr<nam::DSP> engine;
    std::vector<float> output;
    bool compatible = false;
};
