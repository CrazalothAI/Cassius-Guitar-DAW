#pragma once
#include "PluginProcessor.h"
class AmpSuiteAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AmpSuiteAudioProcessorEditor(AmpSuiteAudioProcessor&);
    ~AmpSuiteAudioProcessorEditor() override;
    void resized() override;
private:
    void chooseFile(int);
    void chooseRigFile(bool save);
    void chooseRelink(const juce::String& id);
    void chooseImports(const juce::String& kind);
    AmpSuiteAudioProcessor& processor;
    // Destruction order: chooser, attachments, browser, relays.
    std::vector<std::unique_ptr<juce::WebSliderRelay>> relays;
    std::unique_ptr<juce::WebBrowserComponent> webView;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> attachments;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpSuiteAudioProcessorEditor)
};
