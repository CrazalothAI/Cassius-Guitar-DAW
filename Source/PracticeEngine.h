#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include "PracticeSections.h"

// One audio producer, one disk consumer. No decoding, file writes, allocation,
// mutex acquisition or ownership destruction happens in process().
class PracticeEngine final : private juce::Thread
{
public:
    explicit PracticeEngine(int fifoFrames = 262144, juce::File sectionsDirectory = {});
    ~PracticeEngine() override;
    void prepare(double sampleRate);
    void load(const juce::File&);
    juce::String command(const juce::String&, double amount = 0);
    juce::String record(const juce::File& parent, const juce::var& rig = {});
    std::function<void(const juce::File&)> onTakeFinished;
    bool isPlaying() const { return playing.load(); }
    bool transportActive() const { return playing.load() || countActive.load() || startRequested.load(); }
    void setCountIn(int bars, double bpm, int beats);
    bool process(juce::AudioBuffer<float>& guitarAndOutput, const float* dry, bool guitarAvailable = true);
    juce::var status();
    juce::var waveform();
    juce::String saveSection(const juce::String& name, const juce::String& id = {});
    juce::String recallSection(const juce::String& id);
    juce::String removeSection(const juce::String& id);
    bool counting() const { return countActive.load(); }
    void interrupted() { if (recordMode.load() >= 2 && recordMode.load() <= 3) { startRequested.store(false); countActive.store(false); recordingFault.store(2); recordMode.store(4); } }
private:
    struct Track { juce::AudioBuffer<float> audio; double rate = 48000, speed = 1, duration = 0; juce::String name; };
    void run() override;
    void readTrack(const juce::File&, unsigned generation, double speed, bool preservePosition);
    bool cancelled(unsigned generation);
    void beginRecording(const juce::File&);
    void drainRecording();
    void finishTake();
    void reclaimTracks();
    void startCount();
    bool sectionsBlocked() const;
    PracticeSections sections;
    juce::var wavePeaks, sectionRows {juce::Array<juce::var>()};
    juce::String trackKey, sectionError;
    int waveRevision = 0, sectionRevision = 0;
    juce::CriticalSection control;
    juce::File pendingTrack, pendingRecording, loadedFile;
    juce::File activeTake;
    juce::String pendingRigJson, activeRigJson;
    bool trackPending = false, recordPending = false;
    bool preserveTrackPosition = false;
    std::atomic<bool> loadingTrack {false};
    unsigned loadGeneration = 0;
    juce::String trackName, error, takePath;
    std::unique_ptr<Track> ownedTrack;
    std::vector<std::unique_ptr<Track>> retired;
    std::atomic<Track*> track {nullptr}, hazard {nullptr};
    std::atomic<bool> playing {false}, countActive {false}, startRequested {false};
    std::atomic<double> rate {48000}, duration {0}, reportedPosition {0}, seek {-1};
    std::atomic<float> levelDb {-12};
    std::atomic<double> requestedSpeed {1}, playbackSpeed {1}, loadProgress {0};
    std::atomic<double> loopFadeMs {5};
    std::atomic<bool> loop {false};
    std::atomic<double> loopA {0}, loopB {0};
    std::atomic<int> bars {0}, beats {4}, countBeat {0};
    std::atomic<double> bpm {120};
    std::atomic<unsigned> startEpoch {0};
    unsigned audioEpoch = 0;
    double position = 0, countSamples = 0, countLength = 0, beatSamples = 24000, clickAge = 1e9;
    juce::SmoothedValue<float> gain;
    // 0 ready, 1 preparing, 2 armed/count-in, 3 recording, 4 draining.
    std::atomic<int> recordMode {0}, callbacks {0};
    std::atomic<juce::int64> recordedFrames {0};
    std::atomic<int> recordingFault {0};
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> recordingAudio;
    std::unique_ptr<juce::AudioFormatWriter> dryWriter, wetWriter;
    std::atomic<double> recordingRate {48000};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PracticeEngine)
};
