#include "PluginProcessor.h"

juce::var AmpSuiteAudioProcessor::boardStatus()
{
    const juce::ScopedLock guard(requestLock);
    auto o = std::make_unique<juce::DynamicObject>(); juce::Array<juce::var> rows;
    const auto board = apvts.state.getChildWithName("PEDALBOARD");
    const bool serial = PedalboardState::serial(apvts.state);
    if (serial) for (const auto& block : board) if (static_cast<int>(block["deleted"]) == 0) {
        auto row = std::make_unique<juce::DynamicObject>();
        for (const auto* key : {"id", "type", "automationSlot", "lane"}) row->setProperty(key, block[key]);
        const int kind = PedalboardState::kind(block), slot = static_cast<int>(block["automationSlot"]);
        row->setProperty("enabledId", BoardParams::onId(kind, slot));
        row->setProperty("trimId", BoardParams::trimId(kind, slot));
        const auto path = kind == 8 ? (slot == 0 ? desiredAmbience : desiredAmbience1) : (slot == 0 ? desiredPedal : desiredPedal1);
        const juce::String assetKind = kind == 8 ? "ambience" : "pedal";
        if (kind == 2 || kind == 8) { row->setProperty("assetId", library.idForPath(assetKind, path)); row->setProperty("assetName", library.find(library.idForPath(assetKind, path))["name"].toString()); }
        rows.add(juce::var(row.release()));
    }
    o->setProperty("serial", serial); o->setProperty("blocks", rows);
    o->setProperty("reserved", serial ? board.getNumChildren() : 0);
    o->setProperty("canUndo", !boardUndo.empty()); o->setProperty("canRedo", !boardRedo.empty());
    o->setProperty("loading", sceneAssetsLoading()); return juce::var(o.release());
}

