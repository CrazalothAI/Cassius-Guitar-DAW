#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

AmpSuiteAudioProcessorEditor::AmpSuiteAudioProcessorEditor(AmpSuiteAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    auto options = juce::WebBrowserComponent::Options()
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2()
            .withUserDataFolder(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("AmpSuite/WebView2")))
        .withNativeIntegrationEnabled()
        .withResourceProvider([](const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
        {
            if (path != "/" && path != "/index.html") return std::nullopt;
            const auto* bytes = reinterpret_cast<const std::byte*>(BinaryData::index_html);
            return juce::WebBrowserComponent::Resource {{bytes, bytes + BinaryData::index_htmlSize}, "text/html"};
        })
        .withNativeFunction("loadModel", [this](const auto&, auto complete) { chooseFile(0); complete(true); })
        .withNativeFunction("loadIRB", [this](const auto&, auto complete) { chooseFile(3); complete(true); })
        .withNativeFunction("loadIR", [this](const auto&, auto complete) { chooseFile(1); complete(true); })
        .withNativeFunction("loadPedal", [this](const auto&, auto complete) { chooseFile(2); complete(true); })
        .withNativeFunction("getLibrary", [this](const auto&, auto complete) { complete(processor.getLibrary()); })
        .withNativeFunction("getBackupStatus", [this](const auto&, auto complete) { complete(processor.backupStatus()); })
        .withNativeFunction("getToneRecovery", [this](const auto&, auto complete) { complete(processor.toneRecoveryStatus()); })
        .withNativeFunction("captureRecoveryTone", [this](const auto&, auto complete) { complete(processor.captureRecoveryTone()); })
        .withNativeFunction("setAutomaticRecovery", [this](const auto& args, auto complete) { complete(args.size() == 1 && args[0].isBool() ? processor.setAutomaticRecovery(static_cast<bool>(args[0])) : juce::String("Choose an automatic recovery setting.")); })
        .withNativeFunction("recoverTone", [this](const auto& args, auto complete) { complete(args.size() == 1 && args[0].isString() ? processor.recoverTone(args[0].toString()) : juce::String("Choose an available tone snapshot.")); })
        .withNativeFunction("createBackup", [this](const auto& args, auto complete) { complete(args.size() == 0 || (args.size() == 1 && args[0].isBool()) ? chooseBackup(false, args.size() == 0 || static_cast<bool>(args[0])) : juce::String("Choose whether to include recorded takes.")); })
        .withNativeFunction("createSelectedBackup", [this](const auto& args, auto complete) {
            juce::StringArray ids;
            if (args.size() != 1 || !args[0].isArray() || args[0].size() == 0 || args[0].size() > 2048) { complete(juce::String("Choose at least one recorded take.")); return; }
            for (const auto& id : *args[0].getArray()) {
                if (!id.isString() || id.toString().isEmpty() || id.toString().length() > 128 || ids.contains(id.toString())) { complete(juce::String("Invalid or duplicate backup take selection.")); return; }
                ids.add(id.toString());
            }
            complete(chooseBackup(false, true, ids));
        })
        .withNativeFunction("restoreBackup", [this](const auto&, auto complete) { complete(chooseBackup(true)); })
        .withNativeFunction("cancelBackup", [this](const auto&, auto complete) { processor.cancelBackup(); complete(juce::String()); })
        .withNativeFunction("revealBackup", [this](const auto&, auto complete) { complete(processor.revealBackup()); })
        .withNativeFunction("inspectRig", [this](const auto& args, auto complete) { complete(processor.inspectRig(args.size() == 1 && args[0].isString() ? args[0].toString() : juce::String())); })
        .withNativeFunction("importAssets", [this](const auto& args, auto complete) {
            const auto kind = args.size() == 1 ? args[0].toString() : juce::String();
            const bool valid = kind == "amp" || kind == "pedal" || kind == "cab" || kind == "ambience" || kind == "pack";
            if (valid) chooseImports(kind); complete(valid);
        })
        .withNativeFunction("boardCommand", [this](const auto& args, auto complete) { complete(args.size() == 2 && args[0].isString() ? processor.boardCommand(args[0].toString(), args[1]) : juce::String("Invalid board request.")); })
        .withNativeFunction("getRig", [this](const auto&, auto complete) { complete(processor.getRig()); })
        .withNativeFunction("storeScene", [this](const auto& args, auto complete) { complete(args.size() == 2 && args[0].isInt() && args[1].isString() ? processor.storeScene(static_cast<int>(args[0]), args[1].toString()) : juce::String("Invalid scene request.")); })
        .withNativeFunction("renameScene", [this](const auto& args, auto complete) { complete(args.size() == 2 && args[0].isInt() && args[1].isString() ? processor.renameScene(static_cast<int>(args[0]), args[1].toString()) : juce::String("Invalid scene request.")); })
        .withNativeFunction("copyScene", [this](const auto& args, auto complete) { complete(args.size() == 3 && args[0].isInt() && args[1].isInt() && args[2].isString() ? processor.copyScene(static_cast<int>(args[0]), static_cast<int>(args[1]), args[2].toString()) : juce::String("Invalid scene request.")); })
        .withNativeFunction("recallScene", [this](const auto& args, auto complete) { complete(args.size() == 1 && args[0].isInt() ? processor.recallScene(static_cast<int>(args[0])) : juce::String("Invalid scene request.")); })
        .withNativeFunction("clearScene", [this](const auto& args, auto complete) { complete(args.size() == 1 && args[0].isInt() ? processor.clearScene(static_cast<int>(args[0])) : juce::String("Invalid scene request.")); })
        .withNativeFunction("applyRig", [this](const auto& args, auto complete) { complete(args.size() == 1 || args.size() == 2 ? processor.applyRig(args[0], true, args.size() == 2 && static_cast<bool>(args[1])) : "Invalid rig request."); })
        .withNativeFunction("saveRig", [this](const auto& args, auto complete) { complete(args.size() == 1 ? processor.saveRig(args[0].toString()) : "Give the rig a name."); })
        .withNativeFunction("updateActiveRig", [this](const auto&, auto complete) { complete(processor.updateActiveRig()); })
        .withNativeFunction("loadRig", [this](const auto& args, auto complete) { complete(args.size() == 1 ? processor.loadRig(args[0].toString()) : "Rig not found."); })
        .withNativeFunction("loadStartingRig", [this](const auto& args, auto complete) { complete(args.size() == 1 && args[0].isString() ? processor.loadStartingRig(args[0].toString()) : juce::String("Choose a starter rig.")); })
        .withNativeFunction("removeRig", [this](const auto& args, auto complete) { complete(args.size() == 1 && processor.removeRig(args[0].toString())); })
        .withNativeFunction("editRig", [this](const auto& args, auto complete) { complete(args.size() == 2 && args[0].isString() ? processor.editRig(args[0].toString(), args[1]) : juce::String("Choose a saved rig and valid metadata.")); })
        .withNativeFunction("duplicateRig", [this](const auto& args, auto complete) { complete(args.size() == 2 && args[0].isString() && args[1].isString() ? processor.duplicateRig(args[0].toString(), args[1].toString()) : juce::String("Choose a saved rig and name for its copy.")); })
        .withNativeFunction("selectAsset", [this](const auto& args, auto complete) { complete((args.size() == 1 || (args.size() == 2 && args[1].toString() == "cabB")) && processor.selectAsset(args[0].toString(), args.size() == 2)); })
        .withNativeFunction("editAsset", [this](const auto& args, auto complete) { complete(args.size() == 2 && processor.editAsset(args[0].toString(), args[1])); })
        .withNativeFunction("exportRig", [this](const auto&, auto complete) { complete(chooseRigFile(true)); })
        .withNativeFunction("importRig", [this](const auto&, auto complete) { complete(chooseRigFile(false)); })
        .withNativeFunction("exportRigPack", [this](const auto&, auto complete) { complete(chooseRigFile(true, true)); })
        .withNativeFunction("importRigPack", [this](const auto&, auto complete) { complete(chooseRigFile(false, true)); })
        .withNativeFunction("exportSavedRig", [this](const auto& args, auto complete) { complete(args.size() == 2 && args[0].isString() && args[0].toString().isNotEmpty() && args[1].isBool() ? chooseRigFile(true, static_cast<bool>(args[1]),args[0].toString()) : juce::String("Choose a saved rig and export format.")); })
        .withNativeFunction("relinkAsset", [this](const auto& args, auto complete) { if (args.size() == 1) chooseRelink(args[0].toString()); complete(args.size() == 1); })
        .withNativeFunction("selectAmpVoice", [this](const auto& args, auto complete) { complete(args.size() == 1 && processor.selectAmpVoice(args[0].toString())); })
        .withNativeFunction("clearStage", [this](const auto& args, auto complete)
        {
            // An empty path unloads the stage on the loader thread.
            const auto stage = args.size() == 1 ? args[0].toString() : juce::String();
            if (stage == "pedal") processor.requestPedal(juce::File());
            else if (stage == "cabB") processor.requestCabB(juce::File());
            else if (stage == "amp" || stage == "cab") processor.requestFile(stage == "amp", juce::File());
            complete(stage == "amp" || stage == "pedal" || stage == "cab" || stage == "cabB");
        })
        .withNativeFunction("setBufferSize", [this](const auto& args, auto complete)
        {
            // Standalone only: a DAW owns its own buffer size.
            if (args.size() != 1 || !processor.setDeviceBufferSize) { complete(juce::String("Set the buffer size in your DAW's audio settings.")); return; }
            complete(processor.setDeviceBufferSize(static_cast<int>(args[0])));
        })
        .withNativeFunction("setTuner", [this](const auto& args, auto complete) { processor.setTunerActive(args.size() == 1 && static_cast<bool>(args[0])); complete(true); })
        .withNativeFunction("showAudioSettings", [this](const auto&, auto complete) {
            if (processor.showDeviceSettings) { processor.showDeviceSettings(); complete(juce::String()); }
            else complete(juce::String("Choose the interface in your DAW's audio settings."));
        })
        .withNativeFunction("setInputChannel", [this](const auto& args, auto complete) {
            if (args.size() != 1 || !processor.setDeviceInputChannel) { complete(juce::String("Choose the guitar input in your DAW.")); return; }
            complete(processor.setDeviceInputChannel(static_cast<int>(args[0])));
        })
        .withNativeFunction("loadBackingTrack", [this](const auto&, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Use your DAW's backing-track transport.")); return; }
            choosePractice(false); complete(juce::String());
        })
        .withNativeFunction("practiceControl", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Use your DAW's transport and recording.")); return; }
            if (args.size() != 2 || !args[0].isString() || !(args[1].isInt() || args[1].isInt64() || args[1].isDouble())) { complete(juce::String("Invalid practice request.")); return; }
            complete(processor.practice.command(args[0].toString(), static_cast<double>(args[1])));
        })
        .withNativeFunction("getPracticeWaveform", [this](const auto&, auto complete) { complete(processor.practice.waveform()); })
        .withNativeFunction("getPracticeJournal", [this](const auto&, auto complete) { complete(processor.practiceJournal ? processor.practiceJournal->document() : juce::var()); })
        .withNativeFunction("practiceJournalCommand", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings || !processor.practiceJournal) { complete(juce::String("Practice history is available in standalone.")); return; }
            if (args.size() != 2 || !args[0].isString()) { complete(juce::String("Invalid practice history request.")); return; }
            if (static_cast<bool>(processor.backupStatus()["busy"])) { complete(juce::String("Finish backup/recovery before editing practice history.")); return; }
            const auto action = args[0].toString();
            if (action != "saveSet" && action != "removeSet" && action != "start" && action != "pause" && action != "resume" && action != "finish" && action != "removeSession" && action != "addRecording" && action != "removeRecording") { complete(juce::String("Unknown practice history action.")); return; }
            if (action == "addRecording" && !processor.takes.containsVersion(args[1]["takeId"].toString(), args[1]["version"].toString())) { complete(juce::String("That recording version is unavailable. Refresh the take library or import its folder first.")); return; }
            complete(processor.practiceJournal->command(action, args[1]));
        })
        .withNativeFunction("transferPracticeJournal", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings || !processor.practiceJournal || args.size() != 1 || !args[0].isBool()) { complete(juce::String("Choose a practice history import or export in standalone.")); return; }
            if (static_cast<bool>(processor.backupStatus()["busy"]) || static_cast<int>(processor.practice.status()["recordMode"]) != 0) { complete(juce::String("Finish recording and backup before transferring practice history.")); return; }
            choosePracticeJournal(static_cast<bool>(args[0])); complete(juce::String());
        })
        .withNativeFunction("getTakeReviewWaveform", [this](const auto& args, auto complete) { complete(processor.takes.reviewWaveform(args.size() == 2 && args[0].isString() ? args[0].toString() : juce::String(), args.size() == 2 && args[1].isString() ? args[1].toString() : juce::String())); })
        .withNativeFunction("savePracticeSection", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Practice sections are available in standalone.")); return; }
            complete(args.size() == 2 && args[0].isString() && args[1].isString() ? processor.practice.saveSection(args[0].toString(), args[1].toString()) : juce::String("Invalid section request."));
        })
        .withNativeFunction("recallPracticeSection", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Practice sections are available in standalone.")); return; }
            complete(args.size() == 1 && args[0].isString() ? processor.practice.recallSection(args[0].toString()) : juce::String("Invalid section request."));
        })
        .withNativeFunction("removePracticeSection", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Practice sections are available in standalone.")); return; }
            complete(args.size() == 1 && args[0].isString() ? processor.practice.removeSection(args[0].toString()) : juce::String("Invalid section request."));
        })
        .withNativeFunction("practiceStart", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Use your DAW's transport and recording.")); return; }
            if (static_cast<bool>(processor.backupStatus()["busy"])) { complete(juce::String("Finish backup/recovery before starting a recording.")); return; }
            if (args.size() != 2 || (args[0].toString() != "play" && args[0].toString() != "record") || !(args[1].isInt() || args[1].isInt64() || args[1].isDouble()) || !std::isfinite(static_cast<double>(args[1])) || static_cast<double>(args[1]) < 0 || static_cast<double>(args[1]) > 2 || static_cast<double>(args[1]) != std::floor(static_cast<double>(args[1]))) { complete(juce::String("Invalid count-in request.")); return; }
            processor.practice.setCountIn(static_cast<int>(args[1]), processor.apvts.getRawParameterValue("METRO_BPM")->load(), juce::roundToInt(processor.apvts.getRawParameterValue("METRO_BEATS")->load()));
            processor.takes.stopReview();
            if (args[0].toString() == "record") { choosePractice(true); complete(juce::String()); }
            else complete(processor.practice.command("play"));
        })
        .withNativeFunction("openTakeFolder", [this](const auto&, auto complete) {
            const juce::File folder(processor.practice.status()["takePath"].toString());
            if (folder.isDirectory()) folder.revealToUser(); complete(folder.isDirectory());
        })
        .withNativeFunction("getTakes", [this](const auto&, auto complete) { complete(processor.takes.list()); })
        .withNativeFunction("restoreTakeRig", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings || static_cast<int>(processor.practice.status()["recordMode"]) != 0 || static_cast<bool>(processor.status()["rigLoading"])) { complete(juce::String("Finish recording and rig loading before recovering a take tone.")); return; }
            if (args.size() != 2 || !args[0].isString() || !args[1].isString()) { complete(juce::String("Choose a take version to recover.")); return; }
            const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
            const auto failure = processor.takes.readRigSnapshot(args[0].toString(), args[1].toString(), [safe, complete](juce::var rig) {
                juce::MessageManager::callAsync([safe, complete, rig = std::move(rig)]() mutable {
                    if (safe == nullptr) return;
                    if (rig.hasProperty("error")) { complete(rig["error"].toString()); return; }
                    if (static_cast<bool>(safe->processor.takes.status()["exporting"])) { complete(juce::String("Finish or cancel the export before recovering a rig.")); return; }
                    if (static_cast<int>(safe->processor.practice.status()["recordMode"]) != 0 || static_cast<bool>(safe->processor.status()["rigLoading"])) { complete(juce::String("Finish recording and rig loading before recovering a take tone.")); return; }
                    const auto error = safe->processor.applyRig(rig, true);
                    if (error.isEmpty()) { safe->processor.takes.stopReview(); safe->processor.practice.command("pause"); }
                    complete(error);
                });
            });
            if (failure.isNotEmpty()) complete(failure);
        })
        .withNativeFunction("importTake", [this](const auto&, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Take review is available in the standalone app.")); return; }
            chooseTakeFolder(); complete(juce::String());
        })
        .withNativeFunction("recoverRecording", [this](const auto& args, auto complete) {
            if (args.size()!=0 || !processor.showDeviceSettings || static_cast<int>(processor.practice.status()["recordMode"])!=0 || static_cast<bool>(processor.takes.status()["exporting"]) || static_cast<bool>(processor.backupStatus()["busy"])) { complete(juce::String("Finish recording, export and backup before recovering a recording in standalone.")); return; }
            chooseTakeFolder(true); complete(juce::String());
        })
        .withNativeFunction("confirmTakeRecovery", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings || static_cast<int>(processor.practice.status()["recordMode"])!=0 || args.size()!=3 || !args[0].isString() || !args[1].isBool() || !static_cast<bool>(args[1]) || !args[2].isBool()) { complete(juce::String("Listen to the recovered processed audio and confirm your review in standalone.")); return; }
            complete(processor.takes.confirmRecovery(args[0].toString(),static_cast<bool>(args[2])));
        })
        .withNativeFunction("revealRecordingRecovery", [this](const auto&, auto complete) { complete(processor.takes.revealRecovery()); })
        .withNativeFunction("editTake", [this](const auto& args, auto complete) {
            complete(args.size() == 3 && args[0].isString() && args[1].isString() && args[2].isBool() ? processor.takes.edit(args[0].toString(), args[1].toString(), static_cast<bool>(args[2])) : juce::String("Invalid take edit."));
        })
        .withNativeFunction("saveTakeNotes", [this](const auto& args, auto complete) {
            complete(args.size() == 2 && args[0].isString() && args[1].isString() ? processor.takes.annotate(args[0].toString(), args[1].toString()) : juce::String("Choose a take and valid notes."));
        })
        .withNativeFunction("renameTakeVersion", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings || static_cast<int>(processor.practice.status()["recordMode"]) != 0) { complete(juce::String("Finish recording before renaming a take version in standalone.")); return; }
            complete(args.size() == 3 && args[0].isString() && args[1].isString() && args[2].isString() ? processor.takes.renameVersion(args[0].toString(),args[1].toString(),args[2].toString()) : juce::String("Choose a reamp version and name."));
        })
        .withNativeFunction("previewTake", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings || static_cast<int>(processor.practice.status()["recordMode"]) != 0) { complete(juce::String("Finish the take before reviewing audio in standalone.")); return; }
            if (args.size() != 2 || !args[0].isString() || !args[1].isString()) { complete(juce::String("Invalid take preview.")); return; }
            processor.practice.command("pause"); complete(processor.takes.preview(args[0].toString(), args[1].toString()));
        })
        .withNativeFunction("reviewControl", [this](const auto& args, auto complete) {
            if (args.size() != 2 || !args[0].isString() || !(args[1].isInt() || args[1].isInt64() || args[1].isDouble())) { complete(juce::String("Invalid review control.")); return; }
            const auto name = args[0].toString();
            if (name == "stop") { processor.takes.stopReview(); complete(juce::String()); }
            else if (name == "level" || name == "seek") complete(processor.takeReview.command(name, static_cast<double>(args[1])));
            else complete(juce::String("Unknown review control."));
        })
        .withNativeFunction("takeReviewControl", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings || static_cast<int>(processor.practice.status()["recordMode"]) != 0) { complete(juce::String("Finish recording before reviewing a take in standalone.")); return; }
            if (args.size() != 4 || !args[0].isString() || !args[1].isString() || !args[2].isString() || !(args[3].isInt() || args[3].isInt64() || args[3].isDouble())) { complete(juce::String("Invalid take review control.")); return; }
            complete(processor.takes.reviewControl(args[0].toString(), args[1].toString(), args[2].toString(), static_cast<double>(args[3])));
        })
        .withNativeFunction("takeReviewSection", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings || static_cast<int>(processor.practice.status()["recordMode"]) != 0) { complete(juce::String("Finish recording before editing take sections in standalone.")); return; }
            bool valid = args.size() == 5; for (const auto& arg : args) valid = valid && arg.isString();
            complete(valid ? processor.takes.reviewSection(args[0].toString(), args[1].toString(), args[2].toString(), args[3].toString(), args[4].toString()) : juce::String("Invalid take section request."));
        })
        .withNativeFunction("reampTake", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Reamping is available in the standalone app.")); return; }
            if (static_cast<int>(processor.practice.status()["recordMode"]) != 0) { complete(juce::String("Finish the recording before starting an export.")); return; }
            const auto numeric = [](const juce::var& v) { return v.isInt() || v.isInt64() || v.isDouble(); };
            complete((args.size() == 1 || (args.size() == 2 && numeric(args[1]))) && args[0].isString() ? processor.takes.reamp(args[0].toString(), processor.getRig(), args.size() == 2 ? static_cast<double>(args[1]) : 0) : juce::String("Choose a take and valid tail to reamp."));
        })
        .withNativeFunction("cancelReamp", [this](const auto&, auto complete) { processor.takes.cancelExport(); complete(juce::String()); })
        .withNativeFunction("exportVideoAudio", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings || static_cast<int>(processor.practice.status()["recordMode"]) != 0) { complete(juce::String("Finish recording in standalone before exporting.")); return; }
            const auto numeric = [](const juce::var& v) { return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite(static_cast<double>(v)); };
            if ((args.size() != 5 && args.size() != 8) || !args[0].isString() || !args[1].isString() || !args[2].isBool()
                || !numeric(args[3]) || !numeric(args[4]) || (args.size() == 8 && (!numeric(args[5]) || !numeric(args[6]) || !numeric(args[7])))) { complete(juce::String("Invalid video export request.")); return; }
            const auto start = args.size() == 8 ? static_cast<double>(args[5]) : 0;
            const auto end = args.size() == 8 ? static_cast<double>(args[6]) : -1;
            const auto fade = args.size() == 8 ? static_cast<double>(args[7]) : 0;
            if (start < 0 || (end != -1 && end <= start) || fade < 0 || fade > .1 || static_cast<double>(args[3]) < -60 || static_cast<double>(args[3]) > 12 || static_cast<double>(args[4]) < -60 || static_cast<double>(args[4]) > 12) { complete(juce::String("Invalid export range, fade or balance.")); return; }
            chooseVideoAudio(args[0].toString(), args[1].toString(), static_cast<bool>(args[2]), static_cast<float>(args[3]), static_cast<float>(args[4]), start, end, fade); complete(juce::String());
        })
        .withNativeFunction("revealVideoExport", [this](const auto&, auto complete) { complete(processor.takes.revealExport()); })
        .withNativeFunction("revealTake", [this](const auto& args, auto complete) { complete(args.size() == 1 && args[0].isString() ? processor.takes.reveal(args[0].toString()) : juce::String("Take not found.")); })
        .withNativeFunction("setMidiEnabled", [this](const auto& args, auto complete) {
            if (args.size() != 1 || !args[0].isBool()) { complete(juce::String("Invalid MIDI enable request.")); return; }
            processor.midiControl.enable(static_cast<bool>(args[0])); complete(juce::String());
        })
        .withNativeFunction("setMidiMapping", [this](const auto& args, auto complete) {
            complete(args.size() == 2 && args[0].isInt() ? processor.midiControl.setMapping(static_cast<int>(args[0]), args[1]) : juce::String("Invalid MIDI assignment."));
        })
        .withNativeFunction("learnMidi", [this](const auto& args, auto complete) {
            complete(args.size() == 1 && args[0].isInt() ? processor.midiControl.learn(static_cast<int>(args[0])) : juce::String("Invalid MIDI learn request."));
        })
        .withNativeFunction("setMidiInput", [this](const auto& args, auto complete) {
            complete(args.size() == 2 && args[0].isString() && args[1].isBool() && processor.setMidiInput ? processor.setMidiInput(args[0].toString(), static_cast<bool>(args[1])) : juce::String("Route MIDI through your DAW."));
        })
        .withNativeFunction("getStatus", [this](const auto&, auto complete) {
            auto status = processor.status(); if (processor.midiInputs) status.getDynamicObject()->setProperty("midiInputs", processor.midiInputs());
            if (processor.deviceSummary) status.getDynamicObject()->setProperty("audioDevice", processor.deviceSummary()); complete(status);
        })
        .withNativeFunction("setInputMonitoring", [this](const auto& args, auto complete) {
            complete(args.size() == 1 && args[0].isBool() && processor.setInputMonitoring ? processor.setInputMonitoring(static_cast<bool>(args[0])) : juce::String("Your DAW controls input monitoring."));
        })
        .withNativeFunction("copySupportReport", [](const auto& args, auto complete) {
            if (args.size() != 1 || !args[0].isString() || args[0].toString().length() > 8192) { complete(juce::String("Invalid support report.")); return; }
            juce::SystemClipboard::copyTextToClipboard(args[0].toString()); complete(juce::String());
        })
        .withNativeFunction("openHelpLink", [](const auto& args, auto complete) {
            const auto key = args.size() == 1 && args[0].isString() ? args[0].toString() : juce::String();
            const char* url = key == "guide" ? "https://github.com/CrazalothAI/Cassius-Guitar-DAW/blob/main/docs/USER-GUIDE.md"
                : key == "support" ? "https://github.com/CrazalothAI/Cassius-Guitar-DAW/issues"
                : key == "source" ? "https://github.com/CrazalothAI/Cassius-Guitar-DAW"
                : key == "license" ? "https://github.com/CrazalothAI/Cassius-Guitar-DAW/blob/main/LICENSE.txt" : nullptr;
            complete(url != nullptr && juce::URL(url).launchInDefaultBrowser() ? juce::String() : juce::String("Could not open this help link."));
        });
    juce::StringArray relayIds; BoardParams::each([&](const auto& p) { relayIds.add(p.id); });
    for (const auto& id : relayIds)
    {
        relays.push_back(std::make_unique<juce::WebSliderRelay>(id));
        options = options.withOptionsFrom(*relays.back());
    }
    webView = std::make_unique<juce::WebBrowserComponent>(options);
    for (size_t i = 0; i < relays.size(); ++i)
        attachments.push_back(std::make_unique<juce::WebSliderParameterAttachment>(
            *processor.apvts.getParameter(relayIds[static_cast<int>(i)]), *relays[i], nullptr));
    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    setResizable(true, true); setResizeLimits(860, 620, 1800, 1200); setSize(1100, 760);
}
// A closed editor cannot show the tuner, so stop its analysis.
AmpSuiteAudioProcessorEditor::~AmpSuiteAudioProcessorEditor()
{
    processor.setTunerActive(false);
    if (static_cast<int>(processor.midiControl.status()["learning"]) >= 0)
        processor.midiControl.learn(-1);
}
void AmpSuiteAudioProcessorEditor::resized() { webView->setBounds(getLocalBounds()); }
void AmpSuiteAudioProcessorEditor::chooseVideoAudio(const juce::String& id, const juce::String& version, bool backing, float guitarDb, float backingDb, double start, double end, double fade)
{
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>("Save video soundtrack (48 kHz / 24-bit stereo WAV)",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Cassian soundtrack.wav"), "*.wav");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, id, version, backing, guitarDb, backingDb, start, end, fade](const juce::FileChooser& dialog) {
            if (safe == nullptr) return;
            const auto destination = dialog.getResult();
            if (destination != juce::File()) {
                const auto failure = static_cast<int>(safe->processor.practice.status()["recordMode"]) != 0 ? juce::String("Finish recording before exporting.")
                    : safe->processor.takes.videoExport(id, version, destination.withFileExtension("wav"), backing, guitarDb, backingDb, start, end, fade);
                if (failure.isNotEmpty()) safe->processor.reportLibraryResult("Load failed: " + failure);
            }
            safe->chooser.reset();
        });
}
void AmpSuiteAudioProcessorEditor::choosePracticeJournal(bool save)
{
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>(save ? "Export practice sets and history" : "Import practice sets and history", juce::File(), "*.json");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync((save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles,
        [safe, save](const juce::FileChooser& dialog) {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (file != juce::File() && safe->processor.practiceJournal) {
                const auto blocked = static_cast<int>(safe->processor.practice.status()["recordMode"]) != 0 || static_cast<bool>(safe->processor.backupStatus()["busy"]);
                const auto failure = blocked ? juce::String("Finish recording and backup before transferring practice history.") : safe->processor.practiceJournal->command(save ? "export" : "import", {}, save ? file.withFileExtension("json") : file);
                if (failure.isNotEmpty()) safe->processor.reportLibraryResult(failure);
            }
            juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->chooser.reset(); });
        });
}

