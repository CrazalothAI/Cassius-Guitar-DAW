#pragma once
#include <juce_dsp/juce_dsp.h>

// Original guitar distortion. Allocation/oversampler setup stays in prepare;
// clipping runs at 4x with DC removal, input bass control and a wet tone filter.
class DistortionPedal {
public:
    struct Settings { float mode, drive, tone, tight, mix; };
    void prepare(double rate, int maximumBlock, Settings s) {
        sampleRate = rate; bass = dc = colour = 0;
        dcCoefficient = 1 - std::exp(-juce::MathConstants<float>::twoPi * 15 / static_cast<float>(rate * 4));
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampling->initProcessing(static_cast<size_t>(maximumBlock)); scratch.setSize(1, maximumBlock);
        for (auto* control : {&mode, &drive, &tone, &tight, &mix}) control->reset(rate * 4, .025);
        configure(s); for (auto* control : {&mode, &drive, &tone, &tight, &mix}) control->setCurrentAndTargetValue(control->getTargetValue());
    }
    void configure(Settings s) {
        mode.setTargetValue(juce::jlimit(0.f, 2.f, s.mode)); drive.setTargetValue(2 + juce::jlimit(0.f, 100.f, s.drive) * .38f);
        tone.setTargetValue(coefficient(1500 + juce::jlimit(0.f, 100.f, s.tone) * 75));
        tight.setTargetValue(coefficient(juce::jlimit(20.f, 250.f, s.tight))); mix.setTargetValue(juce::jlimit(0.f, 100.f, s.mix) / 100);
    }
    void process(float* audio, int samples) {
        if (!mix.isSmoothing() && mix.getCurrentValue() == 0) {
            oversampling->reset(); bass = dc = colour = 0;
            for (auto* control : {&mode, &drive, &tone, &tight, &mix}) control->skip(samples * 4); return;
        }
        scratch.copyFrom(0, 0, audio, samples); juce::dsp::AudioBlock<float> block(scratch);
        auto part = block.getSubBlock(0, static_cast<size_t>(samples)); auto up = oversampling->processSamplesUp(part); auto* data = up.getChannelPointer(0);
        for (size_t i = 0; i < up.getNumSamples(); ++i) {
            const auto dry = data[i]; bass += tight.getNextValue() * (dry - bass);
            const auto input = (dry - bass) * drive.getNextValue();
            const auto hard = .35f * juce::jlimit(-1.f, 1.f, input);
            const auto asymmetric = .35f * juce::jlimit(-.65f, 1.f, input);
            const auto fuzz = .35f * std::tanh(2.f * std::tanh(input * 1.8f));
            const auto voice = mode.getNextValue(); const auto shaped = voice <= 1 ? hard + voice * (asymmetric - hard) : asymmetric + (voice - 1) * (fuzz - asymmetric);
            dc += dcCoefficient * (shaped - dc); colour += tone.getNextValue() * (shaped - dc - colour);
            const auto blend = mix.getNextValue(); data[i] = dry + blend * (colour - dry);
        }
        oversampling->processSamplesDown(part); juce::FloatVectorOperations::copy(audio, scratch.getReadPointer(0), samples);
    }
    float latencySamples() const { return oversampling ? oversampling->getLatencyInSamples() : 0; }
private:
    float coefficient(float frequency) const { return 1 - std::exp(-juce::MathConstants<float>::twoPi * frequency / static_cast<float>(sampleRate * 4)); }
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> scratch;
    juce::SmoothedValue<float> mode, drive, tone, tight, mix;
    double sampleRate = 48000; float bass = 0, dc = 0, colour = 0, dcCoefficient = 0;
};
