#pragma once
#include "ParameterIDs.h"
#include <vector>
#include <cstdlib>
#include <cmath>

// Append-only, kind-qualified host slots. The original 87 definitions stay in
// their original positions. A block's position never determines its parameter ID.
namespace BoardParams {
inline constexpr int slotsPerType = 2;
inline const juce::StringArray types {"compressor", "overdrive", "neural-pedal", "eq", "modulation", "chorus", "delay", "reverb", "ambience", "wah", "distortion", "plate"};
inline const std::array<juce::StringArray, 12> controls {{
    {"CLEAN_COMP", "COMP_THRESH", "COMP_RATIO", "COMP_ATTACK", "COMP_RELEASE", "COMP_MAKEUP"},
    {"OD_ON", "OD_DRIVE", "OD_TONE", "OD_LEVEL", "OD_TIGHT"},
    {"PEDAL_ON", "PEDAL_INPUT", "PEDAL_OUTPUT"},
    {"EQ_ON", "EQ_BODY", "EQ_MUD", "EQ_FOCUS", "EQ_FIZZ"},
    {"MOD_ON", "MOD_TYPE", "MOD_RATE", "MOD_DEPTH", "MOD_MIX", "MOD_FEEDBACK", "MOD_STEREO", "MOD_SYNC", "MOD_DIVISION"},
    {"CHORUS_MIX", "CHORUS_RATE", "CHORUS_DEPTH"},
    {"DELAY_TIME", "DELAY_MIX", "DELAY_WIDTH", "DELAY_FEEDBACK", "DELAY_SYNC", "DELAY_DIVISION"},
    {"REVERB_MIX", "REVERB_SIZE", "REVERB_STYLE", "REVERB_DAMP", "REVERB_PREDELAY"},
    {"AMBIENCE_MIX"},
    {"WAH_MODE", "WAH_POSITION", "WAH_SENSITIVITY", "WAH_RESONANCE", "WAH_MIX"},
    {"DIST_MODE", "DIST_DRIVE", "DIST_TONE", "DIST_TIGHT", "DIST_MIX"},
    {"PLATE_DECAY", "PLATE_TONE", "PLATE_PREDELAY", "PLATE_WIDTH", "PLATE_MIX"}
}};
inline juce::String prefix(int kind, int slot) { return "BOARD_" + types[kind].replaceCharacter('-', '_').toUpperCase() + "_" + juce::String(slot) + "_"; }
inline juce::String parameter(int kind, int slot, const juce::String& original) { return slot == 0 && kind < 8 ? original : prefix(kind, slot) + original; }
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
            if (kind == 9) {
                result.push_back({parameter(kind, slot, "WAH_MODE"), title + "mode", 0, 1, 0, "", 0});
                result.push_back({parameter(kind, slot, "WAH_POSITION"), title + "position", 0, 100, 50, "%", 0});
                result.push_back({parameter(kind, slot, "WAH_SENSITIVITY"), title + "sensitivity", -24, 24, 0, "dB", 0});
                result.push_back({parameter(kind, slot, "WAH_RESONANCE"), title + "resonance", 0, 100, 45, "%", 0});
                result.push_back({parameter(kind, slot, "WAH_MIX"), title + "blend", 0, 100, 100, "%", 0});
            }
            if (kind == 10) {
                result.push_back({parameter(kind, slot, "DIST_MODE"), title + "mode", 0, 2, 0, "", 0});
                result.push_back({parameter(kind, slot, "DIST_DRIVE"), title + "drive", 0, 100, 45, "%", 0});
                result.push_back({parameter(kind, slot, "DIST_TONE"), title + "tone", 0, 100, 50, "%", 0});
                result.push_back({parameter(kind, slot, "DIST_TIGHT"), title + "low cut", 20, 250, 80, "Hz", 80});
                result.push_back({parameter(kind, slot, "DIST_MIX"), title + "blend", 0, 100, 100, "%", 0});
            }
            if (kind == 11) {
                result.push_back({parameter(kind, slot, "PLATE_DECAY"), title + "decay", .3f, 8, 2.2f, "s", 2});
                result.push_back({parameter(kind, slot, "PLATE_TONE"), title + "tone", 0, 100, 55, "%", 0});
                result.push_back({parameter(kind, slot, "PLATE_PREDELAY"), title + "pre-delay", 0, 150, 20, "ms", 0});
                result.push_back({parameter(kind, slot, "PLATE_WIDTH"), title + "width", 0, 100, 80, "%", 0});
                result.push_back({parameter(kind, slot, "PLATE_MIX"), title + "blend", 0, 100, 18, "%", 0});
            }
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
inline bool hasWah(const juce::ValueTree& state) {
    const auto board = state.hasType("PEDALBOARD") ? state : state.getChildWithName("PEDALBOARD");
    for (const auto& block : board) if (block["type"].toString() == "wah") return true;
    return false;
}
// Only absent appended families may default. Reserved/deleted blocks still
// require complete controls, so truncated current rigs cannot silently recall.
inline bool appendedFamilyAbsent(const juce::String& id, const juce::ValueTree& state) {
    const auto board = state.hasType("PEDALBOARD") ? state : state.getChildWithName("PEDALBOARD");
    for (const auto* type : {"wah", "distortion", "plate"}) {
        const auto family = "BOARD_" + juce::String(type).toUpperCase() + "_";
        if (id.startsWith(family)) { for (const auto& block : board) if (block["type"].toString() == type) return false; return true; }
    }
    return false;
}
inline juce::String addDefaults(juce::ValueTree& state, bool requireAll) {
    for (const auto& p : definitions()) {
        auto row = state.getChildWithProperty("id", p.id);
        // Pre-0.4 schema-3 documents have no wah controls. Default only this
        // appended family when no wah block (including reserved slots) exists.
        if (!row.isValid() && (!requireAll || appendedFamilyAbsent(p.id, state))) { row = juce::ValueTree("PARAM"); row.setProperty("id", p.id, nullptr); row.setProperty("value", p.initial, nullptr); state.addChild(row, -1, nullptr); }
        if (!row.hasType("PARAM")) return "Incomplete board parameter: " + p.id;
        const auto text = row["value"].toString().toStdString(); char* end = nullptr; const auto amount = std::strtod(text.c_str(), &end); const auto x = static_cast<float>(amount);
        if (end == text.c_str() || *end != '\0' || !std::isfinite(amount) || x < p.min || x > p.max || (p.unit[0] == 0 && amount != std::floor(amount))) return "Invalid board parameter: " + p.id;
    }
    return {};
}
}
