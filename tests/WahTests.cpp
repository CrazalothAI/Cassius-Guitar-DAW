#include "../Source/PluginProcessor.h"
#include "../Source/dsp/WahPedal.h"
#include <iostream>

namespace {
void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
void settle(AmpSuiteAudioProcessor& p) {
    for (int i=0;i<1000;++i) { if (!static_cast<bool>(p.status()["rigLoading"])) return; juce::Thread::sleep(5); }
    require(false,"Wah graph preparation timed out");
}
juce::var object(std::initializer_list<std::pair<juce::Identifier,juce::var>> fields) {
    auto value=std::make_unique<juce::DynamicObject>(); for (const auto& field:fields) value->setProperty(field.first,field.second); return juce::var(value.release());
}
juce::ValueTree state(AmpSuiteAudioProcessor& p) { return juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString())); }
void set(AmpSuiteAudioProcessor& p,const char* id,float value) { auto* param=p.apvts.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(value)); }
double response(double rate,float position,double frequency) {
    WahPedal wah; wah.prepare({rate,128,1},{0,position,0,80,100});
    juce::AudioBuffer<float> audio(1,128); double energy=0; int samples=0;
    for (int block=0;block<200;++block) {
        for(int i=0;i<128;++i) audio.setSample(0,i,.1f*static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*frequency*(block*128+i)/rate)));
        wah.process(audio);
        if(block>=100) for(int i=0;i<128;++i){const auto x=audio.getSample(0,i);require(std::isfinite(x)&&std::abs(x)<.3f,"Wah response must be finite with restrained resonant gain");energy+=x*x;++samples;}
    }
    return std::sqrt(energy/samples);
}
std::vector<float> envelopeRender(int blockSize,float level,float sensitivity=0) {
    WahPedal wah; wah.prepare({48000,static_cast<juce::uint32>(blockSize),2},{1,50,sensitivity,65,100});
    std::vector<float> result(12000); juce::AudioBuffer<float> buffer(2,blockSize);
    for(int offset=0;offset<12000;offset+=blockSize){const auto n=juce::jmin(blockSize,12000-offset);juce::AudioBuffer<float> audio(buffer.getArrayOfWritePointers(),2,n);audio.clear();
        for(int i=0;i<n;++i) audio.setSample(0,i,level*static_cast<float>(std::sin((offset+i)*.07)+.3*std::sin((offset+i)*.19)));
        wah.process(audio);
        for(int i=0;i<n;++i){result[static_cast<size_t>(offset+i)]=audio.getSample(0,i)/level;require(audio.getSample(1,i)==0,"Stereo wah must not leak signal into a silent channel");}
    }
    return result;
}
}

