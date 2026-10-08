#include "../Source/PluginProcessor.h"
#include <iostream>
namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void ready(AmpSuiteAudioProcessor& p){for(int i=0;i<1000;++i){auto s=p.status();if(!static_cast<bool>(s["rigLoading"])){require(!s["message"].toString().startsWith("Load failed:"),"Scene copy fixture must prepare");return;}juce::Thread::sleep(5);}require(false,"Scene copy fixture timed out");}
void set(AmpSuiteAudioProcessor& p,const char* id,float x){auto* parameter=p.apvts.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(x));}
juce::var bank(AmpSuiteAudioProcessor& p){return juce::JSON::parse(p.scenes.save()["json"].toString());}
juce::ValueTree state(AmpSuiteAudioProcessor& p){return juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString()));}
void sameSound(const juce::ValueTree& a,const juce::ValueTree& b){BoardParams::each([&](const auto& d){require(a.getChildWithProperty("id",d.id)["value"]==b.getChildWithProperty("id",d.id)["value"],"Copy must preserve every live parameter");});require(PedalboardState::equal(a,b),"Copy must preserve live board identities/topology");}
void sameSnapshot(const juce::var& a,const juce::var& b){require(juce::JSON::toString(a["parameters"])==juce::JSON::toString(b["parameters"])&&a["board"]==b["board"],"Copy must preserve exact saved tone and board");}
}
void runSceneCopyChecks(){
    AmpSuiteAudioProcessor p(false);p.prepareToPlay(48000,128);require(p.loadStartingRig("factory.velvet-lead").isEmpty(),"Scene copy fixture must load");ready(p);
    require(p.storeScene(0,"Lead").isEmpty(),"Source scene must store");set(p,"BOARD_DISTORTION_0_DIST_DRIVE",80);require(p.storeScene(1,"Heavy").isEmpty(),"Active scene must store");require(p.saveRig("Scene copying").isEmpty(),"Complete rig must save");
    set(p,"BOARD_DISTORTION_0_DIST_DRIVE",90);set(p,"AMP_MID",4);const auto before=state(p);const auto source=bank(p)["slots"][0].clone();
    require(p.copyScene(0,2,"  Lead variation  ").isEmpty(),"Saved scene must copy with trimmed name");sameSound(before,state(p));sameSnapshot(source,bank(p)["slots"][2]);
    require(bank(p)["slots"][2]["name"].toString()=="Lead variation"&&bank(p)["slots"][0]["name"].toString()=="Lead","Copy must preserve source name and set destination name");
    require(static_cast<int>(p.scenes.status(p.apvts)["active"])==1&&static_cast<bool>(p.scenes.status(p.apvts)["edited"])&&static_cast<bool>(p.status()["activeRigEdited"]),"Copy must retain active/edited scene and mark saved rig edited");
    const auto intact=p.scenes.save()["json"].toString();
    require(p.copyScene(-1,2,"No").isNotEmpty()&&p.copyScene(0,4,"No").isNotEmpty()&&p.copyScene(0,0,"No").isNotEmpty()&&p.copyScene(3,2,"No").isNotEmpty()&&p.copyScene(0,2," ").isNotEmpty()&&p.copyScene(0,2,juce::String::repeatedString("x",49)).isNotEmpty()&&p.scenes.save()["json"].toString()==intact,"Invalid copies must preserve the complete bank");
    require(p.copyScene(0,1,"Replaced active").isEmpty(),"Copy must replace an occupied destination");sameSound(before,state(p));require(static_cast<int>(p.scenes.status(p.apvts)["active"])==-1,"Replacing active saved slot must detach live identity");
    require(p.storeScene(0,"Changed source").isEmpty(),"Source must allow subsequent replacement");sameSnapshot(source,bank(p)["slots"][2]);sameSnapshot(source,bank(p)["slots"][1]);
    const auto copied=p.scenes.save()["json"].toString();juce::MemoryBlock session;p.getStateInformation(session);AmpSuiteAudioProcessor restored(false);restored.prepareToPlay(44100,257);restored.setStateInformation(session.getData(),static_cast<int>(session.getSize()));ready(restored);
    require(restored.scenes.save()["json"].toString()==copied&&restored.recallScene(2).isEmpty(),"Copied scenes must survive native sessions");ready(restored);require(restored.apvts.getRawParameterValue("BOARD_DISTORTION_0_DIST_DRIVE")->load()==48,"Copy must recall saved source settings, not live edits");
    const auto base=juce::File::getSpecialLocation(juce::File::tempDirectory),root=base.getNonexistentChildFile("Cassian-scene-copy-"+juce::Uuid().toString(),"",false);struct Cleanup{juce::File root,base;~Cleanup(){if(root.isAChildOf(base))root.deleteRecursively();}}cleanup{root,base};require(root.createDirectory().wasOk(),"Copy pack folder must create");const auto pack=root.getChildFile("Scenes.zip");require(p.exportRigPack(pack).isEmpty(),"Copied scene pack must export");AmpSuiteAudioProcessor imported(true,root.getChildFile("Library"));imported.prepareToPlay(48000,128);require(imported.importRigPack(pack).isEmpty()&&imported.loadRig(imported.getLibrary()["rigs"][0]["id"].toString()).isEmpty(),"Copied scene pack must import/load");ready(imported);require(imported.scenes.save()["json"].toString()==copied&&imported.recallScene(1).isEmpty(),"Portable copies must preserve saved tones and names");ready(imported);require(imported.apvts.getRawParameterValue("BOARD_DISTORTION_0_DIST_DRIVE")->load()==48,"Replaced destination must recall copied source tone");
    p.requestFile(true,root.getChildFile("Missing.nam"));require(p.copyScene(0,3,"Blocked").isNotEmpty()&&p.scenes.save()["json"].toString()==copied,"Pending/unresolved assets must block copies");
    std::cout<<"Scene copy isolation, saved/live distinction, active replacement, session and pack checks passed\n";
}
