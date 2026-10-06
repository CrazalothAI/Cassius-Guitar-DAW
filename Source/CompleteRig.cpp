#include "PluginProcessor.h"

void AmpSuiteAudioProcessor::prepareCompleteRig(juce::ValueTree state, bool preserveGlobals, juce::uint64 generation)
{
    try {
        const auto preparedRate = reportedRate.load();
        const int preparedBlock = juce::jlimit(1, 256, reportedBlock.load()), channels = reportedChannels.load();
        std::unique_ptr<NamWrapper> nextAmp, nextPedal, nextPedal1;
        auto nextCab = std::make_unique<DualCab>();
        std::vector<juce::ValueTree> assets;
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        for (const auto* label : {"model", "ir", "pedal", "irB", "pedal1", "ambience", "ambience1"}) {
            const juce::String stage(label), path = state[stage + "Path"].toString();
            if (path.isEmpty()) continue;
            const auto kind = stage.startsWith("ambience") ? "ambience" : stage == "model" ? "amp" : (stage == "ir" || stage == "irB") ? "cab" : "pedal";
            const juce::File file(path);
            auto asset = AssetLibrary::describe(file, kind);
            const auto expected = state[stage + "Id"].toString();
            if (expected.isNotEmpty() && expected != asset["id"].toString()) throw std::runtime_error("A rig asset changed during preparation");
            sharedStore.manage(asset); assets.push_back(asset);
            // Managed copies are stable if the original download is moved later.
            state.setProperty(stage + "Path", asset["path"], nullptr);
            state.setProperty(stage + "Id", asset["id"], nullptr);
            const juce::File managed(asset["path"].toString());
            if (stage.startsWith("ambience")) continue; // Validated and prepared by the serial node below.
            if (stage != "ir" && stage != "irB") {
                auto next = std::make_unique<NamWrapper>(managed, assetSourceName(asset, state.getChildWithName("LIBRARY"))); next->prepare(preparedRate, preparedBlock);
                if (stage == "model") nextAmp = std::move(next); else if (stage == "pedal1") nextPedal1 = std::move(next); else nextPedal = std::move(next);
            } else {
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(managed));
                if (!reader || reader->numChannels == 0 || reader->numChannels > 2 || reader->lengthInSamples <= 0 || reader->lengthInSamples > reader->sampleRate * 10)
                    throw std::runtime_error("Rig contains an unsupported cabinet response");
                juce::AudioBuffer<float> impulse(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                if (!reader->read(&impulse, 0, impulse.getNumSamples(), 0, true, true)) throw std::runtime_error("Could not read the rig cabinet");
                nextCab->load(std::move(impulse), reader->sampleRate, stage == "irB" ? 1 : 0);
            }
        }
        // This isolated convolution is fully built before it can reach the callback.
        const auto v = [&](const char* id) { return static_cast<float>(state.getChildWithProperty("id", id)["value"]); };
        nextCab->prepare({preparedRate, static_cast<juce::uint32>(preparedBlock), static_cast<juce::uint32>(channels)},
            {v("CAB_B_ON") >= .5f, v("CAB_BLEND"), v("CAB_A_LEVEL"), v("CAB_B_LEVEL"), v("CAB_A_PAN"), v("CAB_B_PAN"),
             v("CAB_A_INVERT") >= .5f, v("CAB_B_INVERT") >= .5f, v("CAB_A_DELAY"), v("CAB_B_DELAY"), v("CAB_LOW_CUT"), v("CAB_HIGH_CUT")});
        auto nextBoard = PedalboardState::serial(state) ? std::make_unique<SerialPedalboard>(state, apvts,
            juce::dsp::ProcessSpec {preparedRate, static_cast<juce::uint32>(preparedBlock), static_cast<juce::uint32>(channels)}) : nullptr;
        if (nextBoard) for (const auto& row : state.getChildWithName("PEDALBOARD"))
            if ((PedalboardState::kind(row) == 2 || PedalboardState::kind(row) == 8) && static_cast<int>(row["deleted"]) == 0) {
                const int slot = static_cast<int>(row["automationSlot"]);
                const int kind = PedalboardState::kind(row);
                const bool missing = kind == 2 ? !(slot == 0 ? nextPedal : nextPedal1) : state[slot == 0 ? "ambiencePath" : "ambience1Path"].toString().isEmpty();
                if (v(BoardParams::onId(kind, slot).toRawUTF8()) >= .5f && missing) throw std::runtime_error("Assign a file before enabling a captured pedal");
            }
        if (generation != requestGeneration.load() || threadShouldExit()) return;
        rigMuted.store(false); rigSwapReady.store(true);
        // Fade before committing. Fixed recalls retain fixed effect tails;
        // replacing a serial graph starts fresh tails.
        while (!threadShouldExit() && generation == requestGeneration.load() && !rigMuted.load()
               && juce::Time::getMillisecondCounterHiRes() - lastAudioTick.load() < 100)
            wait(2);
        if (threadShouldExit() || generation != requestGeneration.load()) return;
        // Serialize the final commit with new requests. A newer selection must
        // never be overwritten by a rig that finished preparing just before it.
        const juce::ScopedLock commitLock(requestLock);
        if (threadShouldExit() || generation != requestGeneration.load()) return;
        if (preserveGlobals)
            for (const auto& definition : Params::definitions)
                if (PerformanceScenes::global(definition.id))
                    state.getChildWithProperty("id", definition.id).setProperty("value", apvts.getRawParameterValue(definition.id)->load(), nullptr);
        const juce::String gear = nextAmp ? nextAmp->gear() : "";
        const bool levelled = nextAmp && nextAmp->hasLoudness(), hasCab = nextAmp && nextAmp->hasCabinet(), cabKnown = nextAmp && nextAmp->cabinetIsKnown();
        const double levelDb = nextAmp ? nextAmp->levelMatchDb() : 0;
        {
            const juce::ScopedLock lock(dspLock);
            if (preparedRate != rate || preparedBlock != maxBlock || channels != getTotalNumOutputChannels())
                throw std::runtime_error("Audio settings changed during rig preparation; recall the rig again");
            // Fixed effects rested during serial processing. Do not resume
            // their frozen delay/reverb buffers when returning to an old rig.
            if (serialBoard && !nextBoard) { delay.reset(); roomDelay.reset(); reverb.reset(); }
            // No parsing, prewarming, convolution construction, or destruction here.
            model.swap(nextAmp); pedal.swap(nextPedal); pedal1.swap(nextPedal1); cab.swap(nextCab); serialBoard.swap(nextBoard);
            ampExpectedRate.store(model ? model->expectedRate() : 0); pedalExpectedRate.store(pedal ? pedal->expectedRate() : 0);
            activeAmpSource = static_cast<int>(state.getChildWithProperty("id", "AMP_SOURCE")["value"]);
            ampSlotGain.setCurrentAndTargetValue(1); rigSwitchGain.setCurrentAndTargetValue(0);
            cleanBlend.setCurrentAndTargetValue(static_cast<float>(state.getChildWithProperty("id", "AMP_CLEAN")["value"]));
        }
        // The guitar stays faded while host parameters are replaced outside dspLock.
        const auto midi = state.getChildWithName("MIDICONTROL");
        if (midi.isValid()) { const auto text = midi["json"].toString(); midiControl.restore(text.length() <= 32768 ? juce::JSON::parse(text) : juce::var("invalid")); state.removeChild(midi, nullptr); }
        apvts.replaceState(state);
        scenes.restore(state.getChildWithName("SCENES"));
        if (pendingBoardGeneration == generation && pendingBoardBefore.isValid()) {
            if (pendingBoardAction == "undo") { boardUndo.pop_back(); boardRedo.push_back(pendingBoardBefore); }
            else if (pendingBoardAction == "redo") { boardRedo.pop_back(); boardUndo.push_back(pendingBoardBefore); }
            else { boardUndo.push_back(pendingBoardBefore); if (boardUndo.size() > 32) boardUndo.erase(boardUndo.begin()); boardRedo.clear(); }
        } else if (pendingScene < 0) { boardUndo.clear(); boardRedo.clear(); }
        if (pendingScene >= 0) scenes.markActive(pendingScene);
        pendingScene = -1; pendingBoardBefore = {}; pendingBoardAction.clear();
        {
            const juce::ScopedLock lock(requestLock);
            desiredAmbience = ambiencePath = state["ambiencePath"].toString(); desiredAmbience1 = ambience1Path = state["ambience1Path"].toString();
            desiredPedal1 = pedal1Path = state["pedal1Path"].toString();
            desiredIrB = irBPath = state["irBPath"].toString();
            desiredModel = modelPath = state["modelPath"].toString(); desiredPedal = pedalPath = state["pedalPath"].toString(); desiredIr = irPath = state["irPath"].toString();
            library.merge(state.getChildWithName("LIBRARY")); for (const auto& asset : assets) library.upsert(asset);
            activeRig.restore(state.getChildWithName("ACTIVE_RIG"));
            activeRig.refreshSavedBaseline(library.find(activeRig.id));
            // nextAmp/nextPedal now own the retired instances, destroyed on this thread.
            ampGear = gear; ampLevelled = levelled; ampLevelDb = levelDb; ampHasCab = hasCab; ampCabKnown = cabKnown;
            if (persistLibrary().isEmpty()) message = "Complete rig ready";
        }
        if (generation == requestGeneration.load()) rigLoading.store(false);
        rigSwapReady.store(false);
    } catch (const std::exception& e) {
        if (generation == requestGeneration.load()) {
            const juce::ScopedLock lock(requestLock); message = "Load failed: " + juce::String(e.what());
            rigLoading.store(false); rigSwapReady.store(false); pendingBoardBefore = {}; pendingBoardAction.clear(); pendingScene = -1;
        }
    }
}
