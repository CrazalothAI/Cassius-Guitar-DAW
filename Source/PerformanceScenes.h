#pragma once
#include "params/ParameterIDs.h"

// Four parameter snapshots share the current rig's files. All bank operations
// run outside the audio callback; parameters use the existing DSP smoothing.
class PerformanceScenes
{
public:
    static bool global(const juce::String& id) { return id == "INPUT_GAIN" || id == "MASTER_VOL" || id.startsWith("METRO_"); }
    static juce::var capture(juce::AudioProcessorValueTreeState& state)
    {
        auto o = std::make_unique<juce::DynamicObject>();
        for (const auto& p : Params::definitions) if (!global(p.id)) o->setProperty(p.id, state.getRawParameterValue(p.id)->load());
        return juce::var(o.release());
    }
    static juce::String validate(const juce::ValueTree& tree)
    {
        juce::var parsed; return parse(tree, parsed);
    }
    juce::String store(int slot, const juce::String& name, juce::AudioProcessorValueTreeState& state)
    {
        if (!validSlot(slot)) return "Choose one of the four scenes.";
        const auto title = name.trim(); if (title.isEmpty() || title.length() > 48) return "Give the scene a name of 1–48 characters.";
        auto o = std::make_unique<juce::DynamicObject>(); o->setProperty("name", title); o->setProperty("parameters", capture(state));
        const juce::ScopedLock guard(lock); slots[static_cast<size_t>(slot)] = juce::var(o.release()); active = slot; ++revision; error.clear(); return {};
    }
    juce::String clear(int slot)
    {
        if (!validSlot(slot)) return "Choose one of the four scenes.";
        const juce::ScopedLock guard(lock); slots[static_cast<size_t>(slot)] = juce::var(); if (active == slot) active = -1; ++revision; error.clear(); return {};
    }
    juce::String recall(int slot, juce::AudioProcessorValueTreeState& state)
    {
        if (!validSlot(slot)) return "Choose one of the four scenes.";
        juce::var saved; int generation;
        { const juce::ScopedLock guard(lock); saved = slots[static_cast<size_t>(slot)]; generation = revision; }
        if (saved.isVoid()) return "This scene is empty. Store the current tone first.";
        for (const auto& p : Params::definitions) if (!global(p.id)) {
            auto* parameter = state.getParameter(p.id);
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(saved["parameters"][p.id]))); parameter->endChangeGesture();
        }
        const juce::ScopedLock guard(lock);
        if (revision == generation) { active = slot; ++revision; error.clear(); }
        return {};
    }
    juce::ValueTree save()
    {
        const juce::ScopedLock guard(lock); juce::ValueTree tree("SCENES");
        auto o = std::make_unique<juce::DynamicObject>(); juce::Array<juce::var> rows;
        for (const auto& slot : slots) rows.add(slot);
        o->setProperty("version", 1); o->setProperty("slots", rows); tree.setProperty("json", juce::JSON::toString(juce::var(o.release())), nullptr); return tree;
    }
    juce::String restore(const juce::ValueTree& tree)
    {
        juce::var parsed; const auto failure = parse(tree, parsed);
        const juce::ScopedLock guard(lock); slots = {}; active = -1; ++revision; error = failure;
        if (failure.isEmpty() && parsed.isObject()) for (int i = 0; i < 4; ++i) slots[static_cast<size_t>(i)] = parsed["slots"][i];
        return failure;
    }
    juce::var status(juce::AudioProcessorValueTreeState& state)
    {
        const auto current = capture(state); const juce::ScopedLock guard(lock);
        auto o = std::make_unique<juce::DynamicObject>(); juce::Array<juce::var> rows;
        for (const auto& slot : slots) { auto row = std::make_unique<juce::DynamicObject>(); row->setProperty("name", slot["name"]); row->setProperty("stored", slot.isObject()); rows.add(juce::var(row.release())); }
        bool edited = false;
        if (validSlot(active)) for (const auto& p : Params::definitions) if (!global(p.id) && std::abs(static_cast<float>(current[p.id]) - static_cast<float>(slots[static_cast<size_t>(active)]["parameters"][p.id])) > .005f) { edited = true; break; }
        o->setProperty("slots", rows); o->setProperty("active", active); o->setProperty("edited", edited); o->setProperty("revision", revision); o->setProperty("error", error); return juce::var(o.release());
    }
private:
    static bool validSlot(int i) { return i >= 0 && i < 4; }
    static juce::String parse(const juce::ValueTree& tree, juce::var& parsed)
    {
        if (!tree.isValid()) return {}; // Legacy rigs/sessions have an empty bank.
        if (!tree.hasType("SCENES") || !tree["json"].isString() || tree["json"].toString().length() > 65536) return "Invalid scene bank.";
        parsed = juce::JSON::parse(tree["json"].toString());
        if (!parsed.isObject() || !parsed["version"].isInt() || static_cast<int>(parsed["version"]) != 1 || !parsed["slots"].isArray() || parsed["slots"].size() != 4) return "Unsupported scene bank.";
        for (auto& slot : *parsed["slots"].getArray()) {
            if (slot.isVoid()) continue;
            if (!slot.isObject() || !slot["name"].isString() || slot["name"].toString().trim().isEmpty() || slot["name"].toString().length() > 48 || !slot["parameters"].isObject()) return "Invalid saved scene.";
            const auto values = slot["parameters"];
            for (const auto& key : values.getDynamicObject()->getProperties()) {
                bool known = false; for (const auto& p : Params::definitions) if (key.name.toString() == p.id && !global(p.id)) { known = true; break; }
                if (!known) return "Scene contains an unsupported or global parameter.";
            }
            for (size_t i = 0; i < Params::definitions.size(); ++i) {
                const auto& p = Params::definitions[i]; if (global(p.id)) continue;
                // Future additions default without changing these original scene controls.
                if (!values.hasProperty(p.id) && i >= 85) values.getDynamicObject()->setProperty(p.id, p.initial);
                const auto v = values[p.id]; const double x = static_cast<double>(v);
                // JSON shortens float endpoints (e.g. 0.05f). Validate at the
                // native parameter's precision so its own minimum round-trips.
                const auto amount = static_cast<float>(x);
                if ((!v.isDouble() && !v.isInt() && !v.isInt64()) || !std::isfinite(x) || amount < p.min || amount > p.max || (p.unit[0] == 0 && x != std::round(x))) return "Invalid scene parameter: " + juce::String(p.id);
            }
        }
        return {};
    }
    juce::CriticalSection lock;
    std::array<juce::var, 4> slots;
    int active = -1, revision = 0;
    juce::String error;
};
