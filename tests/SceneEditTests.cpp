#include "../Source/PluginProcessor.h"
#include <iostream>
namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void ready(AmpSuiteAudioProcessor& p){for(int i=0;i<1000;++i){auto s=p.status();if(!static_cast<bool>(s["rigLoading"])){require(!s["message"].toString().startsWith("Load failed:"),"Scene edit fixture must prepare");return;}juce::Thread::sleep(5);}require(false,"Scene edit fixture timed out");}
void set(AmpSuiteAudioProcessor& p,const char* id,float x){auto* parameter=p.apvts.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(x));}
juce::var bank(AmpSuiteAudioProcessor& p){return juce::JSON::parse(p.scenes.save()["json"].toString());}
juce::ValueTree state(AmpSuiteAudioProcessor& p){return juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString()));}
void sameSound(const juce::ValueTree& a,const juce::ValueTree& b){BoardParams::each([&](const auto& d){require(a.getChildWithProperty("id",d.id)["value"]==b.getChildWithProperty("id",d.id)["value"],"Scene metadata must preserve every live parameter");});require(PedalboardState::equal(a,b),"Scene metadata must preserve live board identities/topology");}
}
void runSceneEditChecks(){
    AmpSuiteAudioProcessor p(false);p.prepareToPlay(48000,128);require(p.loadStartingRig("factory.velvet-lead").isEmpty(),"Scene edit fixture must load");ready(p);
    require(p.storeScene(0,"Lead").isEmpty(),"Scene must store");set(p,"BOARD_DISTORTION_0_DIST_DRIVE",85);require(p.storeScene(1,"Heavy").isEmpty(),"Independent scene must store");
    require(p.saveRig("Scene naming").isEmpty(),"Scene fixture must save complete rig");const auto before=state(p);const auto original=bank(p);
    require(p.renameScene(0,"  Melodic lead  ").isEmpty(),"Scene must rename with trimmed title");sameSound(before,state(p));const auto renamed=bank(p);
    require(renamed["slots"][0]["name"].toString()=="Melodic lead"&&juce::JSON::toString(original["slots"][0]["parameters"])==juce::JSON::toString(renamed["slots"][0]["parameters"])&&original["slots"][0]["board"]==renamed["slots"][0]["board"],"Rename must preserve exact saved parameters and board");
    require(static_cast<int>(p.scenes.status(p.apvts)["active"])==1&&!static_cast<bool>(p.scenes.status(p.apvts)["edited"])&&static_cast<bool>(p.status()["activeRigEdited"]),"Rename must keep active scene and mark saved rig edited");
    set(p,"AMP_MID",4);require(p.renameScene(1,"Hard lead").isEmpty()&&static_cast<bool>(p.scenes.status(p.apvts)["edited"]),"Rename must retain an edited live scene without storing its changes");
    const auto intact=p.scenes.save()["json"].toString();for(const auto& attempt:{std::pair<int,juce::String>{-1,"No"},{4,"No"},{2,"Empty"},{1," "},{1,juce::String::repeatedString("x",49)}})require(p.renameScene(attempt.first,attempt.second).isNotEmpty()&&p.scenes.save()["json"].toString()==intact,"Invalid rename must preserve the complete scene bank");
    juce::MemoryBlock session;p.getStateInformation(session);AmpSuiteAudioProcessor restored(false);restored.prepareToPlay(44100,257);restored.setStateInformation(session.getData(),static_cast<int>(session.getSize()));ready(restored);
    require(restored.scenes.save()["json"].toString()==intact&&restored.recallScene(0).isEmpty(),"Renamed scenes must restore and recall through native sessions");ready(restored);require(restored.apvts.getRawParameterValue("BOARD_DISTORTION_0_DIST_DRIVE")->load()==48,"Renamed source must recall its original stored tone");
    const auto base=juce::File::getSpecialLocation(juce::File::tempDirectory),root=base.getNonexistentChildFile("Cassian-scene-edit-"+juce::Uuid().toString(),"",false);struct Cleanup{juce::File root,base;~Cleanup(){if(root.isAChildOf(base))root.deleteRecursively();}}cleanup{root,base};require(root.createDirectory().wasOk(),"Scene pack folder must create");const auto pack=root.getChildFile("Scenes.zip");require(p.exportRigPack(pack).isEmpty(),"Renamed scene pack must export");AmpSuiteAudioProcessor imported(true,root.getChildFile("Library"));imported.prepareToPlay(48000,128);require(imported.importRigPack(pack).isEmpty()&&imported.loadRig(imported.getLibrary()["rigs"][0]["id"].toString()).isEmpty(),"Renamed scene pack must import/load");ready(imported);require(imported.scenes.save()["json"].toString()==intact,"Portable scenes must preserve their names and stored tones");
    p.requestFile(true,root.getChildFile("Missing.nam"));require(p.renameScene(0,"Blocked").isNotEmpty()&&p.scenes.save()["json"].toString()==intact,"Pending/unresolved assets must block scene edits");
    std::cout<<"Scene rename isolation, active/edited identity, rejected edits, session and pack checks passed\n";
}
