#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>

// One audio producer, one disk consumer. No decoding, file writes, allocation,
// mutex acquisition or ownership destruction happens in process().
class PracticeEngine final : private juce::Thread
{
public:
    explicit PracticeEngine(int fifoFrames = 262144);
    ~PracticeEngine() override;
    void prepare(double sampleRate);
    void load(const juce::File&);
    juce::String command(const juce::String&, double amount = 0);
    juce::String record(const juce::File& parent);
    void setCountIn(int bars, double bpm, int beats);
    bool process(juce::AudioBuffer<float>& guitarAndOutput, const float* dry, bool guitarAvailable = true);
    juce::var status();
    bool counting() const { return countActive.load(); }
    void interrupted() { if (recordMode.load() >= 2 && recordMode.load() <= 3) { startRequested.store(false); countActive.store(false); recordingFault.store(2); recordMode.store(4); } }
private:
    struct Track { juce::AudioBuffer<float> audio; double rate = 48000; juce::String name; };
    void run() override;
    void readTrack(const juce::File&, unsigned generation);
    void beginRecording(const juce::File&);
    void drainRecording();
    void reclaimTracks();
    void startCount();
    juce::CriticalSection control;
    juce::File pendingTrack, pendingRecording, loadedFile;
    bool trackPending = false, recordPending = false;
    std::atomic<bool> loadingTrack {false};
    unsigned loadGeneration = 0;
    juce::String trackName, error, takePath;
    std::unique_ptr<Track> ownedTrack;
    std::vector<std::unique_ptr<Track>> retired;
    std::atomic<Track*> track {nullptr}, hazard {nullptr};
    std::atomic<bool> playing {false}, countActive {false}, startRequested {false};
    std::atomic<double> rate {48000}, duration {0}, reportedPosition {0}, seek {-1};
    std::atomic<float> levelDb {-12};
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
