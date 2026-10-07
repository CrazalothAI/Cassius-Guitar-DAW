#include "../Source/PluginProcessor.h"
#include "../Source/dsp/PlateReverb.h"
#include <iostream>
namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void ready(AmpSuiteAudioProcessor& p){for(int i=0;i<1000;++i){auto s=p.status();if(!static_cast<bool>(s["rigLoading"])){require(!s["message"].toString().startsWith("Load failed:"),"Plate graph must prepare");return;}juce::Thread::sleep(5);}require(false,"Plate graph timed out");}
juce::ValueTree tree(AmpSuiteAudioProcessor& p){return juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString()));}
juce::var object(std::initializer_list<std::pair<juce::Identifier,juce::var>> fields){auto p=std::make_unique<juce::DynamicObject>();for(const auto& f:fields)p->setProperty(f.first,f.second);return juce::var(p.release());}
void set(AmpSuiteAudioProcessor& p,const char* id,float x){auto* v=p.apvts.getParameter(id);v->setValueNotifyingHost(v->convertTo0to1(x));}
std::vector<float> impulse(double rate,int block,PlateReverb::Settings settings){
    PlateReverb plate;plate.prepare({rate,static_cast<juce::uint32>(block),2},settings);const int frames=static_cast<int>(rate*3);std::vector<float> result(static_cast<size_t>(frames)*2);juce::AudioBuffer<float> buffer(2,block);
    for(int offset=0;offset<frames;offset+=block){const auto n=juce::jmin(block,frames-offset);juce::AudioBuffer<float> audio(buffer.getArrayOfWritePointers(),2,n);audio.clear();if(offset==0)audio.setSample(0,0,.5f);plate.process(audio);for(int i=0;i<n;++i)for(int ch=0;ch<2;++ch){const auto x=audio.getSample(ch,i);require(std::isfinite(x)&&std::abs(x)<1,"Plate impulses must remain finite and bounded");result[static_cast<size_t>(offset+i)*2+static_cast<size_t>(ch)]=x;}}
    return result;
}
double lateEnergy(const std::vector<float>& audio,double rate){double sum=0;for(size_t i=static_cast<size_t>(rate)*2;i<audio.size();++i)sum+=audio[i]*audio[i];return sum;}
int onset(const std::vector<float>& audio){for(size_t i=0;i<audio.size();i+=2)if(std::abs(audio[i])+std::abs(audio[i+1])>1.e-6)return static_cast<int>(i/2);return -1;}
}
void runPlateChecks(){
    for(double rate:{44100.,48000.,96000.}){
        const auto a=impulse(rate,127,{2.2f,55,0,100,100}),b=impulse(rate,512,{2.2f,55,0,100,100}),delayed=impulse(rate,128,{2.2f,55,100,100,100});
        double stereo=0,energy=0;for(size_t i=0;i<a.size();++i){require(std::abs(a[i]-b[i])<1.e-6,"Plate must be independent of callback partitioning");energy+=a[i]*a[i];if(i%2==0)stereo+=std::abs(a[i]-a[i+1]);}
        require(energy>.0001&&stereo>1,"Plate tank must produce an audible decorrelated stereo tail");
        require(onset(a)>static_cast<int>(rate*.02)&&std::abs(onset(delayed)-onset(a)-juce::roundToInt(rate*.1))<=1,"Pre-delay must move only wet onset by the requested time");
        const auto shortTail=impulse(rate,128,{.4f,55,0,100,100}),longTail=impulse(rate,128,{6,55,0,100,100});
        require(lateEnergy(longTail,rate)>lateEnergy(shortTail,rate)*20,"Longer decay must extend measured tail energy");
    }
    const auto mono=impulse(48000,128,{2.2f,55,0,0,100});for(size_t i=0;i<mono.size();i+=2)require(mono[i]==mono[i+1],"Zero width must mono only the wet field");
    const auto dark=impulse(48000,128,{2.2f,0,0,100,100}),bright=impulse(48000,128,{2.2f,100,0,100,100});double tone=0;for(size_t i=0;i<dark.size();++i)tone+=std::abs(dark[i]-bright[i]);require(tone>1,"Tone must change the decaying signal");
    PlateReverb dry;dry.prepare({48000,128,2},{8,100,150,100,0});juce::AudioBuffer<float> audio(2,128);audio.clear();audio.setSample(0,4,.123f);audio.setSample(1,7,-.231f);dry.process(audio);require(audio.getSample(0,4)==.123f&&audio.getSample(1,7)==-.231f,"Zero blend must preserve exact dry stereo timing");
    AmpSuiteAudioProcessor p(false);p.prepareToPlay(48000,128);require(p.boardCommand("convert",{}).isEmpty(),"Plate fixture must convert");ready(p);p.storeScene(0,"Pre-plate rig");auto old=tree(p);
    for(int i=old.getNumChildren();--i>=0;)if(old.getChild(i)["id"].toString().startsWith("BOARD_PLATE_"))old.removeChild(i,nullptr);
    auto bank=old.getChildWithName("SCENES");auto scenes=juce::JSON::parse(bank["json"].toString());for(auto& slot:*scenes["slots"].getArray())if(slot.isObject())for(const auto& d:BoardParams::definitions())if(d.id.startsWith("BOARD_PLATE_"))slot["parameters"].getDynamicObject()->removeProperty(d.id);bank.setProperty("json",juce::JSON::toString(scenes),nullptr);
    require(p.applyRig(object({{"schema",3},{"state",old.toXmlString()}})).isEmpty(),"Older rigs/scenes must default absent plate controls");ready(p);require(p.recallScene(0).isEmpty(),"Old scenes must still recall");ready(p);
    const auto parameters=p.getParameters();int lastDist=-1,firstPlate=-1;for(int i=0;i<parameters.size();++i)if(auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[i])){if(parameter->paramID.startsWith("BOARD_DISTORTION_"))lastDist=i;if(firstPlate<0&&parameter->paramID.startsWith("BOARD_PLATE_"))firstPlate=i;}require(firstPlate==lastDist+1,"Plate controls must append after distortion without shifting host IDs");
    require(p.boardCommand("add",object({{"type","plate"},{"lane","post"}})).isEmpty(),"Plate must add after cabinet");ready(p);const auto id=tree(p).getChildWithName("PEDALBOARD").getChildWithProperty("type","plate")["id"].toString();set(p,"BOARD_PLATE_0_PLATE_DECAY",3);
    require(p.boardCommand("duplicate",object({{"id",id}})).isEmpty(),"Plate must duplicate independently");ready(p);set(p,"BOARD_PLATE_1_PLATE_DECAY",6);require(p.apvts.getRawParameterValue("BOARD_PLATE_0_PLATE_DECAY")->load()==3,"Second plate must not edit first decay");
    p.storeScene(1,"Two plates");set(p,"BOARD_PLATE_1_PLATE_DECAY",1);require(p.recallScene(1).isEmpty(),"Plate scene must recall");ready(p);require(p.apvts.getRawParameterValue("BOARD_PLATE_1_PLATE_DECAY")->load()==6,"Scene must restore independent plate controls");
    const auto intact=p.getRig()["state"].toString();auto invalid=tree(p);invalid.removeChild(invalid.getChildWithProperty("id","BOARD_PLATE_0_PLATE_TONE"),nullptr);require(p.applyRig(object({{"schema",3},{"state",invalid.toXmlString()}})).isNotEmpty()&&p.getRig()["state"].toString()==intact,"A present plate requires complete state");
    juce::MemoryBlock session;p.getStateInformation(session);AmpSuiteAudioProcessor restored(false);restored.prepareToPlay(96000,257);restored.setStateInformation(session.getData(),static_cast<int>(session.getSize()));ready(restored);require(restored.apvts.getRawParameterValue("BOARD_PLATE_1_PLATE_DECAY")->load()==6&&PedalboardState::equal(tree(p),tree(restored)),"Native sessions must restore plate controls at another rate");
    const auto base=juce::File::getSpecialLocation(juce::File::tempDirectory),root=base.getNonexistentChildFile("Cassian-plate-pack-"+juce::Uuid().toString(),"",false);struct Cleanup{juce::File root,base;~Cleanup(){if(root.isAChildOf(base))root.deleteRecursively();}}cleanup{root,base};require(root.createDirectory().wasOk(),"Plate pack test directory must create");const auto pack=root.getChildFile("Plate.zip");require(p.exportRigPack(pack).isEmpty(),"Plate pack must export without captured assets");AmpSuiteAudioProcessor portable(true,root.getChildFile("Library"));portable.prepareToPlay(44100,128);require(portable.importRigPack(pack).isEmpty(),"Plate pack must import");require(portable.loadRig(portable.getLibrary()["rigs"][0]["id"].toString()).isEmpty(),"Plate pack must load");ready(portable);require(portable.apvts.getRawParameterValue("BOARD_PLATE_1_PLATE_DECAY")->load()==6,"Portable pack must preserve independent decay");
    std::cout<<"Plate onset, stereo, decay, migration and recall checks passed\n";
}
