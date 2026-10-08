#pragma once
#include "params/ParameterIDs.h"
#include "PedalboardState.h"

// Four parameter snapshots share the current rig's files. All bank operations
// run outside the audio callback; parameters use the existing DSP smoothing.
class PerformanceScenes
{
public:
    static bool global(const juce::String& id) { return id == "INPUT_GAIN" || id == "MASTER_VOL" || id.startsWith("METRO_") || id.startsWith("GUITAR_MIX_"); }
    static juce::var capture(juce::AudioProcessorValueTreeState& state)
    {
        auto o = std::make_unique<juce::DynamicObject>();
        for (const auto& p : allDefinitions()) if (!global(p.id)) o->setProperty(p.id, state.getRawParameterValue(p.id)->load());
        return juce::var(o.release());
    }
    static juce::String validate(const juce::ValueTree& tree)
    {
        juce::var parsed; return parse(tree, parsed);
    }
    // Upgrade old banks with the shared rig's stable block identities. Missing
    // banks remain absent so old sessions still start with four empty slots.
    static juce::String migrate(juce::ValueTree& parent)
    {
        juce::ValueTree tree;
        for (const auto& child : parent) if (child.hasType("SCENES")) {
            if (tree.isValid()) return "Duplicate scene bank.";
            tree = child;
        }
        if (!tree.isValid()) return {};
        juce::var parsed;
        const auto board = parent.getChildWithName("PEDALBOARD");
        if (const auto failure = parse(tree, parsed, board.isValid() ? board : PedalboardState::legacy()); failure.isNotEmpty()) return failure;
        tree.setProperty("json", juce::JSON::toString(parsed), nullptr);
        return {};
    }
    // Native legacy banks historically clear/report malformed parameter JSON.
    // Present board metadata is checked separately before any session mutation.
    static juce::String validateBoards(const juce::ValueTree& tree)
    {
        if (!tree.isValid() || !tree["json"].isString()) return {};
        if (tree["json"].toString().length() > 65536) return "Scene bank is too large.";
        const auto parsed = juce::JSON::parse(tree["json"].toString());
        if (!parsed.isObject()) return {};
        const auto version = parsed["version"];
        if ((version.isInt() || version.isInt64() || version.isDouble()) && static_cast<double>(version) >= 2) {
            juce::var checked; return parse(tree, checked);
        }
        if (!parsed["slots"].isArray()) return {};
        bool carriesBoards = false;
        for (const auto& slot : *parsed["slots"].getArray()) if (slot.isObject() && slot.hasProperty("board")) {
            carriesBoards = true;
            juce::ValueTree board;
            if (const auto failure = readBoard(slot["board"], board); failure.isNotEmpty()) return failure;
        }
        if (carriesBoards) { juce::var checked; return parse(tree, checked); }
        return {};
    }
    static bool equivalent(const juce::ValueTree& a, const juce::ValueTree& b)
    {
        juce::var left, right;
        if (parse(a, left).isNotEmpty() || parse(b, right).isNotEmpty()) return false;
        for (int i = 0; i < 4; ++i) {
            const auto x = left.isObject() ? left["slots"][i] : juce::var();
            const auto y = right.isObject() ? right["slots"][i] : juce::var();
            if (x.isVoid() != y.isVoid()) return false;
            if (x.isVoid()) continue;
            if (x["name"].toString() != y["name"].toString()) return false;
            for (const auto& p : allDefinitions()) if (!global(p.id)
                && std::abs(static_cast<float>(x["parameters"][p.id.toRawUTF8()]) - static_cast<float>(y["parameters"][p.id.toRawUTF8()])) > .0001f) return false;
            juce::ValueTree first, second;
            if (readBoard(x["board"], first).isNotEmpty() || readBoard(y["board"], second).isNotEmpty()
                || !PedalboardState::equal(holder(first), holder(second))) return false;
        }
        return true;
    }
    juce::String store(int slot, const juce::String& name, juce::AudioProcessorValueTreeState& state)
    {
        if (!validSlot(slot)) return "Choose one of the four scenes.";
        const auto title = name.trim(); if (title.isEmpty() || title.length() > 48) return "Give the scene a name of 1 to 48 characters.";
        if (const auto failure = PedalboardState::validate(state.state); failure.isNotEmpty()) return failure;
        auto board = state.state.getChildWithName("PEDALBOARD"); if (!board.isValid()) board = PedalboardState::legacy();
        auto o = std::make_unique<juce::DynamicObject>(); o->setProperty("name", title); o->setProperty("parameters", capture(state));
        o->setProperty("board", board.toXmlString());
        const juce::ScopedLock guard(lock); slots[static_cast<size_t>(slot)] = juce::var(o.release()); active = slot; ++revision; error.clear(); return {};
    }
    juce::String rename(int slot, const juce::String& name)
    {
        if (!validSlot(slot)) return "Choose one of the four scenes.";
        const auto title = name.trim(); if (title.isEmpty() || title.length() > 48) return "Give the scene a name of 1 to 48 characters.";
        const juce::ScopedLock guard(lock);
        if (!slots[static_cast<size_t>(slot)].isObject()) return "This scene is empty. Store the current tone first.";
        if (slots[static_cast<size_t>(slot)]["name"].toString() == title) return {};
        auto renamed = slots[static_cast<size_t>(slot)].clone(); renamed.getDynamicObject()->setProperty("name", title);
        slots[static_cast<size_t>(slot)] = std::move(renamed); ++revision; error.clear(); return {};
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
        juce::ValueTree board;
        if (const auto failure = readBoard(saved["board"], board); failure.isNotEmpty()) return failure;
        for (const auto& p : allDefinitions()) if (!global(p.id)) {
            auto* parameter = state.getParameter(p.id);
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(saved["parameters"][p.id.toRawUTF8()]))); parameter->endChangeGesture();
        }
        // Metadata changes do not reset any DSP objects or running effect tails.
        state.state.removeChild(state.state.getChildWithName("PEDALBOARD"), nullptr);
        state.state.addChild(board, -1, nullptr);
        const juce::ScopedLock guard(lock);
        if (revision == generation) { active = slot; ++revision; error.clear(); }
        return {};
    }
    juce::ValueTree save()
    {
        const juce::ScopedLock guard(lock); juce::ValueTree tree("SCENES");
        auto o = std::make_unique<juce::DynamicObject>(); juce::Array<juce::var> rows;
        for (const auto& slot : slots) rows.add(slot);
        o->setProperty("version", 3); o->setProperty("slots", rows); tree.setProperty("json", juce::JSON::toString(juce::var(o.release())), nullptr); return tree;
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
        if (validSlot(active)) for (const auto& p : allDefinitions()) if (!global(p.id) && std::abs(static_cast<float>(current[p.id.toRawUTF8()]) - static_cast<float>(slots[static_cast<size_t>(active)]["parameters"][p.id.toRawUTF8()])) > .005f) { edited = true; break; }
        if (validSlot(active) && !edited) {
            juce::ValueTree board;
            edited = readBoard(slots[static_cast<size_t>(active)]["board"], board).isNotEmpty()
                || !PedalboardState::equal(state.state, holder(board));
        }
        o->setProperty("slots", rows); o->setProperty("active", active); o->setProperty("edited", edited); o->setProperty("revision", revision); o->setProperty("error", error); return juce::var(o.release());
    }
    juce::String applySnapshot(int slot, juce::ValueTree& state) {
        if (!validSlot(slot)) return "Choose one of the four scenes.";
        const juce::ScopedLock guard(lock); const auto saved = slots[static_cast<size_t>(slot)];
        if (!saved.isObject()) return "This scene is empty. Store the current tone first.";
        juce::ValueTree board; if (const auto failure = readBoard(saved["board"], board); failure.isNotEmpty()) return failure;
        state.removeChild(state.getChildWithName("PEDALBOARD"), nullptr); state.addChild(board, -1, nullptr);
        for (const auto& p : allDefinitions()) if (!global(p.id)) state.getChildWithProperty("id", p.id).setProperty("value", saved["parameters"][p.id.toRawUTF8()], nullptr);
        return {};
    }
    void markActive(int slot) { const juce::ScopedLock guard(lock); active = slot; ++revision; }
