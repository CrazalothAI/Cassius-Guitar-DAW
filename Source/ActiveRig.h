#pragma once
#include "PerformanceScenes.h"

// UI/session identity with a small comparison baseline. Caller owns requestLock.
// Asset references use stable IDs when available; global listening controls do
// not mark a saved guitar tone as edited.
class ActiveRig
{
public:
    juce::String id, name;
    void set(const juce::String& nextId, const juce::String& nextName, const juce::ValueTree& state)
    {
        auto normalized = state.createCopy();
        if (PedalboardState::migrate(normalized).isNotEmpty() || BoardParams::addDefaults(normalized, false).isNotEmpty() || PerformanceScenes::migrate(normalized).isNotEmpty()) {
            id.clear(); name.clear(); baseline = {}; return;
        }
        id = nextId; name = nextName.substring(0, 80); baseline = juce::ValueTree("BASELINE");
        for (const auto& p : allDefinitions()) if (!PerformanceScenes::global(p.id)) {
            const auto row = normalized.getChildWithProperty("id", p.id);
            if (row.isValid()) baseline.addChild(row.createCopy(), -1, nullptr);
        }
        for (const auto* stage : {"model", "ir", "pedal", "irB", "pedal1", "ambience", "ambience1"}) for (const auto* suffix : {"Path", "Id"}) {
            const auto key = juce::String(stage) + suffix; baseline.setProperty(key, normalized[key], nullptr);
        }
        const auto scenes = normalized.getChildWithName("SCENES"); if (scenes.isValid()) baseline.addChild(scenes.createCopy(), -1, nullptr);
        baseline.addChild(normalized.getChildWithName("PEDALBOARD").createCopy(), -1, nullptr);
    }
    juce::ValueTree save() const {
        juce::ValueTree tree("ACTIVE_RIG"); tree.setProperty("id", id, nullptr); tree.setProperty("name", name, nullptr);
        if (baseline.isValid()) tree.addChild(baseline.createCopy(), -1, nullptr); return tree;
    }
    void restore(const juce::ValueTree& tree) {
        id.clear(); name.clear(); baseline = {};
        if (!tree.isValid() || tree["id"].toString().length() > 128 || tree["name"].toString().length() > 80) return;
        auto saved = tree.getChildWithName("BASELINE").createCopy();
        if (saved.isValid() && BoardParams::addDefaults(saved, false).isNotEmpty()) return;
        if (!saved.isValid() || saved.getNumChildren() > static_cast<int>(allDefinitions().size()) + 2 || tree.toXmlString().length() > 131072) return;
        for (const auto& p : allDefinitions()) if (!PerformanceScenes::global(p.id)) {
            const auto row = saved.getChildWithProperty("id", p.id); const float x = static_cast<float>(row["value"]);
            if (!row.isValid() || !std::isfinite(x) || x < p.min || x > p.max) return;
        }
        auto migrated = saved.createCopy();
        if (PedalboardState::migrate(migrated).isNotEmpty() || PerformanceScenes::migrate(migrated).isNotEmpty()) return;
        id = tree["id"].toString(); name = tree["name"].toString(); baseline = migrated;
    }
    // A/B/session snapshots can predate an in-place Save. Compare against the
    // currently saved entry so an older sound is correctly marked edited.
    void refreshSavedBaseline(const juce::ValueTree& entry) {
        if (!entry.hasType("RIG") || entry["id"].toString() != id) return;
        const auto xml = juce::XmlDocument::parse(entry["state"].toString());
        if (!xml || !xml->hasTagName("AmpSuiteState")) return;
        const auto state = juce::ValueTree::fromXml(*xml);
        if (entry.hasProperty("schema")) {
            const auto schema = entry["schema"].toString();
            if ((schema != "1" && schema != "2" && schema != "3") || ((schema == "2" || schema == "3") && !state.getChildWithName("PEDALBOARD").isValid())) return;
        }
        ActiveRig candidate; candidate.set(id, entry["name"].toString(), state);
        ActiveRig validated; validated.restore(candidate.save());
        if (validated.name.isNotEmpty()) *this = std::move(validated);
    }
    bool edited(juce::AudioProcessorValueTreeState& state, const std::array<juce::String, 7>& paths,
                const std::array<juce::String, 7>& ids, const juce::ValueTree& scenes) const {
        if (!baseline.isValid() || name.isEmpty()) return false;
        for (const auto& p : allDefinitions()) if (!PerformanceScenes::global(p.id))
            if (std::abs(state.getRawParameterValue(p.id)->load() - static_cast<float>(baseline.getChildWithProperty("id", p.id)["value"])) > .0001f) return true;
        constexpr const char* stages[] {"model", "ir", "pedal", "irB", "pedal1", "ambience", "ambience1"};
        for (size_t i = 0; i < paths.size(); ++i) {
            const auto savedId = baseline[juce::String(stages[i]) + "Id"].toString();
            if (savedId.isNotEmpty() ? savedId != ids[i] : baseline[juce::String(stages[i]) + "Path"].toString() != paths[i]) return true;
        }
        return !PedalboardState::equal(baseline, state.state)
            || !PerformanceScenes::equivalent(baseline.getChildWithName("SCENES"), scenes);
    }
private:
    static const std::vector<BoardParams::Definition>& allDefinitions() {
        static const auto rows = [] { std::vector<BoardParams::Definition> result; BoardParams::each([&](const auto& p) { result.push_back({p.id, p.name, p.min, p.max, p.initial, p.unit, p.centre}); }); return result; }(); return rows;
    }
    juce::ValueTree baseline;
};
