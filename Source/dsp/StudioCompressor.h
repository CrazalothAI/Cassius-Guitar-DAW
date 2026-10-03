#pragma once
#include <juce_dsp/juce_dsp.h>

// Stereo-linked peak compressor. All automation ramps; no allocation in process.
class StudioCompressor
{
public:
    struct Settings { float mix, threshold, ratio, attack, release, makeup; };
    void prepare(double rate, Settings s)
    {
        sampleRate = rate; envelope = reduction = 0;
        for (auto* p : {&mix, &threshold, &ratio, &attack, &release, &makeup}) p->reset(rate, .03);
        configure(s);
        for (auto* p : {&mix, &threshold, &ratio, &attack, &release, &makeup}) p->setCurrentAndTargetValue(p->getTargetValue());
    }
    void configure(Settings s)
    {
        mix.setTargetValue(s.mix / 100); threshold.setTargetValue(s.threshold);
        ratio.setTargetValue(s.ratio); makeup.setTargetValue(juce::Decibels::decibelsToGain(s.makeup));
        attack.setTargetValue(std::exp(-1.0f / (static_cast<float>(sampleRate) * s.attack * .001f)));
        release.setTargetValue(std::exp(-1.0f / (static_cast<float>(sampleRate) * s.release * .001f)));
    }
    void process(float* const* audio, int channels, int samples)
    {
        if (!mix.isSmoothing() && mix.getCurrentValue() == 0) {
            envelope = reduction = 0;
            for (auto* p : {&threshold, &ratio, &attack, &release, &makeup}) p->skip(samples);
            return;
        }
        for (int i = 0; i < samples; ++i) {
            float peak = 0; for (int ch = 0; ch < channels; ++ch) peak = juce::jmax(peak, std::abs(audio[ch][i]));
            const float a = attack.getNextValue(), r = release.getNextValue();
            const float coefficient = peak > envelope ? a : r;
            envelope = coefficient * envelope + (1 - coefficient) * peak;
            const float over = juce::jmax(0.0f, juce::Decibels::gainToDecibels(envelope, -120.0f) - threshold.getNextValue());
            reduction = over * (1 - 1 / ratio.getNextValue());
            const float wet = juce::Decibels::decibelsToGain(-reduction) * makeup.getNextValue(), blend = mix.getNextValue();
            for (int ch = 0; ch < channels; ++ch) audio[ch][i] *= 1 + blend * (wet - 1);
        }
    }
    float reductionDb() const { return reduction * mix.getCurrentValue(); }
private:
    double sampleRate = 48000;
    float envelope = 0, reduction = 0;
    juce::SmoothedValue<float> mix, threshold, ratio, attack, release, makeup;
};
