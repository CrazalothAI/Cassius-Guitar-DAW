#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
namespace Params
{
// `centre` puts that value at mid-travel (0 = linear), so wide ranges spend the knob
// where it matters: High cut, for example, would otherwise give half its travel to 11.5-20 kHz.
struct Definition
{
    const char* id; const char* name; float min, max, initial; const char* unit; float centre = 0;
    // 0/1 parameters with no unit are switches and are exposed to hosts as on/off.
    constexpr bool isSwitch() const { return min == 0 && max == 1 && unit[0] == 0; }
};
inline constexpr std::array definitions {
    Definition { "INPUT_GAIN", "Input", -24, 24, 0, "dB" },
    Definition { "GATE_THRESH", "Gate", -80, 0, -60, "dB" },
    Definition { "DRIVE_GAIN", "Drive", 0, 24, 0, "dB" },
    Definition { "AMP_BASS", "Bass", -12, 12, 0, "dB" },
    Definition { "AMP_MID", "Middle", -12, 12, 0, "dB" },
    Definition { "AMP_TREBLE", "Treble", -12, 12, 0, "dB" },
    Definition { "AMP_OUT", "Amp output", -24, 12, 0, "dB" },
    Definition { "DELAY_TIME", "Delay time", 40, 1000, 320, "ms", 300 },
    Definition { "DELAY_MIX", "Delay mix", 0, 100, 0, "%" },
    Definition { "REVERB_MIX", "Reverb", 0, 100, 12, "%" },
    Definition { "MASTER_VOL", "Master", -60, 6, -12, "dB" },
    Definition { "AMP_CLEAN", "Clean channel", 0, 1, 0, "" },
    Definition { "TIGHT", "Tight", 20, 180, 20, "Hz", 70 },
    Definition { "PRESENCE", "Presence", -6, 6, 0, "dB" },
    Definition { "CLEAN_COMP", "Clean compression", 0, 100, 35, "%" },
    Definition { "HIGH_CUT", "High cut", 3000, 20000, 20000, "Hz", 8000 },
    Definition { "DELAY_WIDTH", "Delay width", 0, 100, 0, "%" },
    Definition { "REVERB_SIZE", "Reverb size", 0, 100, 60, "%" },
    Definition { "GATE_ON", "Noise gate enabled", 0, 1, 1, "" },
    Definition { "GATE_RELEASE", "Gate release", 40, 500, 140, "ms", 150 },
    Definition { "PEDAL_ON", "Pedal enabled", 0, 1, 0, "" },
    Definition { "DYN_RES_ON", "Dynamic resonance", 0, 1, 0, "" },
    Definition { "DYN_RES_AMOUNT", "Chug cut", 0, 100, 50, "%" },
    Definition { "CHUG_ATTACK", "Chug attack", 0, 100, 0, "%" },
    Definition { "THICKEN_ON", "Sub-synthesis", 0, 1, 0, "" },
    Definition { "THICKEN_MIX", "Sub mix", 0, 100, 30, "%" },
    Definition { "PIEZO_ON", "Piezo resonator", 0, 1, 0, "" },
    Definition { "PIEZO_BLEND", "Piezo sparkle", 0, 100, 40, "%" },
    Definition { "MICRO_DELAY", "Micro-delay", 0, 1.0f, 0.0f, "ms" },
    // Added last so existing sessions' parameter indices never move.
    Definition { "METRO_ON", "Metronome", 0, 1, 0, "" },
    Definition { "METRO_BPM", "Metronome tempo", 40, 240, 120, "BPM" },
    Definition { "METRO_BEATS", "Metronome beats per bar", 1, 12, 4, "beats" },
    Definition { "METRO_LEVEL", "Metronome level", -40, 0, -18, "dB" },
    Definition { "EQ_ON", "EQ pedal enabled", 0, 1, 0, "" },
    Definition { "EQ_BODY", "EQ body", -12, 12, 0, "dB" },
    Definition { "EQ_MUD", "EQ mud", -12, 12, 0, "dB" },
    Definition { "EQ_FOCUS", "EQ focus", -12, 12, 0, "dB" },
    Definition { "EQ_FIZZ", "EQ fizz", -12, 12, 0, "dB" }
};
enum Index { input, gate, drive, bass, mid, treble, ampOut, delayTime, delayMix, reverbMix, master, clean, tight,
             presence, cleanComp, highCut, delayWidth, reverbSize, gateOn, gateRelease, pedalOn,
             dynResOn, dynResAmount, chugAttack, thickenOn, thickenMix, piezoOn, piezoBlend, microDelay,
             metroOn, metroBpm, metroBeats, metroLevel, eqOn, eqBody, eqMud, eqFocus, eqFizz };
inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (const auto& p : definitions)
    {
        if (p.isSwitch())
        {
            result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID {p.id, 1}, p.name, p.initial >= 0.5f));
            continue;
        }
        juce::NormalisableRange<float> range {p.min, p.max, 0.01f};
        if (p.centre > 0) range.setSkewForCentre(p.centre);
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {p.id, 1}, p.name, range, p.initial,
            juce::AudioParameterFloatAttributes().withLabel(p.unit)));
    }
    return result;
}
}
