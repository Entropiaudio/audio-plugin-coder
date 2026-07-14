#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/**
 * Entropan — band-targeted spectral panner (Entropia Audio).
 *
 * Phase 4.1: core DSP — serial Linkwitz-Riley splitter cascade with allpass
 * compensation, per-band lift split, equal-power pan (static target for now),
 * per-band gain, output/bypass stages. Null-tested (see Source/tests/).
 * Modulator engines land in Phase 4.2.
 */
class EntropanAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kNumBands = 6;
    static constexpr int kMaxSteps = 16;

    EntropanAudioProcessor();
    ~EntropanAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "Entropan"; }
    bool acceptsMidi() const override  { return true;  }   // MIDI rate mode
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // Step-sequencer data (non-APVTS state): one JSON string per band inside
    // apvts.state, so it rides along with session/preset save-load.
    juce::String getStepsJson (int bandIndex) const;
    void setStepsJson (int bandIndex, const juce::String& json);

    // Re-roll (CHAOS button): re-deal random modulator states at next tick.
    void requestReroll() { rerollFlag.store (true); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // Per-band DSP stage: 3-way LR4 split with allpass-compensated low branch.
    //   input → splitLo → {low, rest};  rest → splitHi → {band, high}
    //   low → apLow (LP+HP sum = allpass at f_hi) → residual = lowAP + high
    //   band → lift split → pan/gain → out = residual + unlifted + panned
    struct BandDSP
    {
        juce::dsp::LinkwitzRileyFilter<float> splitLo, splitHi, apLow;

        juce::SmoothedValue<float> lift;      // 0..1
        juce::SmoothedValue<float> gainLin;   // linear, from ±6 dB
        juce::SmoothedValue<float> enable;    // 0..1 engage crossfade (~30 ms)

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            splitLo.prepare (spec);
            splitHi.prepare (spec);
            apLow.prepare (spec);
            splitLo.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);  // processSample gives both outs
            splitHi.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            apLow.setType   (juce::dsp::LinkwitzRileyFilterType::allpass);
            const double sr = spec.sampleRate;
            lift.reset    (sr, 0.005);
            gainLin.reset (sr, 0.005);
            enable.reset  (sr, 0.030);
        }

        void resetState()
        {
            splitLo.reset(); splitHi.reset(); apLow.reset();
        }
    };

    std::array<BandDSP, kNumBands> bands;

    //==============================================================================
    // Per-band modulation engine (Phase 4.2). Evaluated PER SAMPLE — free mode
    // reaches audio rate with no control-grid artifacts. Random modes use
    // stateless cell hashes → loop-safe, reproducible per seed, re-rollable.
    struct Modulator
    {
        double phase   = 0.0;    // cycle position 0..1 (free/MIDI accumulate; sync derives from PPQ)
        float  value   = 0.0f;   // post-slew output (-1..1)
        double lx = 0.1, ly = 0.0, lz = 0.0;   // Lorenz state
        float  slewCoeff = 0.0f; // per-sample one-pole coefficient (from inertia)
        float  target  = 0.0f;
    };
    std::array<Modulator, kNumBands> mods;

    juce::SmoothedValue<float> outGainSm;    // linear
    juce::SmoothedValue<float> bypassSm;     // 0 = process, 1 = bypassed
    juce::SmoothedValue<float> amountSm;     // 0..1 master depth
    juce::AudioBuffer<float> dryBuffer;

    // Cached raw parameter pointers (hot path — no string lookups per block)
    struct BandParams
    {
        std::atomic<float>* on;
        std::atomic<float>* freq;
        std::atomic<float>* width;
        std::atomic<float>* lift;
        std::atomic<float>* depth;
        std::atomic<float>* gain;
        std::atomic<float>* mode;
        std::atomic<float>* rate;
        std::atomic<float>* ratemode;
        std::atomic<float>* div;
        std::atomic<float>* inertia;
        std::atomic<float>* phase;
    };
    std::array<BandParams, kNumBands> bandParams {};
    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pSeed   = nullptr;
    std::atomic<float>* pSpeed  = nullptr;

    // MIDI rate mode: last note frequency (Hz); 0 = no note yet (frozen).
    float midiFreq = 0.0f;

    std::atomic<bool> rerollFlag { false };
    int rerollOffset = 0;                    // hashed into random streams on CHAOS
    double currentSampleRate = 44100.0;

public:
    //==============================================================================
    // ── Steps engine (Phase 4.3) ──
    // Slot-stable ratchets parsed from the per-band JSON into RT-safe snapshots
    // (double-buffered, atomic index swap — message thread writes, audio reads).
    struct StepSlot  { int subdiv = 1; float vals[4] { 0, 0, 0, 0 }; };
    struct StepsData { int count = 0; StepSlot slots[kMaxSteps]; };

    // ── UI telemetry (Phase 4.3) ──
    std::atomic<int>   lastMidiNote { -1 };
    std::array<std::atomic<float>, kNumBands> modOutDepth {};   // post-slew mod × depth

    // Analyzer tap: mono (L+R)/2 of the processed output, drained by the editor.
    int popAnalyzer (float* dest, int maxNum);

private:
    void parseStepsSnapshot (int bandIndex);

    std::array<std::array<StepsData, 2>, kNumBands> stepsBuf {};
    std::array<std::atomic<int>, kNumBands> stepsActive {};

    juce::AbstractFifo analyzerFifo { 1 << 14 };
    std::vector<float> analyzerStore;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntropanAudioProcessor)
};
