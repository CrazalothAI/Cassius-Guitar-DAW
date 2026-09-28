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
        .withNativeFunction("selectAmpVoice", [this](const auto& args, auto complete) { complete(args.size() == 1 && processor.selectAmpVoice(args[0].toString())); })
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
                else safe->processor.requestFile(stage == 0, file);
            }
            safe->chooser.reset();
        });
}
