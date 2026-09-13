#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * __PLUGIN___AudioProcessorEditor — WebView editor scaffold.
 *
 * CRITICAL member order (prevents DAW crash on unload):
 *   1. Parameter relays   (destroyed last)
 *   2. WebBrowserComponent (destroyed middle)
 *   3. Parameter attachments (destroyed first)
 *
 * Relays/attachments are held in vectors and built from ID lists, so adding a
 * parameter is one line in the matching sliderParamIds()/comboParamIds()/
 * toggleParamIds() list — no per-param member boilerplate.
 */
class __PLUGIN___AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit __PLUGIN___AudioProcessorEditor (__PLUGIN___AudioProcessor&);
    ~__PLUGIN___AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static juce::StringArray sliderParamIds();   // float / int → WebSliderRelay
    static juce::StringArray comboParamIds();     // choice     → WebComboBoxRelay
    static juce::StringArray toggleParamIds();    // bool       → WebToggleButtonRelay

    // 1. RELAYS (destroyed last)
    std::vector<std::unique_ptr<juce::WebSliderRelay>>       sliderRelays;
    std::vector<std::unique_ptr<juce::WebComboBoxRelay>>     comboRelays;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>> toggleRelays;

    // 2. WEBVIEW (destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS (destroyed first)
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>>       sliderAttachments;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>>     comboAttachments;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleAttachments;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static const char* mimeForExtension (const juce::String& ext);

    juce::ComponentBoundsConstrainer constrainer;
    __PLUGIN___AudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (__PLUGIN___AudioProcessorEditor)
};
