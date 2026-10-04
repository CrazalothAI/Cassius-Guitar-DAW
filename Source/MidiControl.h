#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Audio-thread input has a fixed SPSC queue. Mapping, host notifications and
// rig preparation requests run on a dedicated control worker, even with no editor.
class MidiControl final : private juce::Thread
{
public:
    struct Mapping { juce::String type = "cc", action = "none", rig; int channel = 0, number = 16; bool inverted = false; };
    using Action = std::function<juce::String(const Mapping&, int)>;
    MidiControl();
    ~MidiControl() override { shutdown(); }
    void start(Action);
    void shutdown();
    void receive(const juce::MidiBuffer&);
    void enable(bool);
    juce::String learn(int slot);
    juce::String setMapping(int slot, const juce::var&);
    juce::var configuration();
    juce::String restore(const juce::var&);
    juce::var status();
private:
    struct Event { unsigned epoch = 0, packed = 0; };
    static juce::var describe(const Mapping&);
    static juce::String parse(const juce::var&, Mapping&);
    static bool expression(const juce::String&);
    static bool conflicts(const std::array<Mapping, 8>&);
    void run() override;
    juce::CriticalSection lock;
    std::array<Mapping, 8> mappings;
    std::array<Event, 128> events {};
    std::array<bool, 16 * 128> pressed {}; // only touched by the audio producer
    juce::AbstractFifo fifo {128};
    std::atomic<bool> enabled {false};
    std::atomic<int> learning {-1};
    std::atomic<unsigned> epoch {1}, lastInput {0}, dropped {0}, revision {0};
    juce::String error, lastAction;
    Action action;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiControl)
};
