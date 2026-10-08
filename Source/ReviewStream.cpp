#include "ReviewStream.h"
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace {
void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
class Decoder final : public juce::AudioSource {
public:
    Decoder(juce::AudioFormatReader& r, juce::int64 start) : reader(r), position(start) {}
    void prepareToPlay(int, double) override {}
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        info.clearActiveBufferRegion();
        const int n = static_cast<int>(juce::jlimit<juce::int64>(0, info.numSamples, reader.lengthInSamples - position));
        if (n > 0 && !reader.read(info.buffer, info.startSample, n, position, true, true)) failed = true;
        position += info.numSamples;
    }
    bool failed = false;
private:
    juce::AudioFormatReader& reader; juce::int64 position;
};
}
ReviewStream::ReviewStream(const juce::File& f, std::unique_ptr<juce::AudioFormatReader> r, double rate)
    : file(f), fileSize(f.getSize()), frames(static_cast<juce::int64>(std::ceil(r->lengthInSamples * rate / r->sampleRate))),
      modified(f.getLastModificationTime()), reader(std::move(r)), targetRate(rate) {}
ReviewStream::~ReviewStream() = default;
bool ReviewStream::unchanged() const { return file.existsAsFile() && file.getSize() == fileSize && file.getLastModificationTime() == modified; }
void ReviewStream::fail(const juce::String& text) { failureText = text; failed.store(true); buffering.store(false); }
void ReviewStream::validate() {
    require(reader->sampleRate >= 8000 && reader->sampleRate <= 384000 && targetRate >= 8000 && targetRate <= 384000
        && std::floor(reader->sampleRate) == reader->sampleRate && std::floor(targetRate) == targetRate,
        "Long-take review requires whole sample rates from 8 to 384 kHz.");
    require(reader->numChannels >= 1 && reader->numChannels <= 2 && frames > 0, "Long-take review requires mono or stereo WAV audio.");
    auto input = file.createInputStream(); require(input && input->getTotalLength() >= 44, "Take WAV is missing or truncated.");
    const auto size = input->getTotalLength();
    require(input->readInt() == 0x46464952, "Long-take review currently supports standard RIFF WAV, not RF64.");
    const auto end = juce::int64(static_cast<juce::uint32>(input->readInt())) + 8;
    require(input->readInt() == 0x45564157 && end >= 44 && end <= size, "Take WAV declares audio bytes that are missing.");
    bool format = false;
    for (int i = 0; i < 64 && input->getPosition() <= end - 8; ++i) {
        const auto kind = input->readInt(); const auto bytes = juce::int64(static_cast<juce::uint32>(input->readInt()));
        const auto position = input->getPosition(); require(bytes <= end - position, "Take WAV chunk is truncated.");
        if (kind == 0x20746d66) {
            require(!format && bytes >= 16 && bytes <= 1024, "Unsupported take WAV format.");
            const auto tag = static_cast<juce::uint16>(input->readShort());
            require(tag == 1 || tag == 3 || tag == 0xfffe, "Long-take review requires uncompressed WAV."); format = true;
        }
        if (kind == 0x61746164) {
            const auto frameBytes = juce::int64(reader->numChannels) * reader->bitsPerSample / 8;
            require(format && frameBytes > 0 && bytes > 0 && bytes % frameBytes == 0 && bytes / frameBytes == reader->lengthInSamples,
                "Take WAV has incomplete audio frames."); return;
        }
        const auto next = position + bytes + (bytes & 1);
        require(next <= end && next <= 1024 * 1024 && input->setPosition(next), "Take WAV header is unsupported or too large.");
    }
    require(false, "Take WAV has no readable audio data.");
}
juce::var ReviewStream::envelope(const std::function<bool()>& cancelled, const std::function<void(double)>& progress) {
    // Scan bounded source blocks, never a whole-take allocation. Envelope time
    // follows the file, independent of the interface's resampling ratio.
    juce::AudioBuffer<float> scratch(2, blockFrames);
    std::array<float, 512> lows {}, highs {};
    const int bins = static_cast<int>(juce::jmin<juce::int64>(512, reader->lengthInSamples));
    for (juce::int64 offset = 0; offset < reader->lengthInSamples; offset += blockFrames) {
        if (cancelled()) return {};
        const int n = static_cast<int>(juce::jmin<juce::int64>(blockFrames, reader->lengthInSamples - offset));
        require(reader->read(&scratch, 0, n, offset, true, true), "Could not read the take waveform.");
        for (int i = 0; i < n; ++i) {
            const int bin = static_cast<int>((offset + i) * bins / reader->lengthInSamples);
            for (unsigned ch = 0; ch < reader->numChannels; ++ch) {
                const float x = scratch.getSample(static_cast<int>(ch), i);
                if (std::isfinite(x)) { lows[bin] = juce::jmin(lows[bin], x); highs[bin] = juce::jmax(highs[bin], x); }
            }
        }
        progress(static_cast<double>(offset + n) / reader->lengthInSamples);
    }
    require(unchanged(), "The take changed during preparation. Reload it.");
    juce::Array<juce::var> result;
    for (int i = 0; i < bins; ++i) result.add(juce::var(juce::Array<juce::var>{lows[i], highs[i]}));
    return juce::var(result);
}
bool ReviewStream::ready(juce::int64 frame) const {
    for (const auto& b : blocks) if ((b.state.load() == Ready || b.state.load() == Reading) && frame >= b.start && frame < b.start + b.count) return true;
    return false;
}
bool ReviewStream::fill(Block& b, juce::int64 start, const std::function<bool()>& cancelled) {
    if (cancelled()) return false;
    if (!unchanged()) { fail("Take audio changed or disappeared. Reload this version before listening."); return false; }
    const auto sourceRate = static_cast<juce::int64>(reader->sampleRate), outputRate = static_cast<juce::int64>(targetRate);
    const double ratio = reader->sampleRate / targetRate;
    // JUCE uses a second-order Butterworth filter. At the supported minimum
    // ratio 1/48 its pole radius is < .955; 2048 filter-domain samples decay
    // well below float precision. Align the restart to the rational rate
    // period so the resampler's fractional phase matches decoding from zero.
    const auto period = outputRate / std::gcd(sourceRate, outputRate);
    const auto warmup = static_cast<juce::int64>(std::ceil(2048 * juce::jmax(1., 1. / ratio)));
    const auto restart = juce::jmax<juce::int64>(0, start - warmup) / period * period;
    Decoder decoder(*reader, restart * sourceRate / outputRate);
    juce::ResamplingAudioSource resampler(&decoder, false, 2);
    resampler.setResamplingRatio(ratio); resampler.prepareToPlay(1024, targetRate);
    juce::AudioBuffer<float> scratch(2, 1024);
    for (auto offset = restart; offset < start;) {
        if (cancelled()) return false;
        const int n = static_cast<int>(juce::jmin<juce::int64>(1024, start - offset));
        resampler.getNextAudioBlock({&scratch, 0, n}); offset += n;
    }
    b.start = start; b.count = static_cast<int>(juce::jmin<juce::int64>(blockFrames, frames - start));
    const int readFrames = static_cast<int>(juce::jmin<juce::int64>(b.count + 1, frames - start));
    for (int offset = 0; offset < readFrames; offset += 1024) {
        if (cancelled() || wanted.load() / blockFrames * blockFrames > start + blockFrames || wanted.load() + slotCount * blockFrames < start) return false;
        resampler.getNextAudioBlock({&b.audio, offset, juce::jmin(1024, readFrames - offset)});
    }
    if (decoder.failed || !unchanged()) { fail("Could not read this take; its audio may have changed or become unavailable. Reload it."); return false; }
    if (reader->numChannels == 1) b.audio.copyFrom(1, 0, b.audio, 0, 0, readFrames);
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < readFrames; ++i) if (!std::isfinite(b.audio.getSample(ch, i))) b.audio.setSample(ch, i, 0);
        if (readFrames == b.count) b.audio.setSample(ch, b.count, b.audio.getSample(ch, b.count - 1));
    }
    return true;
}
bool ReviewStream::service(const std::function<bool()>& cancelled) {
    if (failed.load()) return false;
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastCheck >= 250) { lastCheck = now; if (!unchanged()) { fail("Take audio changed or disappeared. Reload this version before listening."); return false; } }
    const auto base = wanted.load() / blockFrames * blockFrames;
    for (int i = 0; i < slotCount && base + juce::int64(i) * blockFrames < frames; ++i) {
        const auto start = base + juce::int64(i) * blockFrames;
        if (ready(start)) continue;
        for (auto& b : blocks) {
            int expected = b.state.load();
            if (expected == Free || (expected == Ready && (b.start < base || b.start >= base + slotCount * blockFrames))) {
                if (!b.state.compare_exchange_strong(expected, Writing)) continue;
                try { const bool ok = fill(b, start, cancelled); b.state.store(ok ? Ready : Free); return ok; }
                catch (const std::exception& e) { b.state.store(Free); fail("Take playback could not prepare audio: " + juce::String(e.what())); return false; }
            }
        }
        return false;
    }
    return false;
}
ReviewStream::Read::~Read() { if (slot >= 0) stream.blocks[slot].state.store(Ready); }
bool ReviewStream::Read::sample(juce::int64 frame, float fraction, float& left, float& right) {
    if (stream.failed.load()) { stream.buffering.store(false); return false; }
    // Once this callback misses a range, retry on the next callback rather
    // than scanning every slot for every silent sample.
    if (frame / blockFrames == missingBlock) return false;
    if (slot >= 0 && (frame < stream.blocks[slot].start || frame >= stream.blocks[slot].start + stream.blocks[slot].count)) {
        stream.blocks[slot].state.store(Ready); slot = -1;
    }
    if (slot < 0) for (int i = 0; i < slotCount; ++i) {
        auto& b = stream.blocks[i]; int expected = Ready;
        if (!b.state.compare_exchange_strong(expected, Reading)) continue;
        if (frame >= b.start && frame < b.start + b.count) { slot = i; break; }
        b.state.store(Ready);
    }
    if (slot < 0) {
        missingBlock = frame / blockFrames;
        if (!stream.buffering.exchange(true)) stream.underruns.fetch_add(1);
        return false;
    }
    stream.buffering.store(false);
    const auto& b = stream.blocks[slot]; const int index = static_cast<int>(frame - b.start);
    const auto value = [&](int ch) { const auto* x = b.audio.getReadPointer(ch); return x[index] + fraction * (x[index + 1] - x[index]); };
    left = value(0); right = value(1); return true;
}
