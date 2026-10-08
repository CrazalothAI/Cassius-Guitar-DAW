#include "../Source/PluginProcessor.h"
#include <iostream>

namespace {
void require(bool ok, const char* reason) {if (!ok) throw std::runtime_error(reason);}
void ready(ToneRecovery& store) {for (int i=0;i<5000;++i) {if (!store.busy()) return; juce::Thread::sleep(1);} require(false,"Tone recovery worker timed out");}
}
void runToneRecoveryChecks() {
    const auto parent = juce::File::getSpecialLocation(juce::File::tempDirectory), root = parent.getChildFile("Cassian-tone-recovery-" + juce::Uuid().toString());
    struct Cleanup {juce::File root,parent; ~Cleanup() {if(root.isAChildOf(parent))root.deleteRecursively();}} cleanup{root,parent};
    auto processor = std::make_unique<AmpSuiteAudioProcessor>(false); auto rig = processor->getRig();
    juce::String firstId;
    {
        ToneRecovery store(root); ready(store); require(store.status()["snapshots"].size()==0,"New snapshot history must be empty");
        require(store.capture(rig,"Clean practice").isEmpty(),"Snapshot must queue"); ready(store);
        auto rows=store.status()["snapshots"]; require(rows.size()==1 && store.status()["error"].toString().isEmpty(),"Worker must commit one valid snapshot");
        firstId=rows[0]["id"].toString(); require(store.read(firstId)["state"].toString()==rig["state"].toString(),"Snapshot must preserve complete tone state");
        require(store.capture(rig,"Clean practice").isEmpty(),"Unchanged snapshot may queue"); ready(store); require(store.status()["snapshots"].size()==1,"Unchanged tone must not consume history");
        require(store.setAutomatic(false).isEmpty(),"Automatic preference must queue"); ready(store); require(!store.automatic(),"Automatic snapshots can be disabled");
        bool rejected=false; try {store.read("../escape.json");}catch(const std::exception&){rejected=true;} require(rejected,"Snapshot IDs must reject traversal");
        const auto saved=root.getChildFile("recovery-tones").getChildFile(firstId); auto damaged=juce::JSON::parse(saved.loadFileAsString()); damaged.getDynamicObject()->setProperty("state","damaged"); saved.replaceWithText(juce::JSON::toString(damaged));
        rejected=false;try{store.read(firstId);}catch(const std::exception&){rejected=true;}require(rejected,"Damaged snapshot checksum must reject");
    }
    {
        ToneRecovery store(root); ready(store); require(!store.automatic(),"Automatic preference must survive reopening"); require(store.status()["snapshots"].size()==0 && store.status()["error"].toString().isNotEmpty(),"Damaged snapshots must be reported without hiding valid future history");
        for(int i=0;i<67;++i) {
            auto* drive=processor->apvts.getParameter("DRIVE_GAIN"); drive->setValueNotifyingHost(drive->convertTo0to1(1.f+.1f*static_cast<float>(i)));
            require(store.capture(processor->getRig(),"Retention test").isEmpty(),"Changed tone must queue"); ready(store);
        }
        const auto rows=store.status()["snapshots"]; require(rows.size()==64,"Automatic history must retain at most 64 valid snapshots");
        require(rows[0]["id"].toString()!=firstId,"Corrupt files must not displace valid history");
        for(const auto& row:*rows.getArray()) require(store.read(row["id"].toString()).getDynamicObject()!=nullptr,"Retained history must remain readable");
    }
    // A queued save is flushed during destruction without capturing a processor.
    const auto closing=root.getChildFile("Closing");
    {ToneRecovery store(closing);ready(store);require(store.capture(processor->getRig(),"Closing tone").isEmpty(),"Closing snapshot must queue");}
    {ToneRecovery store(closing);ready(store);require(store.status()["snapshots"].size()==1,"Closing must finish an already queued snapshot");}
    // Public standalone recovery adds a saved copy; it never recalls it for the player.
    auto live=std::make_unique<AmpSuiteAudioProcessor>(true,root.getChildFile("Live"));
    require(!static_cast<bool>(live->toneRecoveryStatus()["available"]),"Hosts must not start automatic snapshot storage");
    live->showDeviceSettings=[]{}; live->startToneRecovery();
    auto wait=[&]{for(int i=0;i<5000;++i){if(!static_cast<bool>(live->toneRecoveryStatus()["busy"]))return;juce::Thread::sleep(1);}require(false,"Live snapshot worker timed out");}; wait();
    require(live->saveRig("My clean").isEmpty(),"Recovery fixture must save");
    auto* drive=live->apvts.getParameter("DRIVE_GAIN");drive->setValueNotifyingHost(drive->convertTo0to1(7));require(live->captureRecoveryTone().isEmpty(),"Standalone snapshot must queue");wait();
    const auto id=live->toneRecoveryStatus()["snapshots"][0]["id"].toString();drive->setValueNotifyingHost(drive->convertTo0to1(2));
    require(live->recoverTone(id).isEmpty() && live->getLibrary()["rigs"].size()==2,"Recovery must add a saved preset");
    require(live->apvts.getRawParameterValue("DRIVE_GAIN")->load()==2 && live->status()["activeRigName"].toString()=="My clean","Recovery must preserve live edits and rig identity");
    const auto library=live->getLibrary(); require(live->recoverTone("../bad.json").isNotEmpty() && juce::JSON::toString(live->getLibrary())==juce::JSON::toString(library),"Rejected recovery must not mutate the library");
    std::cout<<"Automatic tone snapshot integrity, deduplication, retention, preferences, shutdown and additive recovery passed\n";
}
