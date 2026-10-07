#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>

// Original stereo plate-style network: four input allpasses per side and an
// eight-line orthogonal feedback tank. No IR or third-party reverb implementation.
class PlateReverb {
    struct Line {
        std::vector<float> data; size_t cursor = 0;
        void prepare(int length) { data.assign(static_cast<size_t>(juce::jmax(1,length)),0);cursor=0; }
        float read() const { return data[cursor]; }
        void write(float value) { data[cursor]=value;if(++cursor==data.size())cursor=0; }
        float diffuse(float input) { const auto output=read()-.65f*input;write(input+.65f*output);return output; }
    };
public:
    struct Settings { float decay, tone, predelay, width, mix; };
    void prepare(const juce::dsp::ProcessSpec& spec, Settings s) {
        rate=spec.sampleRate;filtered.fill(0);dc.fill(0);
        constexpr double seconds[]{.0297,.0371,.0411,.0437,.0473,.0531,.0593,.0617};
        for(size_t i=0;i<lines.size();++i){lines[i].prepare(juce::roundToInt(seconds[i]*rate));feedback[i].reset(rate,.03);}
        constexpr double diffusion[]{.0047,.0059,.0097,.0113};
        for(size_t side=0;side<2;++side)for(size_t i=0;i<4;++i)diffusers[side][i].prepare(juce::roundToInt(diffusion[i]*rate*(side==0?1.:1.127)));
        pre.setMaximumDelayInSamples(static_cast<int>(std::ceil(rate*.151))+2);pre.prepare({rate,spec.maximumBlockSize,2});pre.reset();
        for(auto* c:{&damping,&preTime,&stereoWidth,&blend})c->reset(rate,.03);
        dcCoefficient=1-std::exp(-juce::MathConstants<float>::twoPi*20/static_cast<float>(rate));
        configure(s);for(auto* c:{&damping,&preTime,&stereoWidth,&blend})c->setCurrentAndTargetValue(c->getTargetValue());for(auto& f:feedback)f.setCurrentAndTargetValue(f.getTargetValue());
    }
    void configure(Settings s) {
        const auto decay=juce::jlimit(.3f,8.f,s.decay);
        for(size_t i=0;i<lines.size();++i)feedback[i].setTargetValue(static_cast<float>(std::pow(10.,-3.*lines[i].data.size()/(rate*decay))));
        damping.setTargetValue(1-std::exp(-juce::MathConstants<float>::twoPi*(500+juce::jlimit(0.f,100.f,s.tone)*115)/static_cast<float>(rate)));
        preTime.setTargetValue(juce::jlimit(0.f,150.f,s.predelay)*static_cast<float>(rate)/1000);
        stereoWidth.setTargetValue(juce::jlimit(0.f,100.f,s.width)/100);blend.setTargetValue(juce::jlimit(0.f,100.f,s.mix)/100);
    }
    void process(juce::AudioBuffer<float>& audio) {
        juce::ScopedNoDenormals guard; const int channels=audio.getNumChannels();
        for(int sample=0;sample<audio.getNumSamples();++sample){
            const float original[]{audio.getSample(0,sample),audio.getSample(channels>1?1:0,sample)};
            float input[2];const auto t=preTime.getNextValue();
            for(int ch=0;ch<2;++ch){const auto safe=std::isfinite(original[ch])?original[ch]:0.f;dc[static_cast<size_t>(ch)]+=dcCoefficient*(safe-dc[static_cast<size_t>(ch)]);pre.pushSample(ch,safe-dc[static_cast<size_t>(ch)]);input[ch]=pre.popSample(ch,t);for(auto& diffuser:diffusers[static_cast<size_t>(ch)])input[ch]=diffuser.diffuse(input[ch]);}
            std::array<float,8> taps{},matrix{};const auto d=damping.getNextValue();
            for(size_t i=0;i<8;++i){taps[i]=lines[i].read();filtered[i]+=d*(taps[i]-filtered[i]);matrix[i]=filtered[i];}
            // Normalized Hadamard transform is energy preserving. Decay gains
            // stay below one; DC filtering and an emergency finite bound guard
            // pathological inputs without colouring ordinary guitar levels.
            for(size_t size=1;size<8;size*=2)for(size_t offset=0;offset<8;offset+=size*2)for(size_t i=0;i<size;++i){const auto a=matrix[offset+i],b=matrix[offset+i+size];matrix[offset+i]=a+b;matrix[offset+i+size]=a-b;}
            const float injection[]{input[0],input[1],-input[0],-input[1],input[0],-input[1],-input[0],input[1]};
            for(size_t i=0;i<8;++i){const auto value=injection[i]*.25f+matrix[i]*.35355339f*feedback[i].getNextValue();lines[i].write(std::isfinite(value)?juce::jlimit(-8.f,8.f,value):0.f);}
            const auto left=(taps[0]+taps[2]-taps[4]+taps[6])*.5f,right=(taps[1]-taps[3]+taps[5]+taps[7])*.5f;
            const auto middle=(left+right)*.5f,side=(left-right)*.5f*stereoWidth.getNextValue(),mix=blend.getNextValue();
            // Keep a zero blend exactly dry while the inaudible tank drains.
            if(mix>0){audio.setSample(0,sample,original[0]*(1-mix)+(middle+side)*mix);if(channels>1)audio.setSample(1,sample,original[1]*(1-mix)+(middle-side)*mix);}
        }
    }
private:
    std::array<Line,8> lines;
    std::array<std::array<Line,4>,2> diffusers;
    std::array<float,8> filtered{};std::array<float,2> dc{};
    std::array<juce::SmoothedValue<float>,8> feedback;
    juce::SmoothedValue<float> damping,preTime,stereoWidth,blend;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear> pre;
    double rate=48000;float dcCoefficient=0;
};
