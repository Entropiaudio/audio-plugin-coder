#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * SNIP Bridge Plugin Editor - WebView UI Integration
 *
 * CRITICAL: Member declaration order MUST be:
 * 1. Parameter relays (destroyed last)
 * 2. WebBrowserComponent (destroyed middle)
 * 3. Parameter attachments (destroyed first)
 *
 * This order prevents DAW crashes on plugin unload.
 * See: .claude/troubleshooting/resolutions/webview-member-order-crash.md
 */
class SNIPBridgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        public juce::Timer
{
public:
    SNIPBridgeAudioProcessorEditor (SNIPBridgeAudioProcessor&);
    ~SNIPBridgeAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    //==============================================================================
    void timerCallback() override;

private:
    //==============================================================================
    // CRITICAL: MEMBER DECLARATION ORDER
    // DO NOT REORDER - This prevents DAW crash on unload
    //==============================================================================

    // 1. PARAMETER RELAYS (Destroyed last)
    juce::WebSliderRelay genreRelay { "target_genre" };

    // 2. WEBBROWSERCOMPONENT (Destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS (Destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> genreAttachment;

    //==============================================================================
    // Resource provider for embedded web files
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    // Async HTTP POST to meetsnip.com
    void sendAnalysisToSnip();
    void handleSendResult (bool success, const juce::String& errorMsg);

    // Helper functions
    static const char* getMimeForExtension (const juce::String& extension);
    static juce::String getExtension (juce::String filename);

    // Aspect-ratio constrainer for proportional resizing
    juce::ComponentBoundsConstrainer constrainer;

    // Reference to processor
    SNIPBridgeAudioProcessor& audioProcessor;

    // API sending state
    std::atomic<bool> isSending { false };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SNIPBridgeAudioProcessorEditor)
};
