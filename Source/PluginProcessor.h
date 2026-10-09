#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "params/ParameterIDs.h"
#include "dsp/NamWrapper.h"
#include "dsp/DualCab.h"
#include "dsp/StudioCompressor.h"
#include "dsp/Overdrive.h"
#include "dsp/ToneStack.h"
#include "dsp/GuitarGate.h"
#include "dsp/PitchTracker.h"
#include "dsp/DynamicResonanceFilter.h"
#include "dsp/PiezoSimulator.h"
#include "dsp/SubSynthesizer.h"
#include "dsp/MicroDelay.h"
#include "dsp/HighGainAmp.h"
#include "dsp/SpeakerCab.h"
#include "dsp/NoiseShield.h"
#include "dsp/HumCanceller.h"
#include "dsp/Metronome.h"
#include "dsp/PedalEq.h"
#include "dsp/GuitarMix.h"
#include "dsp/StereoChorus.h"
#include "dsp/ModulationPedal.h"
#include "dsp/SerialPedalboard.h"
#include "DeviceHooks.h"
#include "AssetLibrary.h"
#include "LibraryStore.h"
#include "PracticeEngine.h"
#include "TakeLibrary.h"
#include "MidiControl.h"
#include "PedalboardState.h"
#include "PerformanceScenes.h"
#include "ActiveRig.h"
#include "LibraryBackup.h"
#include "ToneRecovery.h"
#include "PracticeJournal.h"

