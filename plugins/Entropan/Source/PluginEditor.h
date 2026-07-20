#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

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
 * 108 parameters bound via relay/attachment vectors:
 *   - 69 float/int  → WebSliderRelay
 *   - 19 choice     → WebComboBoxRelay
 *   - 20 bool       → WebToggleButtonRelay
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

    // ── UI telemetry (60 Hz): spectrum frames + per-band mod values ──
    void timerCallback() override;
    static constexpr int kFftOrder = 12, kFftSize = 1 << kFftOrder, kSpectrumBins = 256;  // 4096-pt → finer low-freq bins
    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fifoDrain, fftAccum, fftWork;
    int accumFill = 0;
    juce::uint32 scopeReadPos = 0;
    // Log-bin resample geometry — constant per sample rate, rebuilt lazily
    // (was ~770 pow/sqrt calls per 60 Hz frame).
    struct BinGeom { int b0, i0, i1; float fr; };
    std::vector<BinGeom> binGeom;
    double binGeomSr = 0.0;

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
