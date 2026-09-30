#pragma once
#include <NAM/get_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "StreamResampler.h"
#include <optional>
#include <string>
class NamWrapper
{
public:
    // Captures are levelled to NAM's -18 dB loudness convention, as the official
    // NAM plugin does, so a quiet capture is not 20 dB below a hot one.
    static constexpr double targetLoudnessDb = -18.0;

    explicit NamWrapper(const juce::File& file)
    {
        // Parse once, keeping the metadata NAM Core does not expose (gear type).
        nlohmann::json config;
        try { config = nlohmann::json::parse(file.loadFileAsString().toStdString()); }
        catch (const std::exception&) { throw std::runtime_error("This .nam file could not be read as a capture."); }
        if (const auto metadata = config.find("metadata"); metadata != config.end() && metadata->is_object())
            if (const auto gear = metadata->find("gear_type"); gear != metadata->end() && gear->is_string())
                gearType = gear->get<std::string>();
        engine = nam::get_dsp(config, nam::DspLoadOptions {false});
        if (!engine) throw std::runtime_error("The capture did not produce a NAM engine.");
        if (engine->NumInputChannels() != 1 || engine->NumOutputChannels() != 1)
            throw std::runtime_error("This version of Cassian requires a mono-input, mono-output amp capture.");
        if (engine->HasLoudness())
            loudnessDb = engine->GetLoudness();
        // A cabinet is part of the capture for amp+cab and full-rig gear types. Without
        // metadata, fall back to the naming convention used by full-rig packs.
        const auto name = file.getFileNameWithoutExtension().toLowerCase().removeCharacters(" -_");
        includesCabinet = gearType.empty() ? (name.contains("fullrig") || name.contains("cab"))
                                           : juce::String(gearType).containsIgnoreCase("cab") || gearType == "studio";
        cabinetKnown = !gearType.empty() || includesCabinet;
    }
    // Called on the loader thread before the model is published to the audio thread.
    void prepare(double rate, int blockSize)
    {
        const auto expected = engine->GetExpectedSampleRate();
        hostRate = rate;
        modelRate = expected > 0 ? expected : rate;
        resampling = std::abs(modelRate - hostRate) >= 1.0;
        modelBlock = static_cast<int>(std::ceil(static_cast<double>(blockSize) * modelRate / hostRate)) + 16;
        output.assign(static_cast<size_t>(juce::jmax(blockSize, modelBlock)), 0.0f);
        modelInput.assign(static_cast<size_t>(modelBlock), 0.0f);
        modelOutput.assign(static_cast<size_t>(modelBlock), 0.0f);
        engine->SetPrewarmOnReset(true);
        engine->Reset(modelRate, resampling ? modelBlock : blockSize);
        if (resampling)
        {
            toModel.prepare(hostRate, modelRate, modelBlock + blockSize);
            fromModel.prepare(modelRate, hostRate, modelBlock + blockSize);
            // A few samples of headroom so a full block can always be pulled.
            fromModel.pushSilence(16);
        }
        // NAM captures and plugin-style processors often settle their internal
        // state on the first few callbacks. Prime them before exposing audio.
        for (int i = 0; i < 5; ++i)
        {
            std::fill(modelInput.begin(), modelInput.end(), 0.0f);
            float* inputs[] { modelInput.data() };
            float* outputs[] { modelOutput.data() };
            engine->process(inputs, outputs, resampling ? modelBlock : blockSize);
        }
    }
    double expectedRate() const { return engine->GetExpectedSampleRate(); }
    bool hasLoudness() const { return loudnessDb.has_value(); }
    double loudness() const { return loudnessDb.value_or(0.0); }
    // Gain that brings this capture to the target loudness, limited to a sane range.
    double levelMatchDb() const { return loudnessDb ? juce::jlimit(-12.0, 24.0, targetLoudnessDb - *loudnessDb) : 0.0; }
    const std::string& gear() const { return gearType; }
    bool hasCabinet() const { return includesCabinet; }
    bool cabinetIsKnown() const { return cabinetKnown; }
    void setOutputGain(float gain) { outputGain = gain; }
    void process(float* samples, int size)
    {
        if (!resampling)
        {
            float* inputs[] {samples};
            float* outputs[] {output.data()};
            engine->process(inputs, outputs, size);
            for (int i = 0; i < size; ++i) samples[i] = output[static_cast<size_t>(i)] * outputGain;
            return;
        }
        toModel.push(samples, size);
        const int count = juce::jmin(toModel.available(), modelBlock);
        toModel.pull(modelInput.data(), count);
        float* inputs[] {modelInput.data()};
        float* outputs[] {modelOutput.data()};
        if (count > 0) engine->process(inputs, outputs, count);
        fromModel.push(modelOutput.data(), count);
        const int ready = juce::jmin(size, fromModel.available());
        fromModel.pull(samples, ready);
        for (int i = ready; i < size; ++i) samples[i] = ready > 0 ? samples[ready - 1] : 0.0f;
        for (int i = 0; i < size; ++i) samples[i] *= outputGain;
    }
private:
    std::unique_ptr<nam::DSP> engine;
    std::string gearType;
    std::optional<double> loudnessDb;
    bool includesCabinet = false, cabinetKnown = false;
    std::vector<float> output, modelInput, modelOutput;
    double hostRate = 48000, modelRate = 48000;
    int modelBlock = 0;
    bool resampling = false;
    float outputGain = 1.0f;
    StreamResampler toModel, fromModel;
};
