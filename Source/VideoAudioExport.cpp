#include "TakeLibrary.h"
#include <cmath>

juce::String TakeLibrary::revealExport() {
    juce::String path; { const juce::ScopedLock guard(lock); path = lastExportPath; }
    if (!juce::File::isAbsolutePath(path) || !juce::File(path).existsAsFile()) return "Exported audio is missing.";
    juce::File(path).revealToUser(); return {};
}

juce::String TakeLibrary::videoExport(const juce::String& id, const juce::String& version, const juce::File& destination, bool backing, float guitarDb, float backingDb) {
    const juce::ScopedLock guard(lock); const auto take = find(id);
    if (!take.isValid() || static_cast<bool>(take["incomplete"])) return "Choose a complete take to export.";
    if (version != "processed" && version != "dry" && !take.getChildWithProperty("id", version).isValid()) return "Take version not found.";
    if (!std::isfinite(guitarDb) || !std::isfinite(backingDb) || guitarDb < -60 || guitarDb > 12 || backingDb < -60 || backingDb > 12) return "Invalid export balance.";
    if (!destination.hasFileExtension("wav") || destination.exists()) return "Choose a new WAV filename; original files are never overwritten.";
    if (backing && !static_cast<bool>(take["hasBacking"])) return "This older take has no recorded backing stem. Export guitar only.";
    if (exporting.exchange(true)) return "An export is already running.";
    cancelled.store(false); progress.store(0); activeId = id; error.clear();
    Job job; job.type = "video"; job.id = id; job.version = version; job.folder = destination; job.backing = backing; job.guitarDb = guitarDb; job.backingDb = backingDb;
    jobs.push_back(std::move(job)); notify(); return {};
}

void TakeLibrary::exportVideoAudio(const Job& job) {
    const auto require = [](bool ok, const juce::String& text) { if (!ok) throw std::runtime_error(text.toStdString()); };
    juce::File guitarFile, backingFile;
    { const juce::ScopedLock guard(lock); const auto take = find(job.id); require(take.isValid(), "Take not found."); const juce::File folder(take["path"].toString());
      guitarFile = job.version == "processed" ? folder.getChildFile("Guitar processed.wav") : job.version == "dry" ? folder.getChildFile("Guitar dry.wav") : juce::File(take.getChildWithProperty("id", job.version)["path"].toString());
      backingFile = folder.getChildFile("Backing track.wav"); }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> guitar(formats.createReaderFor(guitarFile)), backing;
    require(guitar && guitar->numChannels >= 1 && guitar->numChannels <= 2 && guitar->sampleRate >= 8000 && guitar->sampleRate <= 384000 && guitar->lengthInSamples > 0, "Guitar audio is missing or unsupported.");
    const auto frames = static_cast<juce::int64>(std::llround(guitar->lengthInSamples * 48000. / guitar->sampleRate));
    require(frames > 0 && frames * 6. < 4293918720., "Take is too large for a standard video WAV.");
    if (job.backing) {
        backing.reset(formats.createReaderFor(backingFile));
        require(backing && backing->numChannels == 2 && backing->sampleRate == guitar->sampleRate && backing->lengthInSamples == guitar->lengthInSamples, "Backing and guitar stems must have matching rate and length.");
    }
    juce::TemporaryFile temporary(job.folder); juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer; float peak = 0;
    // Two bounded passes preserve the requested balance and attenuate the whole
    // soundtrack only when needed for -1 dBFS sample-peak headroom.
    for (int pass = 0; pass < 2; ++pass) {
        if (cancelled.load() || threadShouldExit()) return;
        juce::AudioFormatReaderSource guitarSource(guitar.get(), false);
        juce::ResamplingAudioSource guitarRate(&guitarSource, false, 2);
        guitarRate.setResamplingRatio(guitar->sampleRate / 48000.); guitarRate.prepareToPlay(1024, 48000);
        std::unique_ptr<juce::AudioFormatReaderSource> backingSource;
        std::unique_ptr<juce::ResamplingAudioSource> backingRate;
        if (backing) { backingSource = std::make_unique<juce::AudioFormatReaderSource>(backing.get(), false); backingRate = std::make_unique<juce::ResamplingAudioSource>(backingSource.get(), false, 2); backingRate->setResamplingRatio(backing->sampleRate / 48000.); backingRate->prepareToPlay(1024, 48000); }
        const auto trim = peak > juce::Decibels::decibelsToGain(-1.f) ? juce::Decibels::decibelsToGain(-1.f) / peak : 1.f;
        if (pass == 1) { auto stream = temporary.getFile().createOutputStream(); require(stream != nullptr, "Could not create video WAV."); writer.reset(wav.createWriterFor(stream.get(), 48000, 2, 24, {}, 0)); require(writer != nullptr, "Could not open video WAV writer."); stream.release(); }
        juce::AudioBuffer<float> audio(2, 1024), bed(2, 1024);
        for (juce::int64 offset = 0; offset < frames; offset += 1024) {
            if (cancelled.load() || threadShouldExit()) return;
            const auto count = static_cast<int>(juce::jmin<juce::int64>(1024, frames - offset));
            audio.clear(); guitarRate.getNextAudioBlock({&audio, 0, count});
            if (guitar->numChannels == 1) audio.copyFrom(1, 0, audio, 0, 0, count);
            audio.applyGain(juce::Decibels::decibelsToGain(job.guitarDb));
            if (backingRate) { bed.clear(); backingRate->getNextAudioBlock({&bed, 0, count}); for (int ch = 0; ch < 2; ++ch) audio.addFrom(ch, 0, bed, ch, 0, count, juce::Decibels::decibelsToGain(job.backingDb)); }
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < count; ++i) { const auto x = audio.getSample(ch, i); require(std::isfinite(x), "Take contains invalid audio samples."); if (pass == 0) peak = juce::jmax(peak, std::abs(x)); }
            if (pass == 1) { audio.applyGain(trim); require(writer->writeFromAudioSampleBuffer(audio, 0, count), "Video WAV disk write failed."); }
            progress.store((pass + (offset + count) / static_cast<double>(frames)) * .5);
        }
    }
    writer.reset(); if (cancelled.load() || threadShouldExit()) return;
    require(!job.folder.exists() && temporary.overwriteTargetFileWithTemporary(), "Could not finalize video WAV; choose a new filename.");
    const juce::ScopedLock guard(lock); lastExportPath = job.folder.getFullPathName(); ++revision;
}
