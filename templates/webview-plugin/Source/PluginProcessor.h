#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/**
 * __PLUGIN___AudioProcessor — WebView plugin template (Entropia Audio scaffold).
 *
 * Distilled from Entropan: APVTS parameter layout with a log-skew helper that
 * stays byte-compatible with the WebView JS param layer, snapshot undo/redo
 * (params only — window size is excluded), and a clean processBlock stub.
 * Replace the demo params + DSP with your own; the editor/UI wiring scales to
 * any parameter count automatically.
 */
class __PLUGIN___AudioProcessor : public juce::AudioProcessor
{
public:
    __PLUGIN___AudioProcessor();
    ~__PLUGIN___AudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "__PLUGIN__"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // ── Snapshot undo / redo (message-thread only) ──
    // The editor commits after each gesture; a host-side compare-and-push
    // filters no-op gestures, so a plain 'mouseup' is a safe commit trigger.
    void commitUndoIfChanged();
    bool undoState();
    bool redoState();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Demo DSP — swap for your own.
    juce::dsp::Gain<float> gain;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> bypassSm;
    juce::AudioBuffer<float> dryBuffer;

    std::atomic<float>* pGain   = nullptr;
    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pMode   = nullptr;
    std::atomic<float>* pBypass = nullptr;

    std::vector<juce::ValueTree> undoStack, redoStack;
    juce::ValueTree lastCommitted;
    static constexpr int kUndoDepth = 128;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (__PLUGIN___AudioProcessor)
};