class AmpSuiteAudioProcessor final : public juce::AudioProcessor, public StandaloneDeviceHooks, private juce::Thread, private juce::Timer
{
public:
    explicit AmpSuiteAudioProcessor(bool sharedLibrary = true, juce::File libraryRoot = LibraryStore::defaultRoot());
    ~AmpSuiteAudioProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override { lastAudioTick.store(0, std::memory_order_relaxed); practice.command("pause"); takes.stopReview(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Cassian"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 60; } // Two serial 30-second ambience responses.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void requestFile(bool model, const juce::File&);
    void requestPedal(const juce::File&);
    void requestCabB(const juce::File&);
    bool selectAmpVoice(const juce::String&);
    juce::var getLibrary();
    juce::var inspectRig(const juce::String& id);
    void importAssets(const juce::Array<juce::File>&, const juce::String& kind);
    juce::var getRig();
    juce::var getSavedRig(const juce::String& id);
    juce::String applyRig(const juce::var&, bool preserveGlobals = true, bool matchLoudness = false);
    juce::String saveRig(const juce::String& name);
    juce::String updateActiveRig();
    juce::String importRig(const juce::String& name, const juce::var& rig);
    juce::String loadRig(const juce::String& id);
    static juce::var startingRigCatalog();
    juce::String loadStartingRig(const juce::String& id);
    bool removeRig(const juce::String& id);
    juce::String editRig(const juce::String& id, const juce::var& changes);
    juce::String duplicateRig(const juce::String& id, const juce::String& name);
    bool editAsset(const juce::String& id, const juce::var& changes);
    bool selectAsset(const juce::String& id, bool cabinetB = false);
    juce::String relinkAsset(const juce::String& id, const juce::File& file);
    juce::String exportRigPack(const juce::File& destination, const juce::var& snapshot = {});
    juce::String importRigPack(const juce::File& source);
    juce::String validateRigDocument(const juce::var& rig);
    // Validate and migrate on an isolated tree before live recall or pack writes.
    juce::String migrateRigDocument(const juce::var& rig, juce::ValueTree& state);
    void requestRigPack(bool save, const juce::File& file, const juce::var& snapshot = {});
    juce::String requestBackup(bool restore, const juce::File& file, bool includeTakes = true, std::optional<juce::StringArray> selectedTakeIds = std::nullopt);
    juce::var backupStatus();
    void cancelBackup() { backupCancelled.store(true); }
    juce::String revealBackup();
    void startToneRecovery() override;
    juce::String captureRecoveryTone();
    juce::String setAutomaticRecovery(bool);
    juce::String recoverTone(const juce::String&);
    juce::var toneRecoveryStatus();
    void reportLibraryResult(const juce::String& text) { const juce::ScopedLock lock(requestLock); message = text; }
    // The pitch tracker only runs while the tuner is open (or Thicken needs it).
    void setTunerActive(bool shouldRun) { tunerRequested.store(shouldRun); }
    juce::var status();
    juce::String storeScene(int slot, const juce::String& name);
    juce::String renameScene(int slot, const juce::String& name);
    juce::String copyScene(int source, int destination, const juce::String& name);
    juce::String recallScene(int slot);
    juce::String clearScene(int slot);
    juce::String boardCommand(const juce::String& action, const juce::var& args);
    juce::var boardStatus();
    PerformanceScenes scenes;
    MidiControl midiControl;
    // The take store stays alive while the recorder finishes during destruction.
    PracticeEngine takeReview;
    TakeLibrary takes;
    PracticeEngine practice;
    std::unique_ptr<PracticeJournal> practiceJournal;
    void renderGuitarOffline(juce::AudioBuffer<float>&, int frames);
    juce::AudioProcessorValueTreeState apvts;
private:
    void run() override;
    float value(Params::Index i) const { return parameters[static_cast<size_t>(i)]->load(); }
    void processChunk(juce::AudioBuffer<float>&);
    void finishOutputMix(juce::AudioBuffer<float>&, bool suppressClick = false);
    void processUniversalAmp(juce::AudioBuffer<float>&);
    DualCab::Settings cabinetSettings() const;
    StudioCompressor::Settings compressorSettings(bool enabled) const;
    ModulationPedal::Settings modulationSettings() const;
    void setParameterValue(const char* id, float value);
    juce::String handleMidiAction(const MidiControl::Mapping&, int value);
    juce::ValueTree copyRigState(bool includeSavedRigs);
    juce::String persistLibrary(const juce::StringArray& removed = {});
    juce::String assetSourceName(const juce::ValueTree& asset, const juce::ValueTree& incoming = {});
    void prepareCompleteRig(juce::ValueTree state, bool preserveGlobals, juce::uint64 generation);
    std::vector<juce::ValueTree> boardUndo, boardRedo;
    juce::ValueTree pendingBoardBefore;
    juce::String pendingBoardAction;
    juce::uint64 pendingBoardGeneration = 0;
    int pendingScene = -1;
    std::array<std::atomic<float>*, Params::definitions.size()> parameters {};
    juce::CriticalSection dspLock, requestLock;
    AssetLibrary library;
    ActiveRig activeRig;
    LibraryStore sharedStore;
    std::unique_ptr<ToneRecovery> toneRecovery;
    void timerCallback() override;
    juce::CriticalSection backupLock;
    std::atomic<bool> backupBusy {false}, backupCancelled {false};
    std::atomic<double> backupProgress {0};
    juce::String backupOperation, backupError, backupSummary, backupLocation;
    std::vector<std::pair<juce::File, juce::String>> pendingImports;
    struct PackJob { juce::File file; bool save; juce::var snapshot; };
    std::vector<PackJob> pendingPacks;
    juce::String desiredModel, desiredIr, modelPath, irPath, message = "Load an amp capture to get started";
    bool modelPending = false, irPending = false, irBPending = false;
    juce::String desiredIrB, irBPath;
    juce::String desiredPedal, pedalPath;
    juce::String desiredPedal1, pedal1Path;
    juce::String desiredAmbience, ambiencePath, desiredAmbience1, ambience1Path;
    bool pedalPending = false;
    juce::ValueTree pendingRig;
    bool pendingRigPreservesGlobals = true;
    std::atomic<juce::uint64> requestGeneration {0};
    std::array<std::atomic<juce::uint64>, 4> stageRequestGeneration {};
    bool sceneAssetsLoading() const; // Caller holds requestLock.
    std::atomic<bool> rigLoading {false}, rigSwapReady {false}, rigMuted {false};
    std::atomic<double> lastAudioTick {0};
    std::atomic<int> reportedChannels {2};
    std::unique_ptr<NamWrapper> model;
    std::unique_ptr<NamWrapper> pedal;
    std::unique_ptr<NamWrapper> pedal1;
    std::unique_ptr<SerialPedalboard> serialBoard;
    std::unique_ptr<DualCab> cab = std::make_unique<DualCab>();
    StudioCompressor preCompressor, postCompressor;
    Overdrive overdrive;
    std::atomic<float> reportedCompression {0}, reportedOverdriveLatency {0};
    GuitarGate gate;
    PitchTracker pitchTracker;
    DynamicResonanceFilter dynamicResonance;
    PiezoSimulator piezo;
    SubSynthesizer subSynth;
    MicroDelay microDelay;
    HighGainAmp fallbackAmp;
    SpeakerCab speaker;
    NoiseShield noiseShield;
    HumCanceller humCanceller;
    Metronome metronome;
    PedalEq pedalEq;
    GuitarMix guitarMix;
    juce::AudioBuffer<float> guitarMixDelta;
    juce::dsp::Gain<float> inputGain, ampGain, masterGain;
    juce::SmoothedValue<float> driveGain, delayTime, delayMix;
    juce::SmoothedValue<float> pedalInputGain, pedalOutputGain, delayFeedbackGain, reverbPreDelay;
    StereoChorus chorus;
    ModulationPedal modulation;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> roomDelay;
    juce::AudioBuffer<float> roomAudio;
    juce::SmoothedValue<float> cleanBlend, tightCutoff;
    juce::SmoothedValue<float> ampSlotGain;
    juce::SmoothedValue<float> rigSwitchGain;
    int activeAmpSource = 0;
    juce::SmoothedValue<float> compressionMix, highCutoff, stereoWidth;
    juce::dsp::Compressor<float> cleanCompressor;
    std::vector<float> cleanAudio, gateEnvelope, pedalAudio, subSynthAudio, dryInput, recordingDry;
    juce::AudioBuffer<float> backingAudio;
    juce::SmoothedValue<float> pedalBlend;
    float cleanLow = 0, cleanHigh = 0, metalLow = 0, metalLow2 = 0;
    float compressionMakeupGain = 1.41254f;
    juce::SmoothedValue<float> roomDry;
    // Filter coefficients are recomputed only when their smoothed cutoff moves.
    float tightCoeffHz = -1, tightCoeff = 0, highCoeffHz = -1, highCoeff = 0, cleanDriveGain = -1, cleanDrive = 1;
    std::array<std::array<float, 2>, 2> highCutState {};
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay;
    juce::dsp::Reverb reverb;
    juce::dsp::Limiter<float> limiter;
    ToneStack tone;
    double rate = 48000;
    int maxBlock = 256; // Internal fixed processing quantum, including before device preparation.
    int hostBlock = 512;
    std::atomic<float> inputPeak {0}, outputPeak {0};
    std::atomic<float> prePedalPeak {0}, postPedalPeak {0}, postAmpPeak {0}, postCabPeak {0};
    std::atomic<float> postEqPeak {0};
    std::atomic<bool> reportedHum {false};
    std::atomic<float> reportedMains {60};
    std::atomic<float> guitarPower {0}, inputPower {0};
    std::atomic<int> swapBypasses {0};
    std::atomic<float> gateLevel {0};
    juce::AudioProcessLoadMeasurer processLoad;
    int clipHoldSamples = 0;
    int outputLimitHold = 0;
    std::atomic<bool> outputPeakWarning {false};
    std::atomic<bool> inputClipped {false};
    std::atomic<double> reportedRate {48000}, ampExpectedRate {0}, pedalExpectedRate {0};
    std::atomic<int> reportedBlock {512};
    std::atomic<bool> tunerRequested {false}, speakerActive {false}, fallbackActive {false}, metronomeFollowsHost {false};
    std::atomic<double> metronomeBpm {120};
    // Loaded capture details for the editor; guarded by requestLock.
    juce::String ampGear; double ampLevelDb = 0; bool ampLevelled = false, ampHasCab = false, ampCabKnown = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpSuiteAudioProcessor)
};
