#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <array>
#include <functional>

// Reader/resampler ownership stays on PracticeEngine's disk thread. Slots are
// immutable while Ready/Reading; only the worker may transition to Writing.
class ReviewStream final {
public:
    static constexpr int blockFrames = 16384, slotCount = 16;
    static constexpr juce::int64 cacheBytes = juce::int64(slotCount) * 2 * (blockFrames + 1) * sizeof(float);
    ReviewStream(const juce::File&, std::unique_ptr<juce::AudioFormatReader>, double targetRate);
    ~ReviewStream();
    void validate();
    juce::var envelope(const std::function<bool()>& cancelled, const std::function<void(double)>& progress);
    bool service(const std::function<bool()>& cancelled);
    void request(juce::int64 frame) { wanted.store(juce::jlimit<juce::int64>(0, frames - 1, frame)); }
    juce::int64 frameCount() const { return frames; }
    bool ready(juce::int64 frame) const; // worker only
    bool hasFailed() const { return failed.load(); }
    juce::String failure() const { return failureText; } // worker only
    bool isBuffering() const { return buffering.load(); }
    juce::int64 underrunCount() const { return underruns.load(); }
    class Read final {
    public:
        explicit Read(ReviewStream& s) : stream(s) {}
        ~Read();
        bool sample(juce::int64 frame, float fraction, float& left, float& right);
    private:
        ReviewStream& stream; int slot = -1; juce::int64 missingBlock = -1;
    };
private:
    enum State { Free, Writing, Ready, Reading };
    struct Block {
        Block() : audio(2, blockFrames + 1) {}
        std::atomic<int> state {Free};
        juce::int64 start = 0; int count = 0;
        juce::AudioBuffer<float> audio;
    };
    bool unchanged() const;
    bool fill(Block&, juce::int64 start, const std::function<bool()>& cancelled);
    void fail(const juce::String&);
    juce::File file; juce::int64 fileSize, frames; juce::Time modified;
    std::unique_ptr<juce::AudioFormatReader> reader;
    double targetRate;
    std::array<Block, slotCount> blocks;
    std::atomic<juce::int64> wanted {0}, underruns {0};
    std::atomic<bool> failed {false}, buffering {false};
    juce::String failureText;
    juce::uint32 lastCheck = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReviewStream)
};
