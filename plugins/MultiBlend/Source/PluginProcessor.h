#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <set>
#include "PluginHostManager.h"
#include "PluginSlot.h"
#include "CrossoverProcessor.h"
#include "BandProcessor.h"

//==============================================================================
// Shared parameter ID definitions used by both Processor and Editor
//==============================================================================
namespace ParamIDs
{
    static const juce::StringArray bandNames { "low", "mid", "high" };
    static const int numBands = 3;
    static const int slotsPerBand = 5;

    struct ParamInfo
    {
        juce::String id;
        bool isToggle;
    };

    inline std::vector<ParamInfo> getAllParams()
    {
        std::vector<ParamInfo> params;

        // Global
        params.push_back ({ "global_dry_wet", false });
        params.push_back ({ "global_input_gain", false });
        params.push_back ({ "global_output_gain", false });
        params.push_back ({ "linear_phase", true });

        // Crossover (freq + slope per crossover line)
        params.push_back ({ "crossover_low_mid", false });
        params.push_back ({ "crossover_low_mid_slope", false });
        params.push_back ({ "crossover_mid_high", false });
        params.push_back ({ "crossover_mid_high_slope", false });

        // Per-band
        for (int b = 0; b < numBands; ++b)
        {
            auto prefix = "band_" + bandNames[b] + "_";
            params.push_back ({ prefix + "dry_wet", false });
            params.push_back ({ prefix + "input_gain", false });
            params.push_back ({ prefix + "output_gain", false });
            params.push_back ({ prefix + "solo", true });
            params.push_back ({ prefix + "mute", true });
            params.push_back ({ prefix + "bypass", true });
            params.push_back ({ prefix + "ms_morph", false });

            // Per-slot
            for (int s = 1; s <= slotsPerBand; ++s)
            {
                auto slotPrefix = prefix + "slot_" + juce::String (s) + "_";
                params.push_back ({ slotPrefix + "dry_wet", false });
                params.push_back ({ slotPrefix + "bypass", true });
            }
        }

        return params;
    }
}

//==============================================================================
class MultiBlendAudioProcessor : public juce::AudioProcessor
{
public:
    MultiBlendAudioProcessor();
    ~MultiBlendAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Recompute per-band latency, set alignment delays, report total to host.
    // CALL FROM MESSAGE THREAD ONLY (queries plugin instances + setLatencySamples).
    void updateLatency();

    // Chaos shuffle — randomly permute loaded plugins across UNLOCKED slot coords.
    // lockedCoords: set of linear keys (band * 5 + slot) to exclude.
    // CALL FROM MESSAGE THREAD ONLY.
    void shufflePlugins (const std::set<int>& lockedCoords);

    // Atomic CHAOS action: randomize unlocked knob params + shuffle plugins,
    // all wrapped in a single undo transaction so one Ctrl+Z reverts everything.
    void triggerChaos (const std::set<int>& lockedSlotCoords,
                       const std::set<juce::String>& lockedParamIds);

    // ── Undoable slot load/unload ──
    struct SlotPluginState
    {
        bool loaded = false;
        juce::PluginDescription desc;
        juce::MemoryBlock state;
    };
    SlotPluginState captureSlot (int band, int slot);
    void restoreSlot (int band, int slot, const SlotPluginState& s);   // async load / unload
    void pushSlotPluginAction (int band, int slot,
                               SlotPluginState before, SlotPluginState after,
                               const juce::String& name);

    // Close current "Chaos" transaction so next action starts fresh.
    // Call before any non-chaos message-thread action that pushes undoable state.
    void closeChaosIfOpen()
    {
        if (undoManager.getCurrentTransactionName() == "Chaos")
            undoManager.beginNewTransaction();
    }

    std::atomic<bool> insideChaos { false };

    // Listener that closes "Chaos" transaction when any non-chaos gesture starts
    struct GestureWatcher : juce::AudioProcessorParameter::Listener
    {
        explicit GestureWatcher (MultiBlendAudioProcessor& p);
        void parameterValueChanged   (int parameterIndex, float newValue) override;
        void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;
        MultiBlendAudioProcessor& owner;
    };
    GestureWatcher gestureWatcher { *this };

    //==============================================================================
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    PluginHostManager hostManager;
    std::array<BandProcessor, 3> bands;              // [low, mid, high]
    CrossoverProcessor crossover;

    // Convenience accessor for Editor's native functions
    PluginSlot& getSlot (int band, int slot) { return bands[(size_t) band].slots[(size_t) slot]; }

    // ---- Spectrum FFT ----
    // Audio thread fills fifo; UI thread (timer in editor) reads + runs FFT.
    static constexpr int fftOrder = 13;            // 8192 samples — finer low-freq resolution
    static constexpr int fftSize  = 1 << fftOrder; // 4096
    enum class FFTChannel { Input, Output };
    void pushSamplesToFFT (const juce::AudioBuffer<float>& buffer, FFTChannel which);
    bool getNextFFTBlock (float* outMagnitudes, int numBins, FFTChannel which);

private:
    juce::dsp::FFT spectrumFFT { fftOrder };
    juce::dsp::WindowingFunction<float> spectrumWindow { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftBuffer {};      // r/i interleaved for FFT

    struct FFTFifo {
        std::array<float, fftSize> samples {};
        std::atomic<int> idx { 0 };
        std::atomic<bool> ready { false };
    };
    FFTFifo fifoInput;
    FFTFifo fifoOutput;

public:

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Global gain staging
    juce::SmoothedValue<float> globalInputGain { 1.0f };
    juce::SmoothedValue<float> globalOutputGain { 1.0f };
    juce::SmoothedValue<float> globalDryWet { 1.0f };

    // Dry buffer for global dry/wet
    juce::AudioBuffer<float> dryBuffer;

    // Dynamic-Phase mode: snapshot of each band's RAW (pre-plugin) crossover region.
    // output = input − Σ(raw touched) + Σ(processed active) → unused spectrum stays
    // bit-transparent; only active bands carry crossover phase.
    std::array<juce::AudioBuffer<float>, 3> rawRegion;

    // Processed bands emerge at the reported latency (plugin lag + alignment delay).
    // The clean input + raw snapshots are at zero latency → delay them to match so
    // the difference (input − raw + processed) recombines phase-coherently.
    static constexpr int maxDynDelay = 8192;
    using DynDelay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>;
    DynDelay dynInputDelay { maxDynDelay };
    std::array<DynDelay, 3> dynRawDelay { DynDelay { maxDynDelay }, DynDelay { maxDynDelay }, DynDelay { maxDynDelay } };
    int lastDynDelay = -1;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiBlendAudioProcessor)
};
