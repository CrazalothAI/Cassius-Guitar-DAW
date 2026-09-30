#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "params/ParameterIDs.h"
#include "dsp/NamWrapper.h"
#include "dsp/IrLoader.h"
#include "dsp/ToneStack.h"
#include "dsp/GuitarGate.h"
#include "dsp/PitchTracker.h"
#include "dsp/DynamicResonanceFilter.h"
#include "dsp/PiezoSimulator.h"
#include "dsp/SubSynthesizer.h"
#include "dsp/MicroDelay.h"
#include "dsp/HighGainAmp.h"
#include "dsp/SpeakerCab.h"

class AmpSuiteAudioProcessor final : public juce::AudioProcessor, private juce::Thread
{
public:
    AmpSuiteAudioProcessor();
    ~AmpSuiteAudioProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Cassian"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void requestFile(bool model, const juce::File&);
    void requestPedal(const juce::File&);
    bool selectAmpVoice(const juce::String&);
    // The pitch tracker only runs while the tuner is open (or Thicken needs it).
    void setTunerActive(bool shouldRun) { tunerRequested.store(shouldRun); }
    juce::var status();
    juce::AudioProcessorValueTreeState apvts;
private:
    void run() override;
    float value(Params::Index i) const { return parameters[static_cast<size_t>(i)]->load(); }
    void processChunk(juce::AudioBuffer<float>&);
    std::array<std::atomic<float>*, Params::definitions.size()> parameters {};
    juce::CriticalSection dspLock, requestLock;
    juce::String desiredModel, desiredIr, modelPath, irPath, message = "Load an amp capture to get started";
    bool modelPending = false, irPending = false;
    juce::String desiredPedal, pedalPath;
    bool pedalPending = false;
    std::unique_ptr<NamWrapper> model;
    std::unique_ptr<NamWrapper> pedal;
    IrLoader cab;
    GuitarGate gate;
    PitchTracker pitchTracker;
    DynamicResonanceFilter dynamicResonance;
    PiezoSimulator piezo;
    SubSynthesizer subSynth;
    MicroDelay microDelay;
    HighGainAmp fallbackAmp;
    SpeakerCab speaker;
    juce::dsp::Gain<float> inputGain, ampGain, masterGain;
    juce::SmoothedValue<float> driveGain, delayTime, delayMix;
    juce::SmoothedValue<float> cleanBlend, tightCutoff;
    juce::SmoothedValue<float> compressionMix, highCutoff, stereoWidth;
    juce::dsp::Compressor<float> cleanCompressor;
    std::vector<float> cleanAudio, gateEnvelope, pedalAudio, subSynthAudio;
    juce::SmoothedValue<float> pedalBlend;
    float cleanLow = 0, cleanHigh = 0, metalLow = 0, metalLow2 = 0;
    // Filter coefficients are recomputed only when their smoothed cutoff moves.
    float tightCoeffHz = -1, tightCoeff = 0, highCoeffHz = -1, highCoeff = 0, cleanDriveGain = -1, cleanDrive = 1;
    std::array<std::array<float, 2>, 2> highCutState {};
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay;
    juce::dsp::Reverb reverb;
    juce::dsp::Limiter<float> limiter;
    ToneStack tone;
    double rate = 48000;
    int maxBlock = 512; // Internal fixed processing quantum.
    int hostBlock = 512;
    std::atomic<float> inputPeak {0}, outputPeak {0};
    std::atomic<float> prePedalPeak {0}, postPedalPeak {0}, postAmpPeak {0}, postCabPeak {0};
    std::atomic<int> swapBypasses {0};
    std::atomic<float> gateLevel {0};
    juce::AudioProcessLoadMeasurer processLoad;
    int clipHoldSamples = 0;
    std::atomic<bool> inputClipped {false};
    std::atomic<double> reportedRate {48000}, ampExpectedRate {0}, pedalExpectedRate {0};
    std::atomic<int> reportedBlock {512};
    std::atomic<bool> tunerRequested {false}, speakerActive {false}, fallbackActive {false};
    // Loaded capture details for the editor; guarded by requestLock.
    juce::String ampGear; double ampLevelDb = 0; bool ampLevelled = false, ampHasCab = false, ampCabKnown = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpSuiteAudioProcessor)
};
