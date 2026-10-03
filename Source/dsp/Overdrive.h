#pragma once
#include <juce_dsp/juce_dsp.h>

// An original mid-forward drive voice, with 4x oversampled soft clipping.
// The dry path stays sample-exact when bypassed; buffers are allocated in prepare.
class Overdrive
{
public:
    struct Settings { bool enabled; float drive, tone, level, tight; };
    void prepare(double rate, int blockSize, Settings s)
    {
        sampleRate = rate; low = toneLow = dcLow = 0;
        dcCoefficient = 1 - std::exp(-juce::MathConstants<float>::twoPi * 15 / static_cast<float>(rate * 4));
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(1, 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampling->initProcessing(static_cast<size_t>(blockSize));
        wet.setSize(1, blockSize);
        for (auto* p : {&blend, &drive, &tone, &level, &tight}) p->reset(rate * 4, .02);
        configure(s);
        for (auto* p : {&blend, &drive, &tone, &level, &tight}) p->setCurrentAndTargetValue(p->getTargetValue());
        // Bypass blend advances at the base rate.
        blend.reset(rate, .02); blend.setCurrentAndTargetValue(s.enabled ? 1.0f : 0.0f);
    }
    void configure(Settings s)
    {
        blend.setTargetValue(s.enabled ? 1.0f : 0.0f);
        drive.setTargetValue(1 + s.drive * .19f);
        tone.setTargetValue(1 - std::exp(-juce::MathConstants<float>::twoPi * (1200 + s.tone * 68) / static_cast<float>(sampleRate * 4)));
        level.setTargetValue(juce::Decibels::decibelsToGain(s.level));
        tight.setTargetValue(1 - std::exp(-juce::MathConstants<float>::twoPi * s.tight / static_cast<float>(sampleRate * 4)));
    }
    void process(float* audio, int samples)
    {
        if (!blend.isSmoothing() && blend.getCurrentValue() == 0) {
            // Reset after the fade, so a later enable cannot replay stale state.
            oversampling->reset(); low = toneLow = dcLow = 0;
            for (auto* p : {&drive, &tone, &level, &tight}) p->skip(samples * 4);
            return;
        }
        wet.copyFrom(0, 0, audio, samples);
        juce::dsp::AudioBlock<float> block(wet); auto part = block.getSubBlock(0, static_cast<size_t>(samples));
        auto up = oversampling->processSamplesUp(part); auto* x = up.getChannelPointer(0);
        for (size_t i = 0; i < up.getNumSamples(); ++i) {
            const float hp = tight.getNextValue();
            low += hp * (x[i] - low);
            // Bounded soft clipping, followed by DC removal and tone filtering.
            const float d = drive.getNextValue(); const float clipped = .35f * std::tanh((x[i] - low) * d * 4);
            dcLow += dcCoefficient * (clipped - dcLow);
            const float lp = tone.getNextValue();
            toneLow += lp * (clipped - dcLow - toneLow);
            x[i] = toneLow * level.getNextValue();
        }
        oversampling->processSamplesDown(part);
        const auto* processed = wet.getReadPointer(0);
        for (int i = 0; i < samples; ++i) audio[i] += blend.getNextValue() * (processed[i] - audio[i]);
    }
    float latencySamples() const { return oversampling ? oversampling->getLatencyInSamples() : 0; }
private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> blend, drive, tone, level, tight;
    double sampleRate = 48000;
    float low = 0, toneLow = 0, dcLow = 0, dcCoefficient = 0;
};