void AmpSuiteAudioProcessorEditor::choosePractice(bool recording)
{
    if (chooser) return;
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    chooser = std::make_unique<juce::FileChooser>(recording ? "Choose a folder for the new guitar take" : "Load a backing track",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), recording ? juce::String() : formats.getWildcardForAllFormats());
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | (recording ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles),
        [safe, recording](const juce::FileChooser& dialog) {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (recording && file.isDirectory()) {
                if (static_cast<bool>(safe->processor.backupStatus()["busy"])) { safe->processor.reportLibraryResult("Finish backup/recovery before starting a recording."); safe->chooser.reset(); return; }
                const auto rig = safe->processor.getRig();
                const auto failure = rig.hasProperty("error") ? rig["error"].toString() : safe->processor.practice.record(file, rig);
                if (failure.isNotEmpty()) safe->processor.reportLibraryResult("Load failed: " + failure);
            } else if (!recording && file.existsAsFile()) safe->processor.practice.load(file);
            safe->chooser.reset();
        });
}
void AmpSuiteAudioProcessorEditor::chooseTakeFolder(bool recover)
{
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>(recover ? "Recover an interrupted Cassian take into a separate copy" : "Import a Cassian take folder", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe,recover](const juce::FileChooser& dialog) {
            if (safe == nullptr) return;
            const auto folder = dialog.getResult();
            if (folder.isDirectory()) {
                if (recover) {
                    const auto failure=static_cast<int>(safe->processor.practice.status()["recordMode"])!=0 || static_cast<bool>(safe->processor.backupStatus()["busy"]) ? juce::String("Finish recording and backup before recovery.") : safe->processor.takes.recoverFolder(folder);
                    if (failure.isNotEmpty()) safe->processor.reportLibraryResult(failure);
                } else safe->processor.takes.importFolder(folder);
            }
            safe->chooser.reset();
        });
}
void AmpSuiteAudioProcessorEditor::chooseFile(int stage)
{
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>(stage == 2 ? "Load pedal capture" : stage == 0 ? "Load neural amp capture" : "Load cabinet impulse response",
        juce::File(), (stage == 1 || stage == 3) ? "*.wav" : "*.nam");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, stage](const juce::FileChooser& dialog)
        {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (file.existsAsFile())
            {
                if (stage == 2) safe->processor.requestPedal(file);
                else if (stage == 3) {
                    safe->processor.requestCabB(file);
                    auto* on = safe->processor.apvts.getParameter("CAB_B_ON"); on->setValueNotifyingHost(1);
                    auto* mode = safe->processor.apvts.getParameter("CAB_MODE"); mode->setValueNotifyingHost(mode->convertTo0to1(1));
                }
                else
                {
                    safe->processor.requestFile(stage == 0, file);
                    if (stage == 0)
                    {
                        auto* source = safe->processor.apvts.getParameter("AMP_SOURCE"); source->setValueNotifyingHost(source->convertTo0to1(3));
                        auto* type = safe->processor.apvts.getParameter("CAPTURE_KIND"); type->setValueNotifyingHost(0);
                    }
                }
            }
            safe->chooser.reset();
        });
}
juce::String AmpSuiteAudioProcessorEditor::chooseBackup(bool restore, bool includeTakes, std::optional<juce::StringArray> selectedTakeIds)
{
    if (chooser) return "Finish the current file selection first.";
    if (static_cast<bool>(processor.backupStatus()["busy"])) return "Finish the current backup/recovery first.";
    if (static_cast<int>(processor.practice.status()["recordMode"]) != 0) return "Finish recording before backup/recovery.";
    chooser = std::make_unique<juce::FileChooser>(restore ? "Restore a Cassian personal backup as copies" : "Save a Cassian personal backup",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(juce::String(selectedTakeIds ? "Cassian selected-takes backup " : includeTakes ? "Cassian backup " : "Cassian tone-library backup ") + juce::Time::getCurrentTime().formatted("%Y-%m-%d") + ".cassian-backup.zip"), "*.zip");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync((restore ? juce::FileBrowserComponent::openMode : juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting) | juce::FileBrowserComponent::canSelectFiles,
        [safe, restore, includeTakes, selectedTakeIds](const juce::FileChooser& dialog) {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (file != juce::File()) { const auto failure = safe->processor.requestBackup(restore, file, includeTakes, selectedTakeIds); if (failure.isNotEmpty()) safe->processor.reportLibraryResult(failure); }
            safe->chooser.reset();
        });
    return {};
}
juce::String AmpSuiteAudioProcessorEditor::chooseRigFile(bool save, bool pack, const juce::String& savedId)
{
    if (chooser) return "Finish the current file selection first.";
    const auto rig = save ? (savedId.isEmpty() ? processor.getRig() : processor.getSavedRig(savedId)) : juce::var();
    if (save && rig.hasProperty("error")) return rig["error"].toString();
    const auto name = savedId.isEmpty() ? juce::String("My rig") : juce::File::createLegalFileName(rig["name"].toString());
    chooser = std::make_unique<juce::FileChooser>(pack ? (save ? "Export portable rig pack" : "Import portable rig pack") : save ? "Export rig references" : "Import Cassian rig",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile((name.isEmpty() ? juce::String("Saved rig") : name) + (pack ? ".cassian.zip" : ".cassian.json")), pack ? "*.zip" : "*.json");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync((save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode)
        | juce::FileBrowserComponent::canSelectFiles, [safe, save, rig, pack](const juce::FileChooser& dialog)
        {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (file != juce::File())
            {
                if (pack) { safe->processor.requestRigPack(save, file, rig); safe->chooser.reset(); return; }
                const auto error = save ? (file.replaceWithText(juce::JSON::toString(rig, false)) ? juce::String() : "Could not write the rig file.")
                    : file.getSize() > 4 * 1024 * 1024 ? juce::String("Rig file is too large.")
                    : safe->processor.importRig(file.getFileNameWithoutExtension().replace(".cassian", ""), juce::JSON::parse(file.loadFileAsString()));
                safe->processor.reportLibraryResult(error.isEmpty() ? (save ? "Rig exported as asset references" : "Rig imported · open Library Presets to load it") : "Load failed: " + error);
            }
            safe->chooser.reset();
        });
    return {};
}
void AmpSuiteAudioProcessorEditor::chooseRelink(const juce::String& id)
{
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>("Locate the original asset", juce::File(), (id.startsWith("cab:") || id.startsWith("ambience:")) ? "*.wav" : "*.nam");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, id](const juce::FileChooser& dialog)
        {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (file.existsAsFile())
            {
                const auto error = safe->processor.relinkAsset(id, file);
                safe->processor.reportLibraryResult(error.isEmpty() ? "Asset relinked" : "Load failed: " + error);
            }
            safe->chooser.reset();
        });
}
void AmpSuiteAudioProcessorEditor::chooseImports(const juce::String& kind)
{
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>("Add files to your " + kind + " library", juce::File(), kind == "pack" ? "*.zip" : (kind == "cab" || kind == "ambience") ? "*.wav" : "*.nam");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
        [safe, kind](const juce::FileChooser& dialog) {
            if (safe == nullptr) return;
            safe->processor.importAssets(dialog.getResults(), kind);
            safe->chooser.reset();
        });
}
