#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "AnalyzerResample.h"

#if ENTROPAN_MOONBASE   // gate default lives in PluginProcessor.h (included above)
// Moonbase Activate screen — forward declaration only; the full module header is
// included in PluginEditor.cpp.
namespace Moonbase { namespace JUCEClient { struct ActivationUI; } }
#endif

//==============================================================================
/**
 * Entropan Plugin Editor — WebView UI (Chaosverb-clone design system).
 *
 * CRITICAL member order (prevents DAW crash on unload):
 *   1. Parameter relays   (destroyed last)
 *   2. WebBrowserComponent (destroyed middle)
 *   3. Parameter attachments (destroyed first)
 *
 * 121 parameters bound via relay/attachment vectors:
 *   - 75 float/int  → WebSliderRelay
 *   - 20 choice     → WebComboBoxRelay
 *   - 26 bool       → WebToggleButtonRelay
 */
class EntropanAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit EntropanAudioProcessorEditor (EntropanAudioProcessor&);
    ~EntropanAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

public:
    // ── ID inventories (built once, drive relay/attachment loops) ──
    // Public so the gate can assert every APVTS parameter appears in exactly
    // one of them: a parameter missing here has no relay, so the UI control
    // binds to nothing and silently does nothing (T41).
    static juce::StringArray sliderParamIds();
    static juce::StringArray comboParamIds();
    static juce::StringArray toggleParamIds();

private:

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

    // ── UI telemetry (60 Hz): spectrum frames + per-band mod values ──
    void timerCallback() override;
    // Multi-resolution analyzer (shared math: AnalyzerResample.h, also compiled
    // by the test gate). 16384-pt for ≥500 Hz (fast, ~341 ms window); 65536-pt
    // for the lows (~1.4 s window, 0.73 Hz bins — low tones render as needles).
    // Both Blackman-Harris. The big FFT runs every 2nd emit (lows move slowly).
    juce::dsp::FFT fftSmall { EntropanAnalyzer::kSmallOrder };
    juce::dsp::FFT fftBig   { EntropanAnalyzer::kBigOrder };
    juce::dsp::WindowingFunction<float> winSmall { EntropanAnalyzer::kSmallSize, juce::dsp::WindowingFunction<float>::blackmanHarris };
    juce::dsp::WindowingFunction<float> winBig   { EntropanAnalyzer::kBigSize,   juce::dsp::WindowingFunction<float>::blackmanHarris };
    std::vector<float> fifoDrain, fftAccum, workSmall, workBig, bigMags;
    int  accumFill = 0;
    int  bigCounter = 0;         // big FFT every 2nd emit
    bool bigValid = false;       // false until 65536 contiguous samples exist
    int analyzerDropsSeen = 0;   // last analyzerDropped value — change ⇒ flush + re-accumulate
    juce::uint32 scopeReadPos = 0;
    std::vector<EntropanAnalyzer::ColGeom> colGeom;
    double colGeomSr = 0.0;

    juce::ComponentBoundsConstrainer constrainer;
    EntropanAudioProcessor& audioProcessor;

#if ENTROPAN_MOONBASE
    // Moonbase Activate screen (native overlay). Created from the processor's
    // licensing client in the ctor (after the WebView, so it sits on top) and
    // sized to the whole editor in resized(). Declared LAST ⇒ destroyed FIRST,
    // before the WebView and its attachments.
    std::unique_ptr<Moonbase::JUCEClient::ActivationUI> activationUI;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntropanAudioProcessorEditor)
};