juce::String AmpSuiteAudioProcessor::boardCommand(const juce::String& action, const juce::var& args)
{
    const juce::ScopedLock guard(requestLock);
    if (sceneAssetsLoading()) return "Finish loading before editing the pedalboard.";
    auto before = copyRigState(false), state = before.createCopy();
    auto board = state.getChildWithName("PEDALBOARD");
    const auto set = [&](const juce::String& id, float amount) { state.getChildWithProperty("id", id).setProperty("value", amount, nullptr); };
    if (action == "undo" || action == "redo") {
        const auto& stack = action == "undo" ? boardUndo : boardRedo;
        if (stack.empty()) return "No board edit to " + action + ".";
        const auto saved = stack.back();
        state.removeChild(board, nullptr); state.addChild(saved.getChildWithName("PEDALBOARD").createCopy(), -1, nullptr);
        // Structural undo restores the pedal settings and capture assignments,
        // while retaining the current amp, cabinets, scenes and listening level.
        for (int kind = 0; kind < BoardParams::types.size(); ++kind) for (int slot = 0; slot < 2; ++slot) {
            for (const auto& id : BoardParams::controls[static_cast<size_t>(kind)]) { const auto key = BoardParams::parameter(kind, slot, id); set(key, static_cast<float>(saved.getChildWithProperty("id", key)["value"])); }
            for (const auto& key : {BoardParams::onId(kind, slot), BoardParams::trimId(kind, slot)}) set(key, static_cast<float>(saved.getChildWithProperty("id", key)["value"]));
        }
        for (const auto* stage : {"pedal", "pedal1", "ambience", "ambience1"}) for (const auto* suffix : {"Path", "Id"}) { const auto key = juce::String(stage) + suffix; state.setProperty(key, saved[key], nullptr); }
    } else if (action == "convert") {
        if (PedalboardState::serial(state)) return "This rig already uses a serial pedalboard.";
        state.removeChild(board, nullptr); board = PedalboardState::emptySerial(); state.addChild(board, -1, nullptr);
        for (int kind = 0; kind < 8; ++kind) {
            juce::ValueTree block("BLOCK"); block.setProperty("id", "block." + juce::Uuid().toString(), nullptr);
            block.setProperty("type", BoardParams::types[kind], nullptr); block.setProperty("automationSlot", 0, nullptr);
            block.setProperty("deleted", 0, nullptr);
            const bool pre = kind == 1 || kind == 2 || (kind == 0 && static_cast<int>(state.getChildWithProperty("id", "COMP_MODE")["value"]) != 2);
            block.setProperty("lane", pre ? "pre" : "post", nullptr); board.addChild(block, -1, nullptr);
        }
        if (static_cast<int>(state.getChildWithProperty("id", "COMP_MODE")["value"]) == 3) set(BoardParams::onId(0, 0), 0.f);
    } else {
        if (!PedalboardState::serial(state)) return "Enable serial editing first.";
        if (!args.isObject()) return "Invalid board edit.";
        auto block = board.getChildWithProperty("id", args["id"]);
        if (action != "add" && (!block.isValid() || static_cast<int>(block["deleted"]) != 0)) return "Pedal not found.";
        if (action == "add" || action == "duplicate" || action == "replace") {
            const int kind = action == "duplicate" ? PedalboardState::kind(block) : BoardParams::types.indexOf(args["type"].toString());
            if (kind < 0) return "Unsupported pedal type.";
            int slot = -1;
            for (int candidate = 0; candidate < 2; ++candidate) {
                bool reserved = false; for (const auto& row : board) if (PedalboardState::kind(row) == kind && static_cast<int>(row["automationSlot"]) == candidate) reserved = true;
                if (!reserved) { slot = candidate; break; }
            }
            if (slot < 0 || board.getNumChildren() >= 16) return "Both automation slots for this pedal are reserved. Undo removal or start from another rig.";
            juce::ValueTree added("BLOCK"); added.setProperty("id", "block." + juce::Uuid().toString(), nullptr);
            added.setProperty("type", BoardParams::types[kind], nullptr); added.setProperty("automationSlot", slot, nullptr); added.setProperty("deleted", 0, nullptr);
            auto lane = action == "duplicate" || action == "replace" ? block["lane"].toString() : args["lane"].toString();
            if (action == "replace" && (kind == 1 || kind == 2 || kind == 10)) lane = "pre";
            added.setProperty("lane", lane, nullptr); board.addChild(added, action == "duplicate" || action == "replace" ? board.indexOf(block) + 1 : -1, nullptr);
            if (action == "replace") block.setProperty("deleted", 1, nullptr);
            const int from = static_cast<int>(block["automationSlot"]);
            for (const auto& id : BoardParams::controls[static_cast<size_t>(kind)]) {
                float amount = kind == 8 ? 25.f : 0.f;
                if (action == "duplicate") amount = static_cast<float>(state.getChildWithProperty("id", BoardParams::parameter(kind, from, id))["value"]);
                else BoardParams::each([&](const auto& p) { if (id == p.id || BoardParams::parameter(kind, slot, id) == p.id) amount = p.initial; });
                set(BoardParams::parameter(kind, slot, id), amount);
            }
            set(BoardParams::onId(kind, slot), action == "duplicate" ? static_cast<float>(state.getChildWithProperty("id", BoardParams::onId(kind, from))["value"]) : ((kind == 2 || kind == 8) ? 0.f : 1.f));
            set(BoardParams::trimId(kind, slot), action == "duplicate" ? static_cast<float>(state.getChildWithProperty("id", BoardParams::trimId(kind, from))["value"]) : 0.f);
            if (kind == 2 || kind == 8) for (const auto* suffix : {"Path", "Id"}) {
                const juce::String base = kind == 8 ? "ambience" : "pedal";
                const auto source = base + (from == 0 ? "" : "1") + suffix, target = base + (slot == 0 ? "" : "1") + suffix;
                state.setProperty(target, action == "duplicate" ? state[source] : juce::var(), nullptr);
            }
        } else if (action == "remove") block.setProperty("deleted", 1, nullptr);
        else if (action == "lane") block.setProperty("lane", args["lane"], nullptr);
        else if (action == "moveTo") {
            const auto target = board.getChildWithProperty("id", args["beforeId"]);
            const auto lane = args["lane"].toString();
            if (lane != "pre" && lane != "post") return "Invalid pedal lane.";
            if (target.isValid() && (static_cast<int>(target["deleted"]) != 0 || target["lane"].toString() != lane)) return "Invalid target pedal.";
            if (args["beforeId"].toString().isNotEmpty() && !target.isValid()) return "Target pedal not found.";
            if (target == block) return {};
            block.setProperty("lane", lane, nullptr);
            const int current = board.indexOf(block), destination = target.isValid() ? board.indexOf(target) - (current < board.indexOf(target) ? 1 : 0) : board.getNumChildren() - 1;
            board.moveChild(current, destination, nullptr);
        }
        else if (action == "move") {
            if (!args["direction"].isInt() || (static_cast<int>(args["direction"]) != -1 && static_cast<int>(args["direction"]) != 1)) return "Invalid pedal move.";
            const int direction = static_cast<int>(args["direction"]), current = board.indexOf(block);
            int destination = current + direction;
            while (destination >= 0 && destination < board.getNumChildren()) {
                const auto row = board.getChild(destination);
                if (row["lane"] == block["lane"] && static_cast<int>(row["deleted"]) == 0) break;
                destination += direction;
            }
            if (destination >= 0 && destination < board.getNumChildren()) board.moveChild(current, destination, nullptr);
            else return {};
        } else if (action == "capture") {
            const int kind = PedalboardState::kind(block); const juce::String assetKind = kind == 8 ? "ambience" : "pedal";
            if ((kind != 2 && kind != 8) || !args["assetId"].isString()) return "Choose a captured pedal.";
            const auto asset = library.find(args["assetId"].toString());
            if (!asset.hasType("ASSET") || asset["kind"].toString() != assetKind) return "Pedal capture not found.";
            const juce::String stage = assetKind + (static_cast<int>(block["automationSlot"]) == 0 ? "" : "1");
            state.setProperty(stage + "Path", asset["path"], nullptr); state.setProperty(stage + "Id", asset["id"], nullptr);
            if (args["enable"].isBool() && static_cast<bool>(args["enable"])) set(BoardParams::onId(kind, static_cast<int>(block["automationSlot"])), 1.f);
        } else return "Unknown board edit.";
    }
    auto document = std::make_unique<juce::DynamicObject>(); document->setProperty("schema", 3); document->setProperty("state", state.toXmlString());
    if (const auto failure = applyRig(juce::var(document.release())); failure.isNotEmpty()) return failure;
    pendingBoardBefore = before; pendingBoardAction = action; pendingBoardGeneration = requestGeneration.load(); return {};
}
