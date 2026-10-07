#include "../Source/PluginProcessor.h"
#include "../Source/dsp/SerialPedalboard.h"
#include <iostream>

void runExpansionBoardChecks() {
    AmpSuiteAudioProcessor p(false);
    const auto set=[&](const juce::String& id,float x){auto* parameter=p.apvts.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(x));};
    for(int slot=0;slot<2;++slot) {
        set(BoardParams::parameter(10,slot,"DIST_MIX"),40);
        set(BoardParams::parameter(11,slot,"PLATE_MIX"),20);set(BoardParams::parameter(11,slot,"PLATE_DECAY"),6);
        set(BoardParams::parameter(12,slot,"SPRING_MIX"),20);set(BoardParams::parameter(12,slot,"SPRING_DECAY"),6);
    }
    auto state=juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString()));state.removeChild(state.getChildWithName("PEDALBOARD"),nullptr);
    auto board=PedalboardState::emptySerial();
    for(int kind:{10,11,12})for(int slot=0;slot<2;++slot) {
        juce::ValueTree row("BLOCK");row.setProperty("id",BoardParams::types[kind]+"."+juce::String(slot),nullptr);row.setProperty("type",BoardParams::types[kind],nullptr);
        row.setProperty("automationSlot",slot,nullptr);row.setProperty("lane",kind==10?"pre":"post",nullptr);row.setProperty("deleted",0,nullptr);board.addChild(row,-1,nullptr);
    }
    state.addChild(board,-1,nullptr);
    for(double rate:{48000.,96000.})for(int size:{128,256,512}) {
        SerialPedalboard graph(state,p.apvts,{rate,static_cast<juce::uint32>(size),2});juce::AudioBuffer<float> audio(2,size);NamWrapper* captures[]{nullptr,nullptr};
        double total=0,maximum=0;float peak=0;
        for(int block=0;block<384;++block) {
            audio.clear();for(int i=0;i<size;++i)audio.setSample(0,i,.06f*std::sin(static_cast<float>((block*size+i)*440*juce::MathConstants<double>::twoPi/rate)));
            const auto start=juce::Time::getMillisecondCounterHiRes();graph.process(audio,true,120,captures);audio.copyFrom(1,0,audio,0,0,size);graph.process(audio,false,120,captures);
            const auto elapsed=juce::Time::getMillisecondCounterHiRes()-start;if(block>=64){total+=elapsed;maximum=juce::jmax(maximum,elapsed);}
            for(int ch=0;ch<2;++ch)for(int i=0;i<size;++i){const auto x=audio.getSample(ch,i);if(!std::isfinite(x)||std::abs(x)>=1)throw std::runtime_error("Demanding expansion board must render finite audio with headroom");peak=juce::jmax(peak,std::abs(x));}
        }
        // Report local processing cost; OS scheduling and actual interface/host
        // work are outside this fixture, so timing is not a flaky pass/fail gate.
        std::cout<<"Expansion board "<<rate<<" Hz / "<<size<<": average "<<total/320<<" ms; max "<<maximum<<" ms; callback budget "<<size*1000/rate<<" ms; peak "<<peak<<'\n';
    }
}
