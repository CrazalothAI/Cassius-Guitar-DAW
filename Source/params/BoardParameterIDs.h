#pragma once
#include "ParameterIDs.h"
#include <vector>
#include <cstdlib>
#include <cmath>

// Append-only, kind-qualified host slots. The original 87 definitions stay in
// their original positions. A block's position never determines its parameter ID.
namespace BoardParams {
inline constexpr int slotsPerType = 2;
inline const juce::StringArray types {"compressor", "overdrive", "neural-pedal", "eq", "modulation", "chorus", "delay", "reverb", "ambience"};
inline const std::array<juce::StringArray, 9> controls {{
    {"CLEAN_COMP", "COMP_THRESH", "COMP_RATIO", "COMP_ATTACK", "COMP_RELEASE", "COMP_MAKEUP"},
    {"OD_ON", "OD_DRIVE", "OD_TONE", "OD_LEVEL", "OD_TIGHT"},
    {"PEDAL_ON", "PEDAL_INPUT", "PEDAL_OUTPUT"},
    {"EQ_ON", "EQ_BODY", "EQ_MUD", "EQ_FOCUS", "EQ_FIZZ"},
    {"MOD_ON", "MOD_TYPE", "MOD_RATE", "MOD_DEPTH", "MOD_MIX", "MOD_FEEDBACK", "MOD_STEREO", "MOD_SYNC", "MOD_DIVISION"},
    {"CHORUS_MIX", "CHORUS_RATE", "CHORUS_DEPTH"},
    {"DELAY_TIME", "DELAY_MIX", "DELAY_WIDTH", "DELAY_FEEDBACK", "DELAY_SYNC", "DELAY_DIVISION"},
    {"REVERB_MIX", "REVERB_SIZE", "REVERB_STYLE", "REVERB_DAMP", "REVERB_PREDELAY"},
    {"AMBIENCE_MIX"}
}};
inline juce::String prefix(int kind, int slot) { return "BOARD_" + types[kind].replaceCharacter('-', '_').toUpperCase() + "_" + juce::String(slot) + "_"; }
inline juce::String parameter(int kind, int slot, const juce::String& original) { return slot == 0 && kind != 8 ? original : prefix(kind, slot) + original; }
inline juce::String onId(int kind, int slot) {
    const auto first = controls[static_cast<size_t>(kind)][0];
    return first.endsWith("_ON") ? parameter(kind, slot, first) : prefix(kind, slot) + "ON";
}
inline juce::String trimId(int kind, int slot) { return prefix(kind, slot) + "TRIM"; }
struct Definition { juce::String id, name; float min, max, initial; const char* unit; float centre; };
inline const std::vector<Definition>& definitions() {
    static const auto rows = [] {
        std::vector<Definition> result;
        for (int kind = 0; kind < types.size(); ++kind) for (int slot = 0; slot < slotsPerType; ++slot) {
            const auto title = types[kind] + " " + juce::String(slot + 1) + " ";
            if (!controls[static_cast<size_t>(kind)][0].endsWith("_ON")) result.push_back({onId(kind, slot), title + "enabled", 0, 1, 1, "", 0});
            result.push_back({trimId(kind, slot), title + "output trim", -24, 12, 0, "dB", 0});
            if (kind == 8) result.push_back({parameter(kind, slot, "AMBIENCE_MIX"), title + "blend", 0, 100, 25, "%", 0});
            if (slot == 1) for (const auto& id : controls[static_cast<size_t>(kind)]) for (const auto& old : Params::definitions) if (id == old.id)
                result.push_back({parameter(kind, slot, id), title + old.name, old.min, old.max, old.initial, old.unit, old.centre});
        }
        return result;
    }();
    return rows;
}
template<typename Fn> inline void each(Fn fn) { for (const auto& p : Params::definitions) fn(p); for (const auto& p : definitions()) fn(p); }
inline juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    auto result = Params::layout();
    for (const auto& p : definitions()) {
        const juce::ParameterID id {p.id, 1};
        if (p.unit[0] == 0) {
            if (p.min == 0 && p.max == 1) result.add(std::make_unique<juce::AudioParameterBool>(id, p.name, p.initial >= .5f));
            else result.add(std::make_unique<juce::AudioParameterInt>(id, p.name, static_cast<int>(p.min), static_cast<int>(p.max), static_cast<int>(p.initial)));
        } else {
            juce::NormalisableRange<float> range {p.min, p.max, .01f}; if (p.centre > 0) range.setSkewForCentre(p.centre);
            result.add(std::make_unique<juce::AudioParameterFloat>(id, p.name, range, p.initial, juce::AudioParameterFloatAttributes().withLabel(p.unit)));
        }
    }
    return result;
}
inline juce::String addDefaults(juce::ValueTree& state, bool requireAll) {
    for (const auto& p : definitions()) {
        auto row = state.getChildWithProperty("id", p.id);
        if (!row.isValid() && !requireAll) { row = juce::ValueTree("PARAM"); row.setProperty("id", p.id, nullptr); row.setProperty("value", p.initial, nullptr); state.addChild(row, -1, nullptr); }
        if (!row.hasType("PARAM")) return "Incomplete board parameter: " + p.id;
        const auto text = row["value"].toString().toStdString(); char* end = nullptr; const auto amount = std::strtod(text.c_str(), &end); const auto x = static_cast<float>(amount);
        if (end == text.c_str() || *end != '\0' || !std::isfinite(amount) || x < p.min || x > p.max || (p.unit[0] == 0 && amount != std::floor(amount))) return "Invalid board parameter: " + p.id;
    }
    return {};
}
}
