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
        .withNativeFunction("importAssets", [this](const auto& args, auto complete) {
            const auto kind = args.size() == 1 ? args[0].toString() : juce::String();
            const bool valid = kind == "amp" || kind == "pedal" || kind == "cab";
            if (valid) chooseImports(kind); complete(valid);
        })
        .withNativeFunction("getRig", [this](const auto&, auto complete) { complete(processor.getRig()); })
        .withNativeFunction("storeScene", [this](const auto& args, auto complete) { complete(args.size() == 2 && args[0].isInt() && args[1].isString() ? processor.storeScene(static_cast<int>(args[0]), args[1].toString()) : juce::String("Invalid scene request.")); })
        .withNativeFunction("recallScene", [this](const auto& args, auto complete) { complete(args.size() == 1 && args[0].isInt() ? processor.recallScene(static_cast<int>(args[0])) : juce::String("Invalid scene request.")); })
        .withNativeFunction("clearScene", [this](const auto& args, auto complete) { complete(args.size() == 1 && args[0].isInt() ? processor.clearScene(static_cast<int>(args[0])) : juce::String("Invalid scene request.")); })
        .withNativeFunction("applyRig", [this](const auto& args, auto complete) { complete(args.size() == 1 || args.size() == 2 ? processor.applyRig(args[0], true, args.size() == 2 && static_cast<bool>(args[1])) : "Invalid rig request."); })
        .withNativeFunction("saveRig", [this](const auto& args, auto complete) { complete(args.size() == 1 ? processor.saveRig(args[0].toString()) : "Give the rig a name."); })
        .withNativeFunction("loadRig", [this](const auto& args, auto complete) { complete(args.size() == 1 ? processor.loadRig(args[0].toString()) : "Rig not found."); })
        .withNativeFunction("removeRig", [this](const auto& args, auto complete) { complete(args.size() == 1 && processor.removeRig(args[0].toString())); })
        .withNativeFunction("selectAsset", [this](const auto& args, auto complete) { complete((args.size() == 1 || (args.size() == 2 && args[1].toString() == "cabB")) && processor.selectAsset(args[0].toString(), args.size() == 2)); })
        .withNativeFunction("editAsset", [this](const auto& args, auto complete) { complete(args.size() == 2 && processor.editAsset(args[0].toString(), args[1])); })
        .withNativeFunction("exportRig", [this](const auto&, auto complete) { chooseRigFile(true); complete(true); })
        .withNativeFunction("importRig", [this](const auto&, auto complete) { chooseRigFile(false); complete(true); })
        .withNativeFunction("exportRigPack", [this](const auto&, auto complete) { chooseRigFile(true, true); complete(true); })
        .withNativeFunction("importRigPack", [this](const auto&, auto complete) { chooseRigFile(false, true); complete(true); })
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
        .withNativeFunction("importTake", [this](const auto&, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Take review is available in the standalone app.")); return; }
            chooseTakeFolder(); complete(juce::String());
        })
        .withNativeFunction("editTake", [this](const auto& args, auto complete) {
            complete(args.size() == 3 && args[0].isString() && args[1].isString() && args[2].isBool() ? processor.takes.edit(args[0].toString(), args[1].toString(), static_cast<bool>(args[2])) : juce::String("Invalid take edit."));
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
        .withNativeFunction("reampTake", [this](const auto& args, auto complete) {
            if (!processor.showDeviceSettings) { complete(juce::String("Reamping is available in the standalone app.")); return; }
            if (static_cast<int>(processor.practice.status()["recordMode"]) != 0) { complete(juce::String("Finish the recording before starting an export.")); return; }
            complete(args.size() == 1 && args[0].isString() ? processor.takes.reamp(args[0].toString(), processor.getRig()) : juce::String("Choose a take to reamp."));
        })
        .withNativeFunction("cancelReamp", [this](const auto&, auto complete) { processor.takes.cancelExport(); complete(juce::String()); })
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
            auto status = processor.status(); if (processor.midiInputs) status.getDynamicObject()->setProperty("midiInputs", processor.midiInputs()); complete(status);
        });
    for (const auto& parameter : Params::definitions)
    {
        relays.push_back(std::make_unique<juce::WebSliderRelay>(parameter.id));
        options = options.withOptionsFrom(*relays.back());
    }
    webView = std::make_unique<juce::WebBrowserComponent>(options);
    for (size_t i = 0; i < relays.size(); ++i)
        attachments.push_back(std::make_unique<juce::WebSliderParameterAttachment>(
            *processor.apvts.getParameter(Params::definitions[i].id), *relays[i], nullptr));
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
                const auto rig = safe->processor.getRig();
                const auto failure = rig.hasProperty("error") ? rig["error"].toString() : safe->processor.practice.record(file, rig);
                if (failure.isNotEmpty()) safe->processor.reportLibraryResult("Load failed: " + failure);
            } else if (!recording && file.existsAsFile()) safe->processor.practice.load(file);
            safe->chooser.reset();
        });
}
void AmpSuiteAudioProcessorEditor::chooseTakeFolder()
{
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>("Import a Cassian take folder", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe](const juce::FileChooser& dialog) {
            if (safe == nullptr) return;
            const auto folder = dialog.getResult(); if (folder.isDirectory()) safe->processor.takes.importFolder(folder);
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
void AmpSuiteAudioProcessorEditor::chooseRigFile(bool save, bool pack)
{
    if (chooser) return;
    const auto rig = save ? processor.getRig() : juce::var();
    if (save && rig.hasProperty("error")) { processor.reportLibraryResult("Load failed: " + rig["error"].toString()); return; }
    chooser = std::make_unique<juce::FileChooser>(pack ? (save ? "Export portable rig pack" : "Import portable rig pack") : save ? "Export rig references" : "Import Cassian rig",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(pack ? "My rig.cassian.zip" : "My rig.cassian.json"), pack ? "*.zip" : "*.json");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync((save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode)
        | juce::FileBrowserComponent::canSelectFiles, [safe, save, rig, pack](const juce::FileChooser& dialog)
        {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (file != juce::File())
            {
                if (pack) { safe->processor.requestRigPack(save, file); safe->chooser.reset(); return; }
                const auto error = save ? (file.replaceWithText(juce::JSON::toString(rig, false)) ? juce::String() : "Could not write the rig file.")
                    : file.getSize() > 4 * 1024 * 1024 ? juce::String("Rig file is too large.")
                    : safe->processor.importRig(file.getFileNameWithoutExtension().replace(".cassian", ""), juce::JSON::parse(file.loadFileAsString()));
                safe->processor.reportLibraryResult(error.isEmpty() ? (save ? "Rig exported as asset references" : "Rig imported · open Library Presets to load it") : "Load failed: " + error);
            }
            safe->chooser.reset();
        });
}
void AmpSuiteAudioProcessorEditor::chooseRelink(const juce::String& id)
{
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>("Locate the original asset", juce::File(), id.startsWith("cab:") ? "*.wav" : "*.nam");
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
    chooser = std::make_unique<juce::FileChooser>("Add files to your " + kind + " library", juce::File(), kind == "cab" ? "*.wav" : "*.nam");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
        [safe, kind](const juce::FileChooser& dialog) {
            if (safe == nullptr) return;
            safe->processor.importAssets(dialog.getResults(), kind);
            safe->chooser.reset();
        });
}
