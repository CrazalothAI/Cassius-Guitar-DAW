#pragma once
#include <NAM/get_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
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
        hostRate = rate;
        modelRate = expected > 0 ? expected : rate;
        resampling = std::abs(modelRate - hostRate) >= 1.0;
        const auto modelBlock = static_cast<int>(std::ceil(static_cast<double>(blockSize) * modelRate / hostRate)) + 16;
        output.resize(static_cast<size_t>(juce::jmax(blockSize, modelBlock)));
        resampledInput.resize(static_cast<size_t>(modelBlock));
        resampledOutput.resize(static_cast<size_t>(modelBlock));
        engine->SetPrewarmOnReset(true);
        engine->Reset(modelRate, resampling ? modelBlock : blockSize);
        inputResampler.reset(); outputResampler.reset();
        // NAM captures and plugin-style processors often settle their internal
        // state on the first few callbacks. Prime them before exposing audio.
        for (int i = 0; i < 5; ++i)
        {
            std::fill(resampledInput.begin(), resampledInput.end(), 0.0f);
            std::fill(resampledOutput.begin(), resampledOutput.end(), 0.0f);
            std::fill(output.begin(), output.end(), 0.0f);
            float* inputs[] { resampling ? resampledInput.data() : output.data() };
            float* outputs[] { resampling ? resampledOutput.data() : output.data() };
            engine->process(inputs, outputs, resampling ? modelBlock : blockSize);
        }
    }
    bool isCompatible() const { return true; }
    double expectedRate() const { return engine->GetExpectedSampleRate(); }
    void process(float* samples, int size)
    {
        if (!resampling)
        {
            float* inputs[] {samples};
            float* outputs[] {output.data()};
            engine->process(inputs, outputs, size);
            std::copy_n(output.data(), size, samples);
            return;
        }
        const auto modelSamples = juce::jmin(static_cast<int>(resampledInput.size() - 1),
            static_cast<int>(std::ceil(static_cast<double>(size) * modelRate / hostRate)));
        std::fill(resampledInput.begin(), resampledInput.end(), size > 0 ? samples[size - 1] : 0.0f);
        inputResampler.process(hostRate / modelRate, samples, resampledInput.data(), modelSamples);
        float* inputs[] {resampledInput.data()};
        float* outputs[] {resampledOutput.data()};
        engine->process(inputs, outputs, modelSamples);
        std::fill(output.begin(), output.begin() + size, 0.0f);
        outputResampler.process(modelRate / hostRate, resampledOutput.data(), output.data(), size);
        std::copy_n(output.data(), size, samples);
    }
private:
    std::unique_ptr<nam::DSP> engine;
    std::vector<float> output;
    bool compatible = true;
    double hostRate = 48000, modelRate = 48000;
    bool resampling = false;
    juce::LagrangeInterpolator inputResampler, outputResampler;
    std::vector<float> resampledInput, resampledOutput;
};
