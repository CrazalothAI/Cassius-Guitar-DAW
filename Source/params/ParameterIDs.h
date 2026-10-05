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
    Definition { "EQ_FIZZ", "EQ fizz", -12, 12, 0, "dB" },
    Definition { "AMP_SOURCE", "Amp source", 0, 4, 0, "" },
    Definition { "CAPTURE_KIND", "Capture type", 0, 3, 0, "" },
    Definition { "CAB_MODE", "Cabinet mode", 0, 3, 0, "" },
    Definition { "PEDAL_INPUT", "Pedal input", -24, 24, 0, "dB" },
    Definition { "PEDAL_OUTPUT", "Pedal output", -24, 12, 0, "dB" },
    Definition { "COMP_THRESH", "Compressor threshold", -40, 0, -20, "dB" },
    Definition { "COMP_RATIO", "Compressor ratio", 1, 10, 2.5f, ":1" },
    Definition { "COMP_ATTACK", "Compressor attack", 1, 100, 15, "ms" },
    Definition { "COMP_RELEASE", "Compressor release", 20, 500, 140, "ms", 150 },
    Definition { "COMP_MAKEUP", "Compressor makeup", 0, 12, 3, "dB" },
    Definition { "CHORUS_MIX", "Chorus mix", 0, 100, 0, "%" },
    Definition { "CHORUS_RATE", "Chorus rate", 0.1f, 5, 0.8f, "Hz" },
    Definition { "CHORUS_DEPTH", "Chorus depth", 0, 100, 35, "%" },
    Definition { "DELAY_FEEDBACK", "Delay feedback", 0, 85, 35, "%" },
    Definition { "DELAY_SYNC", "Delay sync", 0, 1, 0, "" },
    Definition { "DELAY_DIVISION", "Delay division", 0, 5, 2, "" },
    Definition { "REVERB_STYLE", "Reverb voice", 0, 2, 0, "" },
    Definition { "REVERB_DAMP", "Reverb damping", 0, 100, 55, "%" },
    Definition { "REVERB_PREDELAY", "Reverb pre-delay", 0, 150, 0, "ms" },
    Definition { "CAPTURE_MATCH", "Capture level matching", 0, 1, 1, "" },
    // Appended: older rigs keep their routing and bypassed additions.
    Definition { "COMP_MODE", "Compressor routing", 0, 3, 0, "" },
    Definition { "OD_ON", "Overdrive enabled", 0, 1, 0, "" },
    Definition { "OD_DRIVE", "Overdrive drive", 0, 100, 15, "%" },
    Definition { "OD_TONE", "Overdrive tone", 0, 100, 50, "%" },
    Definition { "OD_LEVEL", "Overdrive level", -24, 12, 0, "dB" },
    Definition { "OD_TIGHT", "Overdrive low cut", 20, 200, 80, "Hz", 80 },
    Definition { "CAB_B_ON", "Second cabinet enabled", 0, 1, 0, "" },
    Definition { "CAB_BLEND", "Cabinet blend", 0, 100, 50, "%" },
    Definition { "CAB_A_LEVEL", "Cabinet A level", -24, 12, 0, "dB" },
    Definition { "CAB_B_LEVEL", "Cabinet B level", -24, 12, 0, "dB" },
    Definition { "CAB_A_PAN", "Cabinet A pan", -100, 100, 0, "%" },
    Definition { "CAB_B_PAN", "Cabinet B pan", -100, 100, 0, "%" },
    Definition { "CAB_A_INVERT", "Cabinet A polarity", 0, 1, 0, "" },
    Definition { "CAB_B_INVERT", "Cabinet B polarity", 0, 1, 0, "" },
    Definition { "CAB_A_DELAY", "Cabinet A alignment", 0, 10, 0, "ms" },
    Definition { "CAB_B_DELAY", "Cabinet B alignment", 0, 10, 0, "ms" },
    Definition { "CAB_LOW_CUT", "Cabinet low cut", 20, 500, 20, "Hz", 100 },
    Definition { "CAB_HIGH_CUT", "Cabinet high cut", 2000, 20000, 20000, "Hz", 8000 },
    // Appended modulation controls preserve the first 76 automation indices.
    Definition { "MOD_ON", "Modulation enabled", 0, 1, 0, "" },
    Definition { "MOD_TYPE", "Modulation voice", 0, 2, 0, "" },
    Definition { "MOD_RATE", "Modulation rate", 0.05f, 10, 0.8f, "Hz" },
    Definition { "MOD_DEPTH", "Modulation depth", 0, 100, 50, "%" },
    Definition { "MOD_MIX", "Modulation mix", 0, 100, 50, "%" },
    Definition { "MOD_FEEDBACK", "Modulation feedback", 0, 70, 20, "%" },
    Definition { "MOD_STEREO", "Modulation stereo", 0, 100, 0, "%" },
    Definition { "MOD_SYNC", "Modulation sync", 0, 1, 0, "" },
    Definition { "MOD_DIVISION", "Modulation division", 0, 4, 2, "" },
    // Listening controls append after the original 85 automation indices.
    Definition { "GUITAR_MIX_LEVEL", "Guitar balance", -12, 12, 0, "dB" },
    Definition { "GUITAR_MIX_FOCUS", "Mix focus", 0, 100, 0, "%" }
};
enum Index { input, gate, drive, bass, mid, treble, ampOut, delayTime, delayMix, reverbMix, master, clean, tight,
             presence, cleanComp, highCut, delayWidth, reverbSize, gateOn, gateRelease, pedalOn,
             dynResOn, dynResAmount, chugAttack, thickenOn, thickenMix, piezoOn, piezoBlend, microDelay,
             metroOn, metroBpm, metroBeats, metroLevel, eqOn, eqBody, eqMud, eqFocus, eqFizz, ampSource, captureKind, cabMode,
             pedalInput, pedalOutput, compThreshold, compRatio, compAttack, compRelease, compMakeup,
             chorusMix, chorusRate, chorusDepth, delayFeedback, delaySync, delayDivision, reverbStyle, reverbDamp, reverbPredelay, captureMatch, compMode, odOn, odDrive, odTone, odLevel, odTight, cabBOn, cabBlend, cabALevel, cabBLevel, cabAPan, cabBPan, cabAInvert, cabBInvert, cabADelay, cabBDelay, cabLowCut, cabHighCut, modOn, modType, modRate, modDepth, modMix, modFeedback, modStereo, modSync, modDivision, guitarMixLevel, guitarMixFocus };
inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (const auto& p : definitions)
    {
        const juce::String id(p.id);
        if (id == "AMP_SOURCE" || id == "CAPTURE_KIND" || id == "CAB_MODE" || id == "DELAY_DIVISION" || id == "REVERB_STYLE" || id == "COMP_MODE" || id == "MOD_TYPE" || id == "MOD_DIVISION")
        {
            const auto choices = id == "AMP_SOURCE" ? juce::StringArray {"Current rig", "Lumen", "Ferrum", "NAM capture", "Natural DI"}
                : id == "CAPTURE_KIND" ? juce::StringArray {"Auto", "Amp-only", "Preamp-only", "Full rig"}
                : id == "CAB_MODE" ? juce::StringArray {"Auto", "External IR", "Built-in speaker", "Off"}
                : id == "DELAY_DIVISION" ? juce::StringArray {"Quarter", "Eighth", "Dotted eighth", "Sixteenth", "Half", "Whole"}
                : id == "COMP_MODE" ? juce::StringArray {"Lumen only (legacy)", "Pre-amp", "Post-cab", "Off"}
                : id == "MOD_TYPE" ? juce::StringArray {"Phaser", "Flanger", "Tremolo"}
                : id == "MOD_DIVISION" ? juce::StringArray {"Whole", "Half", "Quarter", "Eighth", "Dotted eighth"}
                : juce::StringArray {"Room", "Chamber", "Hall"};
            result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID {p.id, 1}, p.name, choices, static_cast<int>(p.initial)));
            continue;
        }
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
