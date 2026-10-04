#include "PluginProcessor.h"

void AmpSuiteAudioProcessor::prepareCompleteRig(juce::ValueTree state, bool preserveGlobals, juce::uint64 generation)
{
    try {
        const auto preparedRate = reportedRate.load();
        const int preparedBlock = juce::jlimit(1, 256, reportedBlock.load()), channels = reportedChannels.load();
        std::unique_ptr<NamWrapper> nextAmp, nextPedal;
        auto nextCab = std::make_unique<DualCab>();
        std::vector<juce::ValueTree> assets;
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        for (const auto* label : {"model", "ir", "pedal", "irB"}) {
            const juce::String stage(label), path = state[stage + "Path"].toString();
            if (path.isEmpty()) continue;
            const auto kind = stage == "model" ? "amp" : (stage == "ir" || stage == "irB") ? "cab" : "pedal";
            const juce::File file(path);
            auto asset = AssetLibrary::describe(file, kind);
            const auto expected = state[stage + "Id"].toString();
            if (expected.isNotEmpty() && expected != asset["id"].toString()) throw std::runtime_error("A rig asset changed during preparation");
            sharedStore.manage(asset); assets.push_back(asset);
            // Managed copies are stable if the original download is moved later.
            state.setProperty(stage + "Path", asset["path"], nullptr);
            state.setProperty(stage + "Id", asset["id"], nullptr);
            const juce::File managed(asset["path"].toString());
            if (stage != "ir" && stage != "irB") {
                auto next = std::make_unique<NamWrapper>(managed, assetSourceName(asset, state.getChildWithName("LIBRARY"))); next->prepare(preparedRate, preparedBlock);
                if (stage == "model") nextAmp = std::move(next); else nextPedal = std::move(next);
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
        if (generation != requestGeneration.load() || threadShouldExit()) return;
        rigMuted.store(false); rigSwapReady.store(true);
        // Let the old guitar fade before committing. Effects keep running, so their
        // existing tails survive. Offline/stopped playback does not need a fade wait.
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
                if (juce::String(definition.id) == "INPUT_GAIN" || juce::String(definition.id) == "MASTER_VOL" || juce::String(definition.id).startsWith("METRO_"))
                    state.getChildWithProperty("id", definition.id).setProperty("value", apvts.getRawParameterValue(definition.id)->load(), nullptr);
        const juce::String gear = nextAmp ? nextAmp->gear() : "";
        const bool levelled = nextAmp && nextAmp->hasLoudness(), hasCab = nextAmp && nextAmp->hasCabinet(), cabKnown = nextAmp && nextAmp->cabinetIsKnown();
        const double levelDb = nextAmp ? nextAmp->levelMatchDb() : 0;
        {
            const juce::ScopedLock lock(dspLock);
            if (preparedRate != rate || preparedBlock != maxBlock || channels != getTotalNumOutputChannels())
                throw std::runtime_error("Audio settings changed during rig preparation; recall the rig again");
            // No parsing, prewarming, convolution construction, or destruction here.
            model.swap(nextAmp); pedal.swap(nextPedal); cab.swap(nextCab);
            ampExpectedRate.store(model ? model->expectedRate() : 0); pedalExpectedRate.store(pedal ? pedal->expectedRate() : 0);
            activeAmpSource = static_cast<int>(state.getChildWithProperty("id", "AMP_SOURCE")["value"]);
            ampSlotGain.setCurrentAndTargetValue(1); rigSwitchGain.setCurrentAndTargetValue(0);
            cleanBlend.setCurrentAndTargetValue(static_cast<float>(state.getChildWithProperty("id", "AMP_CLEAN")["value"]));
        }
        // The guitar stays faded while host parameters are replaced outside dspLock.
        apvts.replaceState(state);
        scenes.restore(state.getChildWithName("SCENES"));
        {
            const juce::ScopedLock lock(requestLock);
            desiredIrB = irBPath = state["irBPath"].toString();
            desiredModel = modelPath = state["modelPath"].toString(); desiredPedal = pedalPath = state["pedalPath"].toString(); desiredIr = irPath = state["irPath"].toString();
            library.merge(state.getChildWithName("LIBRARY")); for (const auto& asset : assets) library.upsert(asset);
            // nextAmp/nextPedal now own the retired instances, destroyed on this thread.
            ampGear = gear; ampLevelled = levelled; ampLevelDb = levelDb; ampHasCab = hasCab; ampCabKnown = cabKnown;
            if (persistLibrary().isEmpty()) message = "Complete rig ready";
        }
        if (generation == requestGeneration.load()) rigLoading.store(false);
        rigSwapReady.store(false);
    } catch (const std::exception& e) {
        if (generation == requestGeneration.load()) {
            const juce::ScopedLock lock(requestLock); message = "Load failed: " + juce::String(e.what());
            rigLoading.store(false); rigSwapReady.store(false);
        }
    }
}
