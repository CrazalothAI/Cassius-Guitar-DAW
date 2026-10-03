#include "PluginEditor.h"
#include <BinaryData.h>

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
        .withNativeFunction("loadIR", [this](const auto&, auto complete) { chooseFile(1); complete(true); })
        .withNativeFunction("loadPedal", [this](const auto&, auto complete) { chooseFile(2); complete(true); })
        .withNativeFunction("getLibrary", [this](const auto&, auto complete) { complete(processor.getLibrary()); })
        .withNativeFunction("importAssets", [this](const auto& args, auto complete) {
            const auto kind = args.size() == 1 ? args[0].toString() : juce::String();
            const bool valid = kind == "amp" || kind == "pedal" || kind == "cab";
            if (valid) chooseImports(kind); complete(valid);
        })
        .withNativeFunction("getRig", [this](const auto&, auto complete) { complete(processor.getRig()); })
        .withNativeFunction("applyRig", [this](const auto& args, auto complete) { complete(args.size() == 1 ? processor.applyRig(args[0]) : "Invalid rig request."); })
        .withNativeFunction("saveRig", [this](const auto& args, auto complete) { complete(args.size() == 1 ? processor.saveRig(args[0].toString()) : "Give the rig a name."); })
        .withNativeFunction("loadRig", [this](const auto& args, auto complete) { complete(args.size() == 1 ? processor.loadRig(args[0].toString()) : "Rig not found."); })
        .withNativeFunction("removeRig", [this](const auto& args, auto complete) { complete(args.size() == 1 && processor.removeRig(args[0].toString())); })
        .withNativeFunction("selectAsset", [this](const auto& args, auto complete) { complete(args.size() == 1 && processor.selectAsset(args[0].toString())); })
        .withNativeFunction("editAsset", [this](const auto& args, auto complete) { complete(args.size() == 2 && processor.editAsset(args[0].toString(), args[1])); })
        .withNativeFunction("exportRig", [this](const auto&, auto complete) { chooseRigFile(true); complete(true); })
        .withNativeFunction("importRig", [this](const auto&, auto complete) { chooseRigFile(false); complete(true); })
        .withNativeFunction("relinkAsset", [this](const auto& args, auto complete) { if (args.size() == 1) chooseRelink(args[0].toString()); complete(args.size() == 1); })
        .withNativeFunction("selectAmpVoice", [this](const auto& args, auto complete) { complete(args.size() == 1 && processor.selectAmpVoice(args[0].toString())); })
        .withNativeFunction("clearStage", [this](const auto& args, auto complete)
        {
            // An empty path unloads the stage on the loader thread.
            const auto stage = args.size() == 1 ? args[0].toString() : juce::String();
            if (stage == "pedal") processor.requestPedal(juce::File());
            else if (stage == "amp" || stage == "cab") processor.requestFile(stage == "amp", juce::File());
            complete(stage == "amp" || stage == "pedal" || stage == "cab");
        })
        .withNativeFunction("setBufferSize", [this](const auto& args, auto complete)
        {
            // Standalone only: a DAW owns its own buffer size.
            if (args.size() != 1 || !processor.setDeviceBufferSize) { complete(juce::String("Set the buffer size in your DAW's audio settings.")); return; }
            complete(processor.setDeviceBufferSize(static_cast<int>(args[0])));
        })
        .withNativeFunction("setTuner", [this](const auto& args, auto complete) { processor.setTunerActive(args.size() == 1 && static_cast<bool>(args[0])); complete(true); })
        .withNativeFunction("getStatus", [this](const auto&, auto complete) { complete(processor.status()); });
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
AmpSuiteAudioProcessorEditor::~AmpSuiteAudioProcessorEditor() { processor.setTunerActive(false); }
void AmpSuiteAudioProcessorEditor::resized() { webView->setBounds(getLocalBounds()); }
void AmpSuiteAudioProcessorEditor::chooseFile(int stage)
{
    if (chooser) return;
    chooser = std::make_unique<juce::FileChooser>(stage == 2 ? "Load pedal capture" : stage == 0 ? "Load neural amp capture" : "Load cabinet impulse response",
        juce::File(), stage == 1 ? "*.wav" : "*.nam");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, stage](const juce::FileChooser& dialog)
        {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (file.existsAsFile())
            {
                if (stage == 2) safe->processor.requestPedal(file);
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
void AmpSuiteAudioProcessorEditor::chooseRigFile(bool save)
{
    if (chooser) return;
    const auto rig = save ? processor.getRig() : juce::var();
    if (save && rig.hasProperty("error")) { processor.reportLibraryResult("Load failed: " + rig["error"].toString()); return; }
    chooser = std::make_unique<juce::FileChooser>(save ? "Export rig references" : "Import Cassian rig",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("My rig.cassian.json"), "*.json");
    const juce::Component::SafePointer<AmpSuiteAudioProcessorEditor> safe(this);
    chooser->launchAsync((save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode)
        | juce::FileBrowserComponent::canSelectFiles, [safe, save, rig](const juce::FileChooser& dialog)
        {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (file != juce::File())
            {
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
