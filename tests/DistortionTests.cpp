#include "../Source/PluginProcessor.h"
#include "../Source/dsp/DistortionPedal.h"
#include <iostream>
namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void ready(AmpSuiteAudioProcessor& p) { for(int i=0;i<1000;++i){auto s=p.status();if(!static_cast<bool>(s["rigLoading"])){require(!s["message"].toString().startsWith("Load failed:"),"Distortion graph must prepare");return;}juce::Thread::sleep(5);}require(false,"Distortion graph timed out"); }
juce::ValueTree tree(AmpSuiteAudioProcessor& p) {return juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString()));}
juce::var object(std::initializer_list<std::pair<juce::Identifier,juce::var>> fields){auto p=std::make_unique<juce::DynamicObject>();for(const auto& f:fields)p->setProperty(f.first,f.second);return juce::var(p.release());}
void set(AmpSuiteAudioProcessor& p,const char* id,float x){auto* v=p.apvts.getParameter(id);v->setValueNotifyingHost(v->convertTo0to1(x));}
std::vector<float> render(double rate,int block,int mode,float drive=45,float tight=80){
    DistortionPedal pedal;pedal.prepare(rate,block,{static_cast<float>(mode),drive,50,tight,100});
    const int frames=static_cast<int>(rate);std::vector<float> result(static_cast<size_t>(frames));std::vector<float> input(static_cast<size_t>(block));
    for(int offset=0;offset<frames;offset+=block){const int n=juce::jmin(block,frames-offset);for(int i=0;i<n;++i)input[static_cast<size_t>(i)]=.12f*static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*220*(offset+i)/rate));pedal.process(input.data(),n);for(int i=0;i<n;++i){const auto x=input[static_cast<size_t>(i)];require(std::isfinite(x)&&std::abs(x)<.8f,"Distortion must stay finite and bounded");result[static_cast<size_t>(offset+i)]=x;}}
    return result;
}
}
void runDistortionChecks(){
    for(double rate:{44100.,48000.,96000.})for(int mode=0;mode<3;++mode){
        const auto a=render(rate,127,mode),b=render(rate,512,mode);double mean=0,energy=0;
        for(size_t i=0;i<a.size();++i){require(std::abs(a[i]-b[i])<1.e-5f,"Distortion must be independent of callback partitioning");if(i>a.size()/2){mean+=a[i];energy+=a[i]*a[i];}}
        require(std::abs(mean/(a.size()/2))<.005&&energy/(a.size()/2)>.01,"Clipping must produce audible saturation while removing DC");
    }
    const auto hard=render(48000,128,0),asymmetric=render(48000,128,1),fuzz=render(48000,128,2),low=render(48000,128,0,0),tight=render(48000,128,0,45,250);
    double voices=0,drives=0,cuts=0;for(size_t i=0;i<hard.size();++i){voices+=std::abs(hard[i]-asymmetric[i])+std::abs(hard[i]-fuzz[i]);drives+=std::abs(hard[i]-low[i]);cuts+=std::abs(hard[i]-tight[i]);}
    require(voices>100&&drives>100&&cuts>100,"Modes, drive and bass control must change the rendered audio");
    DistortionPedal bypass;bypass.prepare(48000,128,{2,100,100,250,0});float dry[128]{};dry[4]=.123f;bypass.process(dry,128);require(dry[4]==.123f,"Zero blend must preserve exact dry audio");
    require(bypass.latencySamples()>0&&bypass.latencySamples()<8,"Oversampling latency must stay below eight base-rate samples");
    AmpSuiteAudioProcessor p(false);p.prepareToPlay(48000,128);require(p.boardCommand("convert",{}).isEmpty(),"Distortion fixture must convert");ready(p);
    // The original parameter prefix must keep its host indices. New controls
    // follow all wah controls rather than shifting existing automation slots.
    const auto parameters=p.getParameters();int lastWah=-1,firstDist=-1;for(int i=0;i<parameters.size();++i)if(auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[i])){if(parameter->paramID.startsWith("BOARD_WAH_"))lastWah=i;if(firstDist<0&&parameter->paramID.startsWith("BOARD_DISTORTION_"))firstDist=i;}
    require(lastWah>=0&&firstDist==lastWah+1,"Distortion automation must append after all existing parameters");
    p.storeScene(0,"Pre-distortion rig");auto old=tree(p);for(int i=old.getNumChildren();--i>=0;)if(old.getChild(i)["id"].toString().startsWith("BOARD_DISTORTION_"))old.removeChild(i,nullptr);
    auto bank=old.getChildWithName("SCENES");auto scene=juce::JSON::parse(bank["json"].toString());for(auto& slot:*scene["slots"].getArray())if(slot.isObject())for(const auto& d:BoardParams::definitions())if(d.id.startsWith("BOARD_DISTORTION_"))slot["parameters"].getDynamicObject()->removeProperty(d.id);bank.setProperty("json",juce::JSON::toString(scene),nullptr);
    require(p.applyRig(object({{"schema",3},{"state",old.toXmlString()}})).isEmpty(),"Old rigs/scenes must default absent distortion controls");ready(p);require(p.recallScene(0).isEmpty(),"Old scenes must remain recallable");ready(p);
    require(p.boardCommand("add",object({{"type","distortion"},{"lane","pre"}})).isEmpty(),"Distortion must add before amp");ready(p);
    const auto id=tree(p).getChildWithName("PEDALBOARD").getChildWithProperty("type","distortion")["id"].toString();set(p,"BOARD_DISTORTION_0_DIST_DRIVE",35);
    require(p.boardCommand("duplicate",object({{"id",id}})).isEmpty(),"Distortion must duplicate");ready(p);set(p,"BOARD_DISTORTION_1_DIST_MODE",2);set(p,"BOARD_DISTORTION_1_DIST_DRIVE",80);
    require(p.apvts.getRawParameterValue("BOARD_DISTORTION_0_DIST_DRIVE")->load()==35,"Duplicate distortion controls must stay independent");
    const auto intact=p.getRig()["state"].toString();require(p.boardCommand("lane",object({{"id",id},{"lane","post"}})).isNotEmpty()&&p.getRig()["state"].toString()==intact,"Mono distortion must reject a stereo post lane atomically");
    auto incomplete=tree(p);incomplete.removeChild(incomplete.getChildWithProperty("id","BOARD_DISTORTION_1_DIST_MODE"),nullptr);require(p.applyRig(object({{"schema",3},{"state",incomplete.toXmlString()}})).isNotEmpty()&&p.getRig()["state"].toString()==intact,"Present distortion blocks require complete state");
    p.storeScene(1,"Heavy pedals");set(p,"BOARD_DISTORTION_1_DIST_DRIVE",2);require(p.recallScene(1).isEmpty(),"Distortion scene must recall");ready(p);require(p.apvts.getRawParameterValue("BOARD_DISTORTION_1_DIST_DRIVE")->load()==80,"Scene must restore the second distortion");
    juce::MemoryBlock session;p.getStateInformation(session);AmpSuiteAudioProcessor restored(false);restored.prepareToPlay(96000,257);restored.setStateInformation(session.getData(),static_cast<int>(session.getSize()));ready(restored);require(restored.apvts.getRawParameterValue("BOARD_DISTORTION_1_DIST_MODE")->load()==2&&PedalboardState::equal(tree(p),tree(restored)),"Distortion must survive native session recall at another rate");
    juce::TemporaryFile pack(".zip");require(p.exportRigPack(pack.getFile()).isEmpty(),"Original distortion must export without a capture dependency");const auto base=juce::File::getSpecialLocation(juce::File::tempDirectory);const auto root=base.getNonexistentChildFile("Cassian-distortion-pack-"+juce::Uuid().toString(),"",false);struct Cleanup {juce::File root,base;~Cleanup(){if(root.isAChildOf(base))root.deleteRecursively();}} cleanup{root,base};
    AmpSuiteAudioProcessor portable(true,root);portable.prepareToPlay(48000,128);require(portable.importRigPack(pack.getFile()).isEmpty(),"Distortion pack must import");require(portable.loadRig(portable.getLibrary()["rigs"][0]["id"].toString()).isEmpty(),"Distortion pack must load");ready(portable);require(portable.apvts.getRawParameterValue("BOARD_DISTORTION_1_DIST_MODE")->load()==2,"Portable pack must retain the selected clipping voice");
    std::cout<<"Distortion sound, appended automation, migration and recall checks passed\n";
}
