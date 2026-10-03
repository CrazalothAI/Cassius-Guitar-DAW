#pragma once
#include <juce_dsp/juce_dsp.h>
class IrLoader
{
public:
    ~IrLoader() { delete incoming.exchange(nullptr); collect(); }
    // Read/allocate on the loader thread. Only the processing thread calls the
    // convolution load API; JUCE requires it to be serialized with process().
    void load(juce::AudioBuffer<float>&& impulse, double sampleRate)
    {
        collect();
        auto* next = new Pending {std::move(impulse), sampleRate, nullptr};
        delete incoming.exchange(next);
        loaded.store(true);
    }
    void prepare(const juce::dsp::ProcessSpec& spec) { acceptIncoming(); convolution.prepare(spec); }
    void process(juce::dsp::ProcessContextReplacing<float>& context)
    {
        acceptIncoming();
        if (loaded.load()) convolution.process(context);
    }
    void clear() { loaded.store(false); }
    bool isLoaded() const { return loaded.load(); }
private:
    void acceptIncoming()
    {
        if (auto* next = incoming.exchange(nullptr)) {
            convolution.loadImpulseResponse(std::move(next->impulse), next->rate, juce::dsp::Convolution::Stereo::yes,
                juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::yes);
            auto* head = retired.load();
            do { next->next = head; } while (!retired.compare_exchange_weak(head, next));
        }
    }
    struct Pending { juce::AudioBuffer<float> impulse; double rate; Pending* next; };
    void collect() { auto* item = retired.exchange(nullptr); while (item) { auto* next = item->next; delete item; item = next; } }
    std::atomic<Pending*> incoming {nullptr}, retired {nullptr};
    juce::dsp::Convolution convolution;
    std::atomic<bool> loaded {false};
};