private:
    static const std::vector<BoardParams::Definition>& allDefinitions() {
        static const auto rows = [] { std::vector<BoardParams::Definition> result; BoardParams::each([&](const auto& p) { result.push_back({p.id, p.name, p.min, p.max, p.initial, p.unit, p.centre}); }); return result; }(); return rows;
    }
    static bool validSlot(int i) { return i >= 0 && i < 4; }
    static juce::ValueTree holder(const juce::ValueTree& board)
    {
        juce::ValueTree parent("AmpSuiteState"); parent.addChild(board.createCopy(), -1, nullptr); return parent;
    }
    static juce::String readBoard(const juce::var& text, juce::ValueTree& board)
    {
        if (!text.isString() || text.toString().getNumBytesAsUTF8() > PedalboardState::maximumBytes) return "Invalid saved scene pedalboard.";
        const auto xml = juce::XmlDocument::parse(text.toString());
        if (!xml || !xml->hasTagName("PEDALBOARD")) return "Invalid saved scene pedalboard.";
        board = juce::ValueTree::fromXml(*xml);
        return PedalboardState::validate(holder(board));
    }
    static juce::String parse(const juce::ValueTree& tree, juce::var& parsed,
                              const juce::ValueTree& legacyBoard = PedalboardState::legacy())
    {
        if (!tree.isValid()) return {}; // Legacy rigs/sessions have an empty bank.
        if (!tree.hasType("SCENES") || !tree["json"].isString() || tree["json"].toString().length() > 65536) return "Invalid scene bank.";
        parsed = juce::JSON::parse(tree["json"].toString());
        if (!parsed.isObject() || !parsed["version"].isInt() || (static_cast<int>(parsed["version"]) < 1 || static_cast<int>(parsed["version"]) > 3) || !parsed["slots"].isArray() || parsed["slots"].size() != 4) return "Unsupported scene bank.";
        const bool legacy = static_cast<int>(parsed["version"]) == 1;
        for (auto& slot : *parsed["slots"].getArray()) {
            if (slot.isVoid()) continue;
            if (!slot.isObject() || !slot["name"].isString() || slot["name"].toString().trim().isEmpty() || slot["name"].toString().length() > 48 || !slot["parameters"].isObject()) return "Invalid saved scene.";
            const auto values = slot["parameters"];
            juce::ValueTree board;
            if (slot.hasProperty("board")) {
                if (const auto failure = readBoard(slot["board"], board); failure.isNotEmpty()) return failure;
            } else if (legacy) board = legacyBoard.createCopy();
            else return "Saved scene is missing its pedalboard.";
            for (const auto& key : values.getDynamicObject()->getProperties()) {
                bool known = false; for (const auto& p : allDefinitions()) if (key.name.toString() == p.id && !global(p.id)) { known = true; break; }
                if (!known) return "Scene contains an unsupported or global parameter.";
            }
            for (size_t i = 0; i < allDefinitions().size(); ++i) {
                const auto& p = allDefinitions()[i]; if (global(p.id)) continue;
                // Future additions default without changing these original scene controls.
                if (!values.hasProperty(p.id) && ((i >= 85 && static_cast<int>(parsed["version"]) < 3)
                    || BoardParams::appendedFamilyAbsent(p.id, board))) values.getDynamicObject()->setProperty(p.id, p.initial);
                const auto v = values[p.id.toRawUTF8()]; const double x = static_cast<double>(v);
                // JSON shortens float endpoints (e.g. 0.05f). Validate at the
                // native parameter's precision so its own minimum round-trips.
                const auto amount = static_cast<float>(x);
                if ((!v.isDouble() && !v.isInt() && !v.isInt64()) || !std::isfinite(x) || amount < p.min || amount > p.max || (p.unit[0] == 0 && x != std::round(x))) return "Invalid scene parameter: " + juce::String(p.id);
            }
            if (const auto failure = PedalboardState::validate(holder(board)); failure.isNotEmpty()) return failure;
            slot.getDynamicObject()->setProperty("board", board.toXmlString());
        }
        parsed.getDynamicObject()->setProperty("version", 3);
        return {};
    }
    juce::CriticalSection lock;
    std::array<juce::var, 4> slots;
    int active = -1, revision = 0;
    juce::String error;
};
