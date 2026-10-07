#pragma once
#include "PracticeEngine.h"
#include <juce_cryptography/juce_cryptography.h>
#include <functional>

// Catalog and export work is confined to a worker. Entries reference user take
// directories; editing metadata never changes the original recordings.
class TakeLibrary final : private juce::Thread
{
public:
    TakeLibrary(juce::File catalogFile, PracticeEngine& reviewPlayer);
    ~TakeLibrary() override;
    void importFolder(const juce::File&);
    juce::String edit(const juce::String& id, const juce::String& name, bool favorite);
    juce::String annotate(const juce::String& id, const juce::String& notes);
    juce::String renameVersion(const juce::String& id, const juce::String& version, const juce::String& name);
    juce::String preview(const juce::String& id, const juce::String& version);
    juce::String reviewControl(const juce::String& id, const juce::String& version, const juce::String& command, double amount);
    juce::var reviewWaveform(const juce::String& id, const juce::String& version);
    // File reads finish on the take worker; the caller chooses its callback thread.
    juce::String readRigSnapshot(const juce::String& id, const juce::String& version, std::function<void(juce::var)>);
    juce::String reamp(const juce::String& id, const juce::var& rig, double tailSeconds = 0);
    juce::String videoExport(const juce::String& id, const juce::String& version, const juce::File& destination, bool backing, float guitarDb, float backingDb, double startSeconds = 0, double endSeconds = -1, double fadeSeconds = 0);
    void stopReview();
    void cancelExport() { cancelled.store(true); }
    juce::var list();
    juce::var status();
    juce::String reveal(const juce::String& id);
    juce::String revealExport();
private:
    struct Job { juce::String type, id, name, version, notes; juce::File folder; bool favorite = false, backing = false; float guitarDb = 0, backingDb = 0; double tailSeconds = 0, startSeconds = 0, endSeconds = -1, fadeSeconds = 0; juce::var rig; unsigned previewGeneration = 0; std::function<void(juce::var)> completed; };
    juce::var loadRigSnapshot(const Job&);
    void run() override;
    void importTake(const juce::File&);
    void exportReamp(const Job&);
    void exportVideoAudio(const Job&);
    void playReview(const Job&);
    void persist(const juce::String& changedId);
    juce::ValueTree find(const juce::String& id);
    juce::CriticalSection lock;
    juce::File catalogFile;
    juce::ValueTree entries {"TAKES"};
    std::vector<Job> jobs;
    PracticeEngine& review;
    juce::String error, activeId, reviewId, reviewVersion, lastExportPath;
    bool reviewLoading = false;
    std::atomic<bool> exporting {false}, cancelled {false};
    std::atomic<bool> snapshotPending {false};
    std::atomic<unsigned> reviewGeneration {0}, revision {0};
    std::atomic<double> progress {0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TakeLibrary)
};
