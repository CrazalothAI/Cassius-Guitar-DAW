#pragma once
#include "IrLoader.h"

class DualCab
{
public:
    struct Settings {
        bool second = false; float blend = 50;
        float levelA = 0, levelB = 0, panA = 0, panB = 0;
        bool invertA = false, invertB = false;
        float delayA = 0, delayB = 0, lowCut = 20, highCut = 20000;
    };
    void load(juce::AudioBuffer<float>&& audio, double impulseRate, int slot = 0) { ir[slot].load(std::move(audio), impulseRate); }
    void clear(int slot = 0) { ir[slot].clear(); }
    bool isLoaded(int slot = 0) const { return ir[slot].isLoaded(); }
    bool hasExternal() const { return isLoaded() || (settings.second && isLoaded(1)); }
    void prepare(const juce::dsp::ProcessSpec& spec) { prepare(spec, Settings {}); }
    void prepare(const juce::dsp::ProcessSpec& spec, Settings s)
    {
        rate = spec.sampleRate; settings = s; filterTick = 0; lastLow = lastHigh = -1;
        for (int k = 0; k < 2; ++k) {
            ir[k].prepare(spec); scratch[k].setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
            alignment[k].setMaximumDelayInSamples(static_cast<int>(rate * .011)); alignment[k].prepare(spec); alignment[k].reset();
            for (auto* p : {&gain[k], &pan[k], &time[k]}) p->reset(rate, .02);
        }
        for (auto* p : {&blend, &lowCut, &highCut, &lowMix, &highMix}) p->reset(rate, .03);
        hp.prepare(spec); lp.prepare(spec); hp.setType(juce::dsp::StateVariableTPTFilterType::highpass); lp.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        configure(s);
        for (int k = 0; k < 2; ++k) for (auto* p : {&gain[k], &pan[k], &time[k]}) p->setCurrentAndTargetValue(p->getTargetValue());
        for (auto* p : {&blend, &lowCut, &highCut, &lowMix, &highMix}) p->setCurrentAndTargetValue(p->getTargetValue());
    }
    void configure(Settings s)
    {
        settings = s; blend.setTargetValue(s.second && isLoaded(1) ? s.blend / 100 : 0);
        gain[0].setTargetValue(juce::Decibels::decibelsToGain(s.levelA) * (s.invertA ? -1 : 1));
        gain[1].setTargetValue(juce::Decibels::decibelsToGain(s.levelB) * (s.invertB ? -1 : 1));
        pan[0].setTargetValue(s.panA / 100); pan[1].setTargetValue(s.panB / 100);
        time[0].setTargetValue(s.delayA * static_cast<float>(rate) / 1000); time[1].setTargetValue(s.delayB * static_cast<float>(rate) / 1000);
        lowCut.setTargetValue(s.lowCut); highCut.setTargetValue(juce::jmin(s.highCut, static_cast<float>(rate) * .45f));
        lowMix.setTargetValue(s.lowCut > 20 ? 1.0f : 0.0f); highMix.setTargetValue(s.highCut < 20000 ? 1.0f : 0.0f);
    }
    void process(juce::dsp::ProcessContextReplacing<float>& context)
    {
        auto block = context.getOutputBlock(); const int samples = static_cast<int>(block.getNumSamples()), channels = static_cast<int>(block.getNumChannels());
        const bool a = isLoaded(), b = isLoaded(1) && (settings.second || blend.getCurrentValue() > 0);
        if (!a && !b) return;
        for (int k = 0; k < 2; ++k) if (k == 0 ? a : b) {
            for (int ch = 0; ch < channels; ++ch) scratch[k].copyFrom(ch, 0, block.getChannelPointer(static_cast<size_t>(ch)), samples);
            juce::dsp::AudioBlock<float> full(scratch[k]); auto part = full.getSubBlock(0, static_cast<size_t>(samples));
            juce::dsp::ProcessContextReplacing<float> partContext(part); ir[k].process(partContext);
        }
        for (int i = 0; i < samples; ++i) {
            const float mix = blend.getNextValue(); float weights[] {a ? (b ? 1 - mix : 1) : 0, b ? (a ? mix : 1) : 0};
            float levels[2], pans[2], times[2];
            for (int k = 0; k < 2; ++k) { levels[k] = gain[k].getNextValue(); pans[k] = pan[k].getNextValue(); times[k] = time[k].getNextValue(); }
            for (int ch = 0; ch < channels; ++ch) {
                float sum = 0;
                for (int k = 0; k < 2; ++k) {
                    alignment[k].pushSample(ch, (k == 0 ? a : b) ? scratch[k].getSample(ch, i) : 0);
                    const float x = alignment[k].popSample(ch, times[k]);
                    const float balance = channels == 1 ? 1 : ch == 0 ? 1 - juce::jmax(0.0f, pans[k]) : 1 + juce::jmin(0.0f, pans[k]);
                    sum += weights[k] * levels[k] * balance * x;
                }
                block.getChannelPointer(static_cast<size_t>(ch))[i] = sum;
            }
        }
        filter(block);
    }
    void filter(juce::dsp::AudioBlock<float>& block)
    {
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            const float loHz = lowCut.getNextValue(), hiHz = highCut.getNextValue();
            if ((filterTick++ & 15) == 0) {
                if (loHz != lastLow) { lastLow = loHz; hp.setCutoffFrequency(loHz); }
                if (hiHz != lastHigh) { lastHigh = hiHz; lp.setCutoffFrequency(hiHz); }
            }
            const float lo = lowMix.getNextValue(), hi = highMix.getNextValue();
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
                auto& x = block.getChannelPointer(ch)[i]; x += lo * (hp.processSample(static_cast<int>(ch), x) - x);
                x += hi * (lp.processSample(static_cast<int>(ch), x) - x);
            }
        }
    }
private:
    IrLoader ir[2]; juce::AudioBuffer<float> scratch[2];
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> alignment[2];
    juce::SmoothedValue<float> gain[2], pan[2], time[2], blend, lowCut, highCut, lowMix, highMix;
    juce::dsp::StateVariableTPTFilter<float> hp, lp;
    double rate = 48000; Settings settings;
    unsigned filterTick = 0; float lastLow = -1, lastHigh = -1;
};
