#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "PluginEditorWindow.h"

class MultiBlendAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::Timer
{
public:
    MultiBlendAudioProcessorEditor (MultiBlendAudioProcessor&);
    ~MultiBlendAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    //==============================================================================
    // CRITICAL: MEMBER DECLARATION ORDER
    // Relays → WebView → Attachments (destruction safety)
    //==============================================================================

    // 1. PARAMETER RELAYS (destroyed last)
    std::vector<std::unique_ptr<juce::WebSliderRelay>> sliderRelays;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>> toggleRelays;

    // 2. WEBBROWSERCOMPONENT (destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS (destroyed first)
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleAttachments;

    //==============================================================================
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static const char* getMimeForExtension (const juce::String& ext);

    // Hosted plugin editor windows
    std::map<std::pair<int, int>, std::unique_ptr<PluginEditorWindow>> editorWindows;

    // Destroy every open hosted-plugin editor window NOW (message thread).
    // MUST run before any plugin instance is detached/replaced/freed: the window
    // owns that plugin's AudioProcessorEditor, and some plugins (e.g. UAD) keep a
    // main-thread CFRunLoop timer that fires into the editor — if the plugin is
    // freed first, that timer hits freed memory → crash.
    void closeAllEditorWindows() { editorWindows.clear(); }

    juce::ComponentBoundsConstrainer constrainer;
    int latencyTickCounter = 0;
    int undoIdleTicks = 0;
    int lastUndoActionCount = 0;
    MultiBlendAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiBlendAudioProcessorEditor)
};
