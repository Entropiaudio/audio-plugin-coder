#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * Entropan Plugin Editor — WebView UI (Chaosverb-clone design system).
 *
 * CRITICAL member order (prevents DAW crash on unload):
 *   1. Parameter relays   (destroyed last)
 *   2. WebBrowserComponent (destroyed middle)
 *   3. Parameter attachments (destroyed first)
 *
 * 77 parameters bound via relay/attachment vectors:
 *   - 51 float/int  → WebSliderRelay
 *   - 19 choice     → WebComboBoxRelay
 *   -  7 bool       → WebToggleButtonRelay
 */
class EntropanAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit EntropanAudioProcessorEditor (EntropanAudioProcessor&);
    ~EntropanAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // ── ID inventories (built once, drive relay/attachment loops) ──
    static juce::StringArray sliderParamIds();
    static juce::StringArray comboParamIds();
    static juce::StringArray toggleParamIds();

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

    // Embedded-resource provider (BinaryData → WebView)
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static const char* mimeForExtension (const juce::String& ext);

    // ── UI telemetry (30 Hz): spectrum frames + per-band mod values ──
    void timerCallback() override;
    static constexpr int kFftOrder = 11, kFftSize = 1 << kFftOrder, kSpectrumBins = 160;
    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fifoDrain, fftAccum, fftWork;
    int accumFill = 0;
    int scopeReadPos = 0;

    juce::ComponentBoundsConstrainer constrainer;
    EntropanAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntropanAudioProcessorEditor)
};
