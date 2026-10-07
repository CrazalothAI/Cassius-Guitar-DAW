#include "../Source/PluginProcessor.h"
#include <iostream>
namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void ready(AmpSuiteAudioProcessor& p){for(int i=0;i<1000;++i){auto s=p.status();if(!static_cast<bool>(s["rigLoading"])){require(!s["message"].toString().startsWith("Load failed:"),"Reset graph must prepare");return;}juce::Thread::sleep(5);}require(false,"Reset graph timed out");}
juce::ValueTree tree(AmpSuiteAudioProcessor& p){return juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString()));}
juce::var args(std::initializer_list<std::pair<juce::Identifier,juce::var>> fields){auto p=std::make_unique<juce::DynamicObject>();for(const auto& f:fields)p->setProperty(f.first,f.second);return juce::var(p.release());}
void set(AmpSuiteAudioProcessor& p,const char* id,float x){auto* v=p.apvts.getParameter(id);v->setValueNotifyingHost(v->convertTo0to1(x));}
float value(const juce::ValueTree& t,const juce::String& id){return static_cast<float>(t.getChildWithProperty("id",id)["value"]);}
}
void runBoardResetChecks(const juce::File& fixture){
    AmpSuiteAudioProcessor p(false);p.prepareToPlay(48000,128);require(p.boardCommand("convert",{}).isEmpty(),"Reset fixture must convert");ready(p);
    const auto first=tree(p).getChildWithName("PEDALBOARD").getChildWithProperty("type","eq")["id"].toString();
    require(p.boardCommand("duplicate",args({{"id",first}})).isEmpty(),"Reset fixture must duplicate EQ");ready(p);
    juce::String second;for(const auto& row:tree(p).getChildWithName("PEDALBOARD"))if(row["type"].toString()=="eq"&&static_cast<int>(row["automationSlot"])==1)second=row["id"].toString();
    set(p,"EQ_FOCUS",3);set(p,"BOARD_EQ_1_EQ_FOCUS",6);set(p,"BOARD_EQ_1_TRIM",4);set(p,"BOARD_EQ_1_EQ_ON",0);set(p,"GUITAR_MIX_LEVEL",5);
    const auto before=tree(p);require(p.boardCommand("reset",args({{"id",second}})).isEmpty(),"Selected pedal must reset");ready(p);const auto reset=tree(p);
    require(PedalboardState::equal(before,reset),"Reset must preserve topology, identities, slots and lanes");
    const juce::StringArray controls{"BOARD_EQ_1_EQ_BODY","BOARD_EQ_1_EQ_MUD","BOARD_EQ_1_EQ_FOCUS","BOARD_EQ_1_EQ_FIZZ","BOARD_EQ_1_TRIM"};
    BoardParams::each([&](const auto& def){const auto x=value(reset,def.id);require(std::abs(x-(controls.contains(def.id)?def.initial:value(before,def.id)))<.001f,"Reset must change only selected controls/trim, preserving bypass and every other parameter");});
    set(p,"AMP_MID",3);set(p,"MASTER_VOL",-17);
    require(p.boardCommand("undo",{}).isEmpty(),"Reset must undo");ready(p);require(value(tree(p),"BOARD_EQ_1_EQ_FOCUS")==6&&value(tree(p),"BOARD_EQ_1_TRIM")==4&&value(tree(p),"AMP_MID")==3&&value(tree(p),"MASTER_VOL")==-17,"Undo must restore the selected settings and retain current amp/listening controls");
    require(p.boardCommand("redo",{}).isEmpty(),"Reset must redo");ready(p);require(value(tree(p),"BOARD_EQ_1_EQ_FOCUS")==0&&value(tree(p),"BOARD_EQ_1_EQ_ON")==0,"Redo must restore defaults while retaining bypass");
    require(p.boardCommand("lane",args({{"id",second},{"lane","pre"}})).isEmpty(),"Reset fixture must move");ready(p);require(p.boardCommand("undo",{}).isEmpty(),"Fixture lane edit must undo");ready(p);
    require(p.boardCommand("reset",args({{"id",second}})).isEmpty()&&static_cast<bool>(p.boardStatus()["canRedo"]),"Resetting defaults must preserve Redo history");
    auto assigned=tree(p);assigned.setProperty("pedalPath",fixture.getFullPathName(),nullptr);assigned.getChildWithProperty("id","PEDAL_INPUT").setProperty("value",7,nullptr);assigned.getChildWithProperty("id","PEDAL_OUTPUT").setProperty("value",-3,nullptr);assigned.getChildWithProperty("id","PEDAL_ON").setProperty("value",0,nullptr);
    require(p.applyRig(args({{"schema",3},{"state",assigned.toXmlString()}})).isEmpty(),"Capture reset fixture must prepare");ready(p);
    const auto captureBefore=tree(p);const auto captured=captureBefore.getChildWithName("PEDALBOARD").getChildWithProperty("type","neural-pedal")["id"].toString();
    require(p.boardCommand("reset",args({{"id",captured}})).isEmpty(),"Captured pedal levels must reset");ready(p);const auto captureAfter=tree(p);
    require(captureBefore["pedalPath"]==captureAfter["pedalPath"]&&captureBefore["pedalId"]==captureAfter["pedalId"]&&value(captureAfter,"PEDAL_INPUT")==0&&value(captureAfter,"PEDAL_OUTPUT")==0&&value(captureAfter,"PEDAL_ON")==0,"Capture reset must keep its assigned file and bypass");
    const auto intact=p.getRig()["state"].toString();require(p.boardCommand("reset",args({{"id","missing"}})).isNotEmpty()&&p.getRig()["state"].toString()==intact,"Unknown reset identity must reject atomically");
    std::cout<<"Pedal reset isolation, bypass/files, Undo/Redo and no-op history checks passed\n";
}
