#include "PluginProcessor.h"
#include <CassianRigData.h>

juce::var AmpSuiteAudioProcessor::startingRigCatalog()
{
    return juce::JSON::parse(juce::String::fromUTF8(CassianRigs::startingRigs_json, CassianRigs::startingRigs_jsonSize));
}

juce::String AmpSuiteAudioProcessor::loadStartingRig(const juce::String& id)
{
    const juce::ScopedLock guard(requestLock);
    if (sceneAssetsLoading()) return "Finish loading before selecting a starter rig.";
    const auto catalog = startingRigCatalog(); juce::var selected;
    if (static_cast<int>(catalog["version"]) != 1 || !catalog["rigs"].isArray()) return "Invalid starter rig catalog.";
    for (const auto* group : {"rigs", "captureRigs"})
        if (const auto* rows = catalog[group].getArray()) for (const auto& rig : *rows) if (rig["id"].toString() == id) selected = rig;
    if (!selected.isObject()) return "Starter rig not found.";
    juce::ValueTree state("AmpSuiteState");
    BoardParams::each([&](const auto& definition) {
        juce::ValueTree row("PARAM"); row.setProperty("id", definition.id, nullptr);
        row.setProperty("value", PerformanceScenes::global(definition.id) ? apvts.getRawParameterValue(definition.id)->load() : definition.initial, nullptr);
        state.addChild(row, -1, nullptr);
    });
    for (const auto& values : {catalog["base"], selected["parameters"]}) {
        if (!values.isObject()) return "Invalid starter rig controls.";
        for (const auto& field : values.getDynamicObject()->getProperties()) {
            const auto key = field.name.toString(); auto row = state.getChildWithProperty("id", key);
            if (!row.isValid() || PerformanceScenes::global(key)) return "Invalid starter rig parameter.";
            row.setProperty("value", field.value, nullptr);
        }
    }
    auto board = PedalboardState::emptySerial();
    if (!selected["board"].isArray()) return "Invalid starter pedalboard.";
    for (const auto& entry : *selected["board"].getArray()) {
        juce::ValueTree block("BLOCK");
        block.setProperty("id", id + "." + entry["type"].toString(), nullptr);
        block.setProperty("type", entry["type"], nullptr); block.setProperty("lane", entry["lane"], nullptr);
        block.setProperty("automationSlot", 0, nullptr); block.setProperty("deleted", 0, nullptr);
        board.addChild(block, -1, nullptr);
    }
    state.addChild(board, -1, nullptr);
    // Exact content identities distinguish capture variants. Missing files reject
    // before a request is queued; never substitute another amp or pedal.
    if (selected["assets"].isObject()) {
        juce::ValueTree references("LIBRARY");
        for (const auto& field : selected["assets"].getDynamicObject()->getProperties()) {
            const auto stage = field.name.toString(), assetId = field.value["id"].toString();
            const auto asset = library.find(assetId);
            if (!asset.hasType("ASSET") || !AssetLibrary::exists(asset["path"].toString()))
                return "Missing sound: " + field.value["name"].toString() + ". Import its sound pack in Library.";
            state.setProperty(stage + "Path", asset["path"], nullptr);
            state.setProperty(stage + "Id", assetId, nullptr);
            references.addChild(asset.createCopy(), -1, nullptr);
        }
        state.addChild(references, -1, nullptr);
    }
    PerformanceScenes emptyScenes; state.addChild(emptyScenes.save(), -1, nullptr);
    ActiveRig identity; identity.set(id, selected["name"].toString(), state); state.addChild(identity.save(), -1, nullptr);
    auto document = std::make_unique<juce::DynamicObject>(); document->setProperty("schema", 3); document->setProperty("state", state.toXmlString());
    // Prepared complete recall replaces external assets and old board/scenes.
    // Input calibration, listening levels and performance controls stay global.
    return applyRig(juce::var(document.release()));
}
