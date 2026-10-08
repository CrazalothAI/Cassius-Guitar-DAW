#include "TakeRecovery.h"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const juce::String& reason) { if (!ok) throw std::runtime_error(reason.toStdString()); }
void check(const std::atomic<bool>& cancelled) { require(!cancelled.load(), "Recording recovery cancelled; original files are unchanged."); }
struct Guard {
    juce::InterProcessLock mutex;
    explicit Guard(const juce::File& folder) : mutex(TakeRecovery::lockName(folder)) { require(mutex.enter(0), "This take is still recording or being recovered. Stop it first."); }
    ~Guard() { mutex.exit(); }
};
class HashInput final : public juce::InputStream {
public:
    HashInput(juce::FileInputStream& stream, const std::atomic<bool>& flag) : input(stream), cancelled(flag) {}
    juce::int64 getTotalLength() override { return input.getTotalLength(); }
    juce::int64 getPosition() override { return input.getPosition(); }
    bool setPosition(juce::int64 p) override { return input.setPosition(p); }
    bool isExhausted() override { return input.isExhausted(); }
    int read(void* out, int bytes) override { check(cancelled); return input.read(out, juce::jmin(bytes, 65536)); }
private:
    juce::FileInputStream& input; const std::atomic<bool>& cancelled;
};
// JUCE readers can zero-fill truncated payloads. Check declared RIFF bounds
// separately, allowing only a readable prefix and ignoring unchecked tails.
void checkWav(const juce::File& file, const juce::AudioFormatReader& reader) {
    auto input = file.createInputStream(); require(input && input->getTotalLength() >= 44, "Recording WAV is missing or truncated.");
    const auto size = input->getTotalLength();
    require(input->readInt() == 0x46464952, "Recovery currently supports checkpointed RIFF WAV files, not RF64 or raw header repair.");
    const auto end = static_cast<juce::int64>(static_cast<juce::uint32>(input->readInt())) + 8;
    require(input->readInt() == 0x45564157 && end >= 44 && end <= size, "Recording WAV declares bytes that are not present.");
    bool format = false;
    for (int n = 0; n < 64 && input->getPosition() <= end - 8; ++n) {
        const auto kind = input->readInt(); const auto bytes = static_cast<juce::int64>(static_cast<juce::uint32>(input->readInt()));
        const auto position = input->getPosition(); require(bytes <= end - position, "Recording WAV chunk is truncated.");
        if (kind == 0x20746d66) {
            require(!format && bytes >= 16 && bytes <= 1024, "Unsupported recording WAV format.");
            const auto tag = static_cast<juce::uint16>(input->readShort());
            require(tag == 1 || tag == 3 || tag == 0xfffe, "Recovery requires uncompressed recording WAV files."); format = true;
        }
        if (kind == 0x61746164) {
            const auto frameBytes = static_cast<juce::int64>(reader.numChannels) * reader.bitsPerSample / 8;
            require(format && frameBytes > 0 && bytes > 0 && bytes % frameBytes == 0 && bytes / frameBytes == reader.lengthInSamples, "No complete readable recording frames were found.");
            require(input->getStatus().wasOk(), "Recording WAV header could not be read."); return;
        }
        const auto next = position + bytes + (bytes & 1);
        require(next <= end && next <= 1024 * 1024 && input->setPosition(next), "Recording WAV header is unsupported or too large.");
    }
    require(false, "No readable recording data chunk was found.");
}
std::unique_ptr<juce::AudioFormatReader> open(juce::AudioFormatManager& formats, const juce::File& file, unsigned channels) {
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    require(reader && reader->numChannels == channels && reader->lengthInSamples > 0 && std::isfinite(reader->sampleRate) && reader->sampleRate >= 8000 && reader->sampleRate <= 384000 && (reader->bitsPerSample == 8 || reader->bitsPerSample == 16 || reader->bitsPerSample == 24 || reader->bitsPerSample == 32), "Choose a take with readable mono dry and stereo processed WAV stems.");
    checkWav(file,*reader); return reader;
}
void copyAudio(juce::AudioFormatReader& reader, const juce::File& target, juce::int64 frames, const std::atomic<bool>& cancelled, std::function<void(double)> progress) {
    juce::WavAudioFormat wav; auto stream = target.createOutputStream(); require(stream != nullptr, "Cannot create recovered recording.");
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),reader.sampleRate,reader.numChannels,32,{},0));
    require(writer != nullptr, "Cannot open recovered recording writer."); stream.release();
    juce::AudioBuffer<float> buffer(static_cast<int>(reader.numChannels),4096);
    for (juce::int64 position = 0; position < frames;) {
        check(cancelled); const auto count = static_cast<int>(juce::jmin<juce::int64>(4096,frames-position));
        require(reader.read(&buffer,0,count,position,true,true), "Recording recovery could not read source audio.");
        for (int c=0;c<buffer.getNumChannels();++c) for (int i=0;i<count;++i) require(std::isfinite(buffer.getSample(c,i)), "Recording contains non-finite samples; it was not recovered.");
        require(writer->writeFromAudioSampleBuffer(buffer,0,count), "Cannot write recovered recording."); position += count;
        if (progress) progress(static_cast<double>(position)/static_cast<double>(frames));
    }
    require(writer->flush(), "Cannot finalize recovered recording."); writer.reset();
}
}
juce::String TakeRecovery::lockName(const juce::File& folder) {
    const auto path = folder.getFullPathName().toLowerCase(); return "CassianRecording-" + juce::SHA256(path.toRawUTF8(),static_cast<size_t>(path.getNumBytesAsUTF8())).toHexString();
}
void TakeRecovery::writeMetadata(const juce::File& file, const juce::var& value) {
    juce::TemporaryFile temporary(file);
    require(temporary.getFile().replaceWithText(juce::JSON::toString(value)) && temporary.overwriteTargetFileWithTemporary(), "Cannot save recording metadata; existing metadata was preserved.");
}
juce::String TakeRecovery::digest(const juce::File& file, const std::atomic<bool>& cancelled) {
    check(cancelled); auto input = file.createInputStream(); require(input != nullptr, "Cannot read recording for verification.");
    const auto bytes = input->getTotalLength(); HashInput wrapped(*input,cancelled); const auto hash = juce::SHA256(wrapped).toHexString();
    require(input->getPosition() == bytes && input->getTotalLength() == bytes && input->getStatus().wasOk(), "Recording changed or could not be read during verification."); check(cancelled); return hash;
}
TakeRecovery::Result TakeRecovery::recover(const juce::File& source, const juce::File& destinationRoot, const std::atomic<bool>& cancelled, std::function<void(double)> progress) {
    require(source.isDirectory() && destinationRoot != juce::File() && source != destinationRoot && !destinationRoot.isAChildOf(source), "Choose a source take separate from recovery storage.");
    check(cancelled); Guard guard(source); juce::AudioFormatManager formats; formats.registerBasicFormats();
    const auto dryFile=source.getChildFile("Guitar dry.wav"), wetFile=source.getChildFile("Guitar processed.wav");
    auto dry=open(formats,dryFile,1), wet=open(formats,wetFile,2); require(dry->sampleRate == wet->sampleRate, "Recording stems have different sample rates; they cannot be aligned safely.");
    const auto frames=juce::jmin(dry->lengthInSamples,wet->lengthInSamples); require(frames*8.0 < 2146435072.0, "Recovered recording is too large for this bounded WAV recovery workflow.");
    struct Watch { juce::File file; juce::String hash; };
    std::vector<Watch> watches; const auto watch=[&](const juce::File& file) { watches.push_back({file,digest(file,cancelled)}); };
    watch(dryFile); watch(wetFile);
    Result result; result.frames=frames; result.sampleRate=dry->sampleRate;
    if (dry->lengthInSamples != wet->lengthInSamples) result.warning="Unequal stems were trimmed to their common readable prefix. ";
    std::unique_ptr<juce::AudioFormatReader> backing; const auto backingFile=source.getChildFile("Backing track.wav");
    if (backingFile.existsAsFile()) {
        try { backing=open(formats,backingFile,2); require(backing->sampleRate==dry->sampleRate && backing->lengthInSamples>=frames,"Backing stem is shorter or has a different sample rate."); watch(backingFile); }
        catch (const std::exception& e) { check(cancelled); backing.reset(); result.warning += "Backing was omitted: " + juce::String(e.what()) + " "; }
    }
    require(destinationRoot.createDirectory().wasOk(), "Cannot create recording recovery storage.");
    require(destinationRoot.getBytesFreeOnVolume() > static_cast<double>(frames)*(backing ? 20.0 : 12.0)+4*1024*1024,"Not enough free space for a separate recovered copy.");
    const auto identity=juce::Uuid().toString();
    const auto folder=destinationRoot.getChildFile(".recording-recovery-" + identity), published=destinationRoot.getChildFile("Recovered take " + identity);
    require(!folder.exists() && folder.createDirectory().wasOk(), "Cannot create a new recovery folder.");
    struct Cleanup { juce::File folder,root; bool keep=false; ~Cleanup() { if (!keep && folder.isAChildOf(root)) folder.deleteRecursively(); } } cleanup {folder,destinationRoot};
    const auto tick=[&](double offset,double amount) { return [&,offset,amount](double p) { if (progress) progress(offset+amount*p); }; };
    copyAudio(*dry,folder.getChildFile("Guitar dry.wav"),frames,cancelled,tick(0,.3));
    copyAudio(*wet,folder.getChildFile("Guitar processed.wav"),frames,cancelled,tick(.3,.3));
    if (backing) copyAudio(*backing,folder.getChildFile("Backing track.wav"),frames,cancelled,tick(.6,.2));
    const auto snapshot=source.getChildFile("Original rig.json");
    if (snapshot.existsAsFile()) {
        if (snapshot.getSize()<=4*1024*1024) { watch(snapshot); require(snapshot.copyFileTo(folder.getChildFile("Original rig.json")),"Cannot preserve the recorded rig snapshot."); }
        else result.warning += "Oversized rig snapshot was omitted. ";
    }
    for (const auto& observed:watches) require(digest(observed.file,cancelled)==observed.hash,"Source take changed during recovery. Stop any writer and try again; originals are unchanged.");
    auto metadata=std::make_unique<juce::DynamicObject>(); metadata->setProperty("schema",1); metadata->setProperty("name",(source.getFileName().substring(0,65)+" (recovered)").substring(0,80));
    metadata->setProperty("created",juce::Time::getCurrentTime().toISO8601(true)); metadata->setProperty("frames",frames); metadata->setProperty("sampleRate",dry->sampleRate);
    metadata->setProperty("incomplete",true); metadata->setProperty("recovered",true); metadata->setProperty("recordingState","recovered");
    metadata->setProperty("recoveryDrySha256",digest(folder.getChildFile("Guitar dry.wav"),cancelled)); metadata->setProperty("recoveryWetSha256",digest(folder.getChildFile("Guitar processed.wav"),cancelled));
    if (backing) metadata->setProperty("recoveryBackingSha256",digest(folder.getChildFile("Backing track.wav"),cancelled));
    writeMetadata(folder.getChildFile("Cassian take.json"),juce::var(metadata.release()));
    auto report=std::make_unique<juce::DynamicObject>(); report->setProperty("format","Cassian recording recovery"); report->setProperty("schema",1); report->setProperty("appVersion",JucePlugin_VersionString);
    report->setProperty("source",source.getFullPathName()); report->setProperty("frames",frames); report->setProperty("sampleRate",result.sampleRate); report->setProperty("backingIncluded",backing!=nullptr); report->setProperty("warning",result.warning);
    report->setProperty("limit","Only the common readable checkpoint prefix was copied. Uncheckpointed tails and damaged headers were not repaired. Listen before confirming this take.");
    writeMetadata(folder.getChildFile("Recording recovery report.json"),juce::var(report.release())); check(cancelled);
    require(!published.exists() && folder.moveFileTo(published),"Cannot publish the recovered recording folder.");
    cleanup.keep=true; result.folder=published; if(progress) progress(1); return result;
}
void TakeRecovery::validateReviewed(const juce::File& folder, juce::int64 frames, double rate, const juce::String& dryHash, const juce::String& wetHash, const std::atomic<bool>& cancelled, const juce::String& backingHash) {
    Guard guard(folder); juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto dry=open(formats,folder.getChildFile("Guitar dry.wav"),1), wet=open(formats,folder.getChildFile("Guitar processed.wav"),2);
    require(dry->lengthInSamples==frames && wet->lengthInSamples==frames && dry->sampleRate==rate && wet->sampleRate==rate,"Recovered stems changed. Recover the original folder again.");
    require(dryHash.length()==64 && wetHash.length()==64 && digest(folder.getChildFile("Guitar dry.wav"),cancelled)==dryHash && digest(folder.getChildFile("Guitar processed.wav"),cancelled)==wetHash,"Recovered audio changed since recovery. It was not approved for export.");
    if (backingHash.isNotEmpty()) {
        auto backing=open(formats,folder.getChildFile("Backing track.wav"),2);
        require(backing->lengthInSamples==frames && backing->sampleRate==rate && backingHash.length()==64 && digest(folder.getChildFile("Backing track.wav"),cancelled)==backingHash,"Recovered backing changed since recovery. It was not approved for export.");
    }
}
