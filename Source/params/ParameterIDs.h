#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
namespace Params
{
struct Definition { const char* id; const char* name; float min, max, initial; const char* unit; };
inline constexpr std::array definitions {
    Definition { "INPUT_GAIN", "Input", -24, 24, 0, "dB" },
    Definition { "GATE_THRESH", "Gate", -80, 0, -60, "dB" },
    Definition { "DRIVE_GAIN", "Drive", 0, 24, 0, "dB" },
    Definition { "AMP_BASS", "Bass", -12, 12, 0, "dB" },
    Definition { "AMP_MID", "Middle", -12, 12, 0, "dB" },
    Definition { "AMP_TREBLE", "Treble", -12, 12, 0, "dB" },
    Definition { "AMP_OUT", "Amp output", -24, 12, 0, "dB" },
    Definition { "DELAY_TIME", "Delay time", 40, 1000, 320, "ms" },
    Definition { "DELAY_MIX", "Delay mix", 0, 100, 0, "%" },
    Definition { "REVERB_MIX", "Reverb", 0, 100, 12, "%" },
    Definition { "MASTER_VOL", "Master", -60, 6, -12, "dB" },
    Definition { "AMP_CLEAN", "Clean channel", 0, 1, 0, "" },
    Definition { "TIGHT", "Tight", 20, 180, 20, "Hz" },
    Definition { "PRESENCE", "Presence", -6, 6, 0, "dB" },
    Definition { "CLEAN_COMP", "Clean compression", 0, 100, 35, "%" },
    Definition { "HIGH_CUT", "High cut", 3000, 20000, 20000, "Hz" },
    Definition { "DELAY_WIDTH", "Delay width", 0, 100, 0, "%" },
    Definition { "REVERB_SIZE", "Reverb size", 0, 100, 60, "%" },
    Definition { "GATE_ON", "Noise gate enabled", 0, 1, 1, "" },
    Definition { "GATE_RELEASE", "Gate release", 40, 500, 140, "ms" },
    Definition { "PEDAL_ON", "Drive pedal", 0, 1, 0, "" },
    Definition { "DYN_RES_ON", "Dynamic resonance", 0, 1, 0, "" },
    Definition { "DYN_RES_AMOUNT", "Chug cut", 0, 100, 50, "%" },
    Definition { "CHUG_ATTACK", "Chug attack", 0, 100, 0, "%" },
    Definition { "THICKEN_ON", "Sub-synthesis", 0, 1, 0, "" },
    Definition { "THICKEN_MIX", "Sub mix", 0, 100, 30, "%" },
    Definition { "PIEZO_ON", "Piezo resonator", 0, 1, 0, "" },
    Definition { "PIEZO_BLEND", "Piezo sparkle", 0, 100, 40, "%" },
    Definition { "MICRO_DELAY", "Micro-delay", 0, 1.0f, 0.0f, "ms" }
};
enum Index { input, gate, drive, bass, mid, treble, ampOut, delayTime, delayMix, reverbMix, master, clean, tight,
             presence, cleanComp, highCut, delayWidth, reverbSize, gateOn, gateRelease, pedalOn,
             dynResOn, dynResAmount, chugAttack, thickenOn, thickenMix, piezoOn, piezoBlend, microDelay };
inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (const auto& p : definitions)
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {p.id, 1}, p.name,
            juce::NormalisableRange<float> {p.min, p.max, 0.01f}, p.initial,
            juce::AudioParameterFloatAttributes().withLabel(p.unit)));
    return result;
}
}