void runWahChecks() {
    for(double rate:{44100.,48000.,96000.}) {
        require(response(rate,0,350)>response(rate,100,350)*4,"Heel position must emphasize lower frequencies");
        require(response(rate,100,2600)>response(rate,0,2600)*4,"Toe position must emphasize higher frequencies");
    }
    const auto small=envelopeRender(127,.02f), large=envelopeRender(127,.25f), sensitive=envelopeRender(127,.02f,24), partitioned=envelopeRender(512,.02f);
    double dynamic=0,sense=0;
    for(size_t i=0;i<small.size();++i){require(std::isfinite(small[i])&&std::isfinite(large[i])&&std::isfinite(sensitive[i]),"Envelope extremes must stay finite");dynamic+=std::abs(small[i]-large[i]);sense+=std::abs(small[i]-sensitive[i]);require(std::abs(small[i]-partitioned[i])<1.e-5f,"Envelope processing must be independent of block partitioning");}
    require(dynamic>100 && sense>100,"Picking level and sensitivity must change the audible sweep, beyond mere gain");
    {
        WahPedal dry;dry.prepare({48000,128,2},{0,50,24,100,0});juce::AudioBuffer<float> audio(2,128);audio.clear();audio.setSample(0,4,.123f);dry.process(audio);
        require(audio.getSample(0,4)==.123f&&audio.getSample(1,4)==0,"Zero blend must preserve exact dry stereo audio");
    }
    AmpSuiteAudioProcessor p(false);p.prepareToPlay(48000,128);
    require(p.boardCommand("convert",{}).isEmpty(),"Wah test must convert");settle(p);
    require(p.storeScene(0,"Pre-0.4 tone").isEmpty(),"Old scene fixture must store");
    auto old=state(p);
    for(int i=old.getNumChildren();--i>=0;)if(old.getChild(i)["id"].toString().startsWith("BOARD_WAH_"))old.removeChild(i,nullptr);
    auto scenes=old.getChildWithName("SCENES");auto json=juce::JSON::parse(scenes["json"].toString());
    for(auto& slot:*json["slots"].getArray())if(slot.isObject())for(const auto& definition:BoardParams::definitions())if(definition.id.startsWith("BOARD_WAH_"))slot["parameters"].getDynamicObject()->removeProperty(definition.id);
    scenes.setProperty("json",juce::JSON::toString(json),nullptr);
    require(p.applyRig(object({{"schema",3},{"state",old.toXmlString()}})).isEmpty(),"Pre-0.4 schema-3 rigs and scenes must default only the new controls");settle(p);
    require(p.recallScene(0).isEmpty(),"Migrated old scene must remain recallable");settle(p);
    require(p.boardCommand("add",object({{"type","wah"},{"lane","pre"}})).isEmpty(),"Wah must add before amp");settle(p);
    require(p.apvts.getRawParameterValue("BOARD_WAH_0_WAH_POSITION")->load()==50&&p.apvts.getRawParameterValue("BOARD_WAH_0_WAH_MIX")->load()==100,"Wah addition must use musical defaults");
    auto graph=state(p).getChildWithName("PEDALBOARD");const auto first=graph.getChildWithProperty("type","wah")["id"].toString();
    set(p,"BOARD_WAH_0_WAH_POSITION",20);
    require(p.boardCommand("duplicate",object({{"id",first}})).isEmpty(),"Wah must duplicate independently");settle(p);
    set(p,"BOARD_WAH_1_WAH_POSITION",85);
    require(p.apvts.getRawParameterValue("BOARD_WAH_0_WAH_POSITION")->load()==20,"Duplicate wah editing must preserve the original");
    require(p.storeScene(1,"Two wahs").isEmpty(),"Wah scene must store");
    set(p,"BOARD_WAH_1_WAH_POSITION",5);require(p.recallScene(1).isEmpty(),"Wah scene must recall");settle(p);
    require(p.apvts.getRawParameterValue("BOARD_WAH_1_WAH_POSITION")->load()==85,"Scene must restore both independent positions");
    auto incomplete=state(p);incomplete.removeChild(incomplete.getChildWithProperty("id","BOARD_WAH_1_WAH_POSITION"),nullptr);
    const auto intact=p.getRig()["state"].toString();
    require(p.applyRig(object({{"schema",3},{"state",incomplete.toXmlString()}})).isNotEmpty()&&p.getRig()["state"].toString()==intact,"Incomplete wah rig must reject atomically");
    auto invalidScene=state(p);auto bank=invalidScene.getChildWithName("SCENES");json=juce::JSON::parse(bank["json"].toString());json["slots"][1]["parameters"].getDynamicObject()->removeProperty("BOARD_WAH_0_WAH_MODE");bank.setProperty("json",juce::JSON::toString(json),nullptr);
    require(p.applyRig(object({{"schema",3},{"state",invalidScene.toXmlString()}})).isNotEmpty()&&p.getRig()["state"].toString()==intact,"Incomplete wah scene must reject without partial recall");
    juce::MemoryBlock session;p.getStateInformation(session);AmpSuiteAudioProcessor restored(false);restored.prepareToPlay(96000,257);restored.setStateInformation(session.getData(),static_cast<int>(session.getSize()));settle(restored);
    require(restored.apvts.getRawParameterValue("BOARD_WAH_1_WAH_POSITION")->load()==85&&PedalboardState::equal(state(p),state(restored)),"Wah state must survive native DAW recall at a different rate");
    set(p,"BOARD_WAH_0_ON",0);set(p,"BOARD_WAH_1_ON",0);
    auto bypassState=state(p);auto bypassBoard=bypassState.getChildWithName("PEDALBOARD");
    for(int i=bypassBoard.getNumChildren();--i>=0;)if(bypassBoard.getChild(i)["type"].toString()!="wah")bypassBoard.removeChild(i,nullptr);
    SerialPedalboard bypass(bypassState,p.apvts,{48000,128,2});NamWrapper* captures[]{nullptr,nullptr};
    juce::AudioBuffer<float> audio(1,128);audio.clear();audio.setSample(0,4,.123f);bypass.process(audio,true,120,captures);
    require(audio.getSample(0,4)==.123f,"Settled wah bypass must be exact dry audio");
    const auto base=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=base.getNonexistentChildFile("Cassian-wah-pack-"+juce::Uuid().toString(),"",false);
    struct Cleanup {juce::File root,base;~Cleanup(){if(root.isAChildOf(base))root.deleteRecursively();}} cleanup{root,base};
    require(root.createDirectory().wasOk(),"Wah pack test directory must create");
    const auto pack=root.getChildFile("Wah.zip");require(p.exportRigPack(pack).isEmpty(),"Wah-containing portable rig must export");
    AmpSuiteAudioProcessor portable(true,root.getChildFile("Library"));portable.prepareToPlay(44100,128);
    require(portable.importRigPack(pack).isEmpty(),"Wah portable pack must import");
    require(portable.loadRig(portable.getLibrary()["rigs"][0]["id"].toString()).isEmpty(),"Imported wah rig must load");settle(portable);
    require(PedalboardState::equal(state(p),state(portable))&&portable.apvts.getRawParameterValue("BOARD_WAH_1_WAH_POSITION")->load()==85,"Wah pack must preserve identities and independent controls");
    std::cout<<"Wah frequency response, linked envelope, independent instances, migrations and recall passed\n";
}
