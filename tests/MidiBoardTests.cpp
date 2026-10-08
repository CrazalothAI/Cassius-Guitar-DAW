#include "../Source/PluginProcessor.h"
#include <iostream>
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<typename Predicate> void waitFor(Predicate ready){for(int i=0;i<1500;++i){if(ready())return;juce::Thread::sleep(5);}require(false,"Board MIDI worker timed out");}
juce::var object(std::initializer_list<std::pair<juce::Identifier,juce::var>> fields){auto o=std::make_unique<juce::DynamicObject>();for(const auto& field:fields)o->setProperty(field.first,field.second);return juce::var(o.release());}
juce::var mapping(const char* action,bool inverted=false,const char* type="cc"){return object({{"type",type},{"action",action},{"channel",1},{"number",20},{"inverted",inverted}});}
float get(AmpSuiteAudioProcessor& p,const juce::String& id){return p.apvts.getRawParameterValue(id)->load();}
void set(AmpSuiteAudioProcessor& p,const juce::String& id,float value){auto* parameter=p.apvts.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
void send(AmpSuiteAudioProcessor& p,int number,int value,bool pc=false){juce::AudioBuffer<float> audio(2,128);audio.clear();juce::MidiBuffer midi;midi.addEvent(pc?juce::MidiMessage::programChange(1,number):juce::MidiMessage::controllerEvent(1,number,value),63);p.processBlock(audio,midi);require(midi.isEmpty()&&std::isfinite(audio.getMagnitude(0,0,128)),"Board MIDI callback must consume messages and render finite audio");}
void settle(AmpSuiteAudioProcessor& p){waitFor([&]{return !static_cast<bool>(p.status()["rigLoading"]);});require(!p.status()["message"].toString().startsWith("Load failed:"),"Board MIDI fixture must prepare");}
juce::ValueTree state(AmpSuiteAudioProcessor& p){return juce::ValueTree::fromXml(*juce::XmlDocument::parse(p.getRig()["state"].toString()));}
std::vector<std::pair<juce::String,float>> values(AmpSuiteAudioProcessor& p){std::vector<std::pair<juce::String,float>> out;BoardParams::each([&](const auto& d){out.emplace_back(d.id,get(p,d.id));});return out;}
void unchanged(AmpSuiteAudioProcessor& p,const std::vector<std::pair<juce::String,float>>& before,const juce::String& changed={}){for(const auto& row:before)if(row.first!=changed&&row.first!="MASTER_VOL")require(get(p,row.first)==row.second,"Board MIDI must preserve unrelated parameters");}
// A second assignment provides a deterministic queue barrier, rather than
// sleeping and assuming that a held switch has already been processed.
void flush(AmpSuiteAudioProcessor& p){const int value=get(p,"MASTER_VOL") < -30 ? 127:0;send(p,119,value);waitFor([&]{return std::abs(get(p,"MASTER_VOL")-(value==0?-60.f:0.f))<.01f;});}
}
void runMidiBoardChecks(){
    const char* actions[]{"distortion1","distortion2","plate1","plate2","spring1","spring2","dist-drive1","dist-drive2","plate-mix1","plate-mix2","spring-mix1","spring-mix2"};
    MidiControl validation;
    for(const auto* action:actions){require(validation.setMapping(0,mapping(action)).isEmpty(),"Every board MIDI target must parse");require(validation.setMapping(0,mapping(action,false,"pc")).isEmpty()!=MidiControl::expression(action),"Board expressions must reject PC while toggles accept it");}
    require(validation.setMapping(0,mapping("plate-mix3")).isNotEmpty()&&MidiControl::boardTarget("BOARD_PLATE_0_ON")==nullptr,"Unknown actions must not become arbitrary parameter access");
    AmpSuiteAudioProcessor p(false);p.prepareToPlay(48000,128);auto fixture=state(p);fixture.removeChild(fixture.getChildWithName("PEDALBOARD"),nullptr);auto board=PedalboardState::emptySerial();
    for(int kind:{10,11,12})for(int slot=0;slot<2;++slot){juce::ValueTree row("BLOCK");row.setProperty("id",BoardParams::types[kind]+"."+juce::String(slot),nullptr);row.setProperty("type",BoardParams::types[kind],nullptr);row.setProperty("automationSlot",slot,nullptr);row.setProperty("lane",kind==10?"pre":"post",nullptr);row.setProperty("deleted",0,nullptr);board.addChild(row,-1,nullptr);}fixture.addChild(board,-1,nullptr);
    require(p.applyRig(object({{"schema",3},{"state",fixture.toXmlString()}})).isEmpty(),"Six-pedal MIDI board must load");settle(p);
    auto barrier=mapping("master");barrier.getDynamicObject()->setProperty("number",119);require(p.midiControl.setMapping(1,barrier).isEmpty(),"MIDI barrier must configure");p.midiControl.enable(true);
    for(const auto* action:actions){
        const auto* target=MidiControl::boardTarget(action);const int kind=BoardParams::types.indexOf(target->type);const auto key=target->control==nullptr?BoardParams::onId(kind,target->slot):BoardParams::parameter(kind,target->slot,target->control);
        require(p.midiControl.setMapping(0,mapping(action)).isEmpty(),"Board target must configure");
        if(target->control==nullptr){
            set(p,key,1);const auto before=values(p);send(p,20,0);send(p,20,127);flush(p);require(get(p,key)==0,"Footswitch must bypass its independent instance");unchanged(p,before,key);
            send(p,20,127);send(p,20,90);flush(p);require(get(p,key)==0,"Held board CC must not toggle again");
            send(p,20,63);send(p,20,64);flush(p);require(get(p,key)==1,"Released board CC must toggle again");
            require(p.midiControl.setMapping(0,mapping(action,false,"pc")).isEmpty(),"Board toggle PC must configure");send(p,20,127,true);flush(p);require(get(p,key)==0,"PC must toggle selected instance");send(p,20,127,true);flush(p);require(get(p,key)==1,"Every matching PC must execute");
        }else{
            const auto enabled=BoardParams::onId(kind,target->slot);set(p,enabled,0);const auto before=values(p);
            for(int value:{0,64,127}){send(p,20,value);flush(p);require(std::abs(get(p,key)-value*100.f/127)<.02f,"Board expression must follow its absolute 0..100 range");unchanged(p,before,key);require(get(p,enabled)==0,"Expression must not enable a bypassed pedal");}
            require(p.midiControl.setMapping(0,mapping(action,true)).isEmpty(),"Inverted expression must configure");send(p,20,0);flush(p);require(get(p,key)==100,"Inverted low expression must reach maximum");send(p,20,127);flush(p);require(get(p,key)==0,"Inverted high expression must reach minimum");
        }
    }
    require(p.midiControl.setMapping(0,mapping("dist-drive2")).isEmpty(),"Independent distortion expression must configure");
    require(p.boardCommand("move",object({{"id","distortion.1"},{"direction",-1}})).isEmpty(),"MIDI target must allow card reorder");settle(p);const auto reordered=values(p);send(p,20,64);flush(p);require(std::abs(get(p,"BOARD_DISTORTION_1_DIST_DRIVE")-6400.f/127)<.02f,"Reordering must retain numbered automation target");unchanged(p,reordered,"BOARD_DISTORTION_1_DIST_DRIVE");
    require(p.storeScene(0,"Six MIDI pedals").isEmpty(),"Board scene must store");
    require(p.boardCommand("remove",object({{"id","distortion.1"}})).isEmpty(),"Target must allow reserved deletion");settle(p);const auto removed=values(p);send(p,20,127);waitFor([&]{return p.midiControl.status()["error"].toString().isNotEmpty();});unchanged(p,removed);require(p.midiControl.status()["error"].toString().contains("distortion 2"),"Deleted target must report its missing numbered pedal");
    require(p.recallScene(0).isEmpty(),"Scene must restore current target topology");settle(p);send(p,20,127);waitFor([&]{return get(p,"BOARD_DISTORTION_1_DIST_DRIVE")==100&&p.midiControl.status()["error"].toString().isEmpty();});
    require(p.midiControl.setMapping(0,mapping("plate-mix2",true)).isEmpty(),"Session expression must configure");const auto config=juce::JSON::toString(p.midiControl.configuration());juce::MemoryBlock session;p.getStateInformation(session);AmpSuiteAudioProcessor restored(false);restored.prepareToPlay(44100,257);restored.setStateInformation(session.getData(),static_cast<int>(session.getSize()));settle(restored);require(juce::JSON::toString(restored.midiControl.configuration())==config,"New mappings must survive native sessions with enable state");send(restored,20,0);waitFor([&]{return get(restored,"BOARD_PLATE_1_PLATE_MIX")==100;});
    require(p.applyRig(object({{"schema",3},{"state",fixture.toXmlString()}})).isEmpty(),"Complete rig must recall");settle(p);require(juce::JSON::toString(p.midiControl.configuration())==config,"Rig recall must retain new MIDI bindings");
    AmpSuiteAudioProcessor legacy(false);legacy.prepareToPlay(48000,128);require(legacy.midiControl.setMapping(0,mapping("spring1")).isEmpty(),"Missing target may be assigned in advance");legacy.midiControl.enable(true);const auto absent=values(legacy);send(legacy,20,0);send(legacy,20,127);waitFor([&]{return legacy.midiControl.status()["error"].toString().isNotEmpty();});unchanged(legacy,absent);
    const auto base=juce::File::getSpecialLocation(juce::File::tempDirectory);p.requestFile(true,base.getChildFile(juce::Uuid().toString()+".nam"));require(p.midiControl.setMapping(0,mapping("plate-mix2")).isEmpty(),"Unresolved target mapping must configure");const auto pending=values(p);send(p,20,127);waitFor([&]{return p.midiControl.status()["error"].toString().contains("Finish loading");});unchanged(p,pending);
    std::cout<<"Independent pedal MIDI toggles, expressions, bypass, reorder/deletion, scene/session recall and loading checks passed\n";
}
