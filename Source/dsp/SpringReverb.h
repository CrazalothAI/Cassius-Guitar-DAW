#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>

// Original spring-style effect. Short delayed allpasses disperse each return
// through four separate feedback rings; no IR or physical-tank emulation.
class SpringReverb {
    struct Line {
        std::vector<float> data; size_t cursor = 0;
        void prepare(int length) { data.assign(static_cast<size_t>(juce::jmax(1,length)),0); cursor=0; }
        float read() const { return data[cursor]; }
        void write(float x) { data[cursor]=x; if(++cursor==data.size())cursor=0; }
        float disperse(float input,float coefficient) { const auto output=read()-coefficient*input; write(input+coefficient*output); return output; }
    };
public:
    struct Settings { float decay,tone,predelay,drip,mix; };
    void prepare(const juce::dsp::ProcessSpec& spec,Settings s) {
        rate=spec.sampleRate; filtered.fill(0); dc.fill(0);
        constexpr double loop[]{.029,.037,.043,.053},shortDelay[]{.0007,.0011,.0019,.0029,.0037,.0043},spread[]{1.,.93,1.17,1.07};
        for(size_t i=0;i<4;++i) {
            rings[i].prepare(juce::roundToInt(rate*loop[i])); loopSeconds[i]=rings[i].data.size()/rate;
            for(size_t j=0;j<6;++j) { dispersion[i][j].prepare(juce::roundToInt(rate*shortDelay[j]*spread[i])); loopSeconds[i]+=dispersion[i][j].data.size()/rate; }
            feedback[i].reset(rate,.03);
        }
        pre.setMaximumDelayInSamples(static_cast<int>(std::ceil(rate*.101))+2); pre.prepare({rate,spec.maximumBlockSize,2}); pre.reset();
        for(auto* c:{&damping,&preTime,&drip,&blend})c->reset(rate,.03);
        dcCoefficient=1-std::exp(-juce::MathConstants<float>::twoPi*20/static_cast<float>(rate));
        configure(s); for(auto* c:{&damping,&preTime,&drip,&blend})c->setCurrentAndTargetValue(c->getTargetValue()); for(auto& f:feedback)f.setCurrentAndTargetValue(f.getTargetValue());
    }
    void configure(Settings s) {
        for(size_t i=0;i<4;++i)feedback[i].setTargetValue(static_cast<float>(std::pow(10.,-3*loopSeconds[i]/juce::jlimit(.3f,6.f,s.decay))));
        damping.setTargetValue(1-std::exp(-juce::MathConstants<float>::twoPi*(1500+juce::jlimit(0.f,100.f,s.tone)*75)/static_cast<float>(rate)));
        preTime.setTargetValue(juce::jlimit(0.f,100.f,s.predelay)*static_cast<float>(rate)/1000);
        drip.setTargetValue(.15f+juce::jlimit(0.f,100.f,s.drip)*.0065f); blend.setTargetValue(juce::jlimit(0.f,100.f,s.mix)/100);
    }
    void process(juce::AudioBuffer<float>& audio) {
        juce::ScopedNoDenormals guard; const int channels=audio.getNumChannels();
        for(int sample=0;sample<audio.getNumSamples();++sample) {
            const float dry[]{audio.getSample(0,sample),audio.getSample(channels>1?1:0,sample)};
            float input[2]; const auto time=preTime.getNextValue(),colour=damping.getNextValue(),dispersionAmount=drip.getNextValue();
            for(size_t ch=0;ch<2;++ch) { const auto safe=std::isfinite(dry[ch])?dry[ch]:0.f; dc[ch]+=dcCoefficient*(safe-dc[ch]); pre.pushSample(static_cast<int>(ch),safe-dc[ch]); input[ch]=pre.popSample(static_cast<int>(ch),time); }
            const float injection[]{input[0],.7f*input[0]+.3f*input[1],input[1],.7f*input[1]+.3f*input[0]}; std::array<float,4> taps{};
            for(size_t i=0;i<4;++i) {
                taps[i]=rings[i].read(); filtered[i]+=colour*(taps[i]-filtered[i]); auto returning=filtered[i];
                for(auto& section:dispersion[i])returning=section.disperse(returning,dispersionAmount);
                const auto next=.35f*injection[i]+returning*feedback[i].getNextValue(); rings[i].write(std::isfinite(next)?juce::jlimit(-8.f,8.f,next):0.f);
            }
            const float wet[]{.75f*(taps[0]+taps[1]),.75f*(taps[2]+taps[3])}; const auto mix=blend.getNextValue();
            // A zero blend leaves both dry channels exactly at their original timing.
            if(mix>0)for(int ch=0;ch<channels;++ch)audio.setSample(ch,sample,dry[ch]*(1-mix)+wet[ch]*mix);
        }
    }
private:
    std::array<Line,4> rings; std::array<std::array<Line,6>,4> dispersion;
    std::array<float,4> filtered{}; std::array<float,2> dc{}; std::array<double,4> loopSeconds{};
    std::array<juce::SmoothedValue<float>,4> feedback; juce::SmoothedValue<float> damping,preTime,drip,blend;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear> pre;
    double rate=48000; float dcCoefficient=0;
};
