#include "../Source/dsp/DualCab.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Render = std::vector<std::vector<float>>;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
juce::AudioBuffer<float> response(int offset = 0) {
    juce::AudioBuffer<float> audio(2, 512); audio.clear();
    for (int ch = 0; ch < 2; ++ch) {
        audio.setSample(ch, offset, ch == 0 ? .8f : .64f);
        audio.setSample(ch, offset + 13, ch == 0 ? .25f : .2f);
    }
    return audio;
}
Render render(double rate, int blockSize, int channels, DualCab::Settings settings, int offsetB = 0) {
    DualCab cab; cab.load(response(), 48000); cab.load(response(offsetB), 48000, 1);
    cab.prepare({rate, static_cast<juce::uint32>(blockSize), static_cast<juce::uint32>(channels)}, settings);
    juce::AudioBuffer<float> audio(channels, blockSize);
    // Let any loader/crossfade settle before a deterministic impulse measurement.
    for (int b = 0; b < 128; ++b) {
        audio.clear(); juce::dsp::AudioBlock<float> block(audio);
        juce::dsp::ProcessContextReplacing<float> context(block); cab.process(context);
    }
    Render result(static_cast<size_t>(channels), std::vector<float>(4096));
    for (int start = 0; start < 4096; start += blockSize) {
        const int count = juce::jmin(blockSize, 4096 - start); audio.clear();
        if (start == 0) for (int ch = 0; ch < channels; ++ch) audio.setSample(ch, 0, ch == 0 ? .25f : .125f);
        juce::dsp::AudioBlock<float> full(audio); auto block = full.getSubBlock(0, static_cast<size_t>(count));
        juce::dsp::ProcessContextReplacing<float> context(block); cab.process(context);
        for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < count; ++i) {
            const auto sample = audio.getSample(ch, i);
            require(std::isfinite(sample), "Cabinet measurement must remain finite");
            result[static_cast<size_t>(ch)][static_cast<size_t>(start + i)] = sample;
        }
    }
    return result;
}
float delayed(const std::vector<float>& samples, int index, double delay) {
    const auto whole = static_cast<int>(std::floor(delay)); const auto fraction = static_cast<float>(delay - whole);
    const auto at = [&](int i) { return i < 0 ? 0.0f : samples[static_cast<size_t>(i)]; };
    return (1 - fraction) * at(index - whole) + fraction * at(index - whole - 1);
}
double difference(const Render& a, const Render& b) {
    double worst = 0;
    for (size_t ch = 0; ch < a.size(); ++ch) for (size_t i = 0; i < a[ch].size(); ++i) worst = juce::jmax(worst, static_cast<double>(std::abs(a[ch][i] - b[ch][i])));
    return worst;
}
}

void runCabinetChecks() {
    for (double rate : {44100., 48000., 96000.}) for (int channels : {1, 2}) {
        DualCab::Settings settings; const auto reference = render(rate, 128, channels, settings);
        require(std::abs(reference[0][0]) > .001f, "Cabinet reference impulse must be audible");
        for (int blockSize : {64, 128, 257}) {
            require(difference(render(rate, blockSize, channels, settings), reference) < 2e-6, "Cabinet response must not depend on callback partition size");
            settings.second = true; settings.blend = 50;
            const auto equal = render(rate, blockSize, channels, settings);
            require(difference(equal, reference) < 2e-6, "Equal cabinet blend must preserve the single response at every rate/block size");
            // Existing JUCE Trim::yes removes leading silence independently from each IR.
            require(difference(render(rate, blockSize, channels, settings, 37), reference) < 2e-6, "Leading IR silence must retain the established trimmed-onset behavior");
            settings.invertB = true; auto cancelled = render(rate, blockSize, channels, settings);
            for (const auto& channel : cancelled) for (const auto sample : channel) require(std::abs(sample) < 2e-6, "Opposite polarity of matched cabinets must cancel without leaking dry audio");
            settings.invertB = false; settings.delayB = .375f;
            for (bool invert : {false, true}) {
                settings.invertB = invert; const auto shifted = render(rate, blockSize, channels, settings);
                double error = 0;
                for (size_t ch = 0; ch < shifted.size(); ++ch) for (size_t i = 0; i < shifted[ch].size(); ++i) {
                    const auto expected = .5f * (reference[ch][i] + (invert ? -1 : 1) * delayed(reference[ch], static_cast<int>(i), rate * .000375));
                    error = juce::jmax(error, static_cast<double>(std::abs(shifted[ch][i] - expected)));
                }
                require(error < 3e-6, "Fractional relative alignment must match the stated delay and polarity in both channels");
            }
            settings.invertB = false; settings.delayA = settings.delayB = .375f;
            const auto aligned = render(rate, blockSize, channels, settings);
            for (size_t ch = 0; ch < aligned.size(); ++ch) for (size_t i = 0; i < aligned[ch].size(); ++i)
                require(std::abs(aligned[ch][i] - delayed(reference[ch], static_cast<int>(i), rate * .000375)) < 3e-6, "Equal cabinet delays must preserve the blend and add the specified causal delay");
            settings = {};
            std::cout << "Cabinet impulse contract: " << rate << " Hz / " << blockSize << " samples / " << channels << " channels passed\n";
        }
    }
}
