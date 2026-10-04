#pragma once
#include "PracticeEngine.h"
#include <juce_cryptography/juce_cryptography.h>

// Catalog and export work is confined to a worker. Entries reference user take
// directories; editing metadata never changes the original recordings.
class TakeLibrary final : private juce::Thread
{
public:
    TakeLibrary(juce::File catalogFile, PracticeEngine& reviewPlayer);
    ~TakeLibrary() override;
    void importFolder(const juce::File&);
    juce::String edit(const juce::String& id, const juce::String& name, bool favorite);
    juce::String preview(const juce::String& id, const juce::String& version);
    juce::String reamp(const juce::String& id, const juce::var& rig);
    void stopReview();
    void cancelExport() { cancelled.store(true); }
    juce::var list();
    juce::var status();
    juce::String reveal(const juce::String& id);
private:
    struct Job { juce::String type, id, name, version; juce::File folder; bool favorite = false; juce::var rig; unsigned previewGeneration = 0; };
    void run() override;
    void importTake(const juce::File&);
    void exportReamp(const Job&);
    void playReview(const Job&);
    void persist(const juce::String& changedId);
    juce::ValueTree find(const juce::String& id);
    juce::CriticalSection lock;
    juce::File catalogFile;
    juce::ValueTree entries {"TAKES"};
    std::vector<Job> jobs;
    PracticeEngine& review;
    juce::String error, activeId, reviewId;
    std::atomic<bool> exporting {false}, cancelled {false};
    std::atomic<unsigned> reviewGeneration {0}, revision {0};
    std::atomic<double> progress {0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TakeLibrary)
};
