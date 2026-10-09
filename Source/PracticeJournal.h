#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <functional>

// Standalone-only local planner. Every document read/write runs on this worker;
// the audio callback and DAW state never reference the journal.
class PracticeJournal final : private juce::Thread
{
public:
    explicit PracticeJournal(juce::File document, std::function<double()> clock = [] { return juce::Time::getMillisecondCounterHiRes() / 1000; });
    ~PracticeJournal() override;
    juce::String command(const juce::String&, const juce::var& = {}, juce::File transfer = {});
    juce::var status(); // Small polling summary; full lists are fetched on revision changes.
    juce::var document();
    static juce::var readDocument(const juce::File&);
private:
    void run() override;
    void publish(const juce::var&, const juce::String& active, double since, bool running);
    static void validate(const juce::var&);
    static void write(const juce::File&, const juce::var&);
    juce::File file;
    std::function<double()> now;
    juce::CriticalSection mutex;
    juce::var view;
    juce::String pendingAction, error, activeId;
    juce::var pendingArguments;
    juce::File pendingTransfer;
    double started = 0;
    bool busy = true, writable = false, running = false;
    int revision = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PracticeJournal)
};
