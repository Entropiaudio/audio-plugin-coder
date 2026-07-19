#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <limits>

//==============================================================================
/**
 * Entropan — band-targeted spectral panner (Entropia Audio).
 *
 * Serial Linkwitz-Riley splitter cascade with allpass compensation, per-band
 * lift split → equal-power pan → ±6 dB gain, six per-band modulator engines
 * (Sine/Tri/S&H/Chaos/Steps/Env) evaluated per sample, PPQ sync + free +
 * MIDI rate modes, global speed, snapshot undo/redo, analyzer + scope
 * telemetry for the WebView UI. Gate: Source/tests/NullTest.cpp.
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

    // UI CHAOS-lock mask (JSON: per-band arrays of frozen param keys). Lives in
    // apvts.state for session persistence; excluded from undo (see stateForUndo).
    juce::String getLocksJson() const;
    void setLocksJson (const juce::String& json);

    // Step-pattern presets (JSON: name → pattern). Persist to a shared file in
    // the user app-data dir so they survive reopen and cross all instances.
    static juce::File stepPresetsFile();
    juce::String getStepPresetsJson() const;
    void setStepPresetsJson (const juce::String& json);

    // Re-roll (CHAOS button): re-deal random modulator states at next tick.
    void requestReroll() { rerollFlag.store (true); }

    // ── Undo / redo (message-thread only) ──
    // Full APVTS-state snapshots (params + step JSON). The editor commits a
    // snapshot after each edit gesture (compare-and-push against lastCommitted).
    void commitUndoIfChanged();
    bool undoState();
    bool redoState();

private:
    // Shared body of undo/redo: pop `from`, push current onto `to`, restore.
    bool restoreSnapshot (std::vector<juce::ValueTree>& from, std::vector<juce::ValueTree>& to);

public:
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

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
        juce::SmoothedValue<float> depth;     // 0..1, per-sample (automation-safe)
        juce::SmoothedValue<float> bias;      // -1..1 static pan offset (resting position)
        float fLoCur = 100.0f, fHiCur = 400.0f;   // block-rate cutoff glide state
        float fLoApplied = -1.0f, fHiApplied = -1.0f;  // last cutoffs pushed into the filters
        bool  cutoffsInit = false;

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            splitLo.prepare (spec);
            splitHi.prepare (spec);
            apLow.prepare (spec);
            // (no setType needed — the two-output processSample always yields
            //  the complementary LP/HP pair regardless of the filter type)
            const double sr = spec.sampleRate;
            lift.reset    (sr, 0.005);
            gainLin.reset (sr, 0.005);
            enable.reset  (sr, 0.030);
            depth.reset   (sr, 0.020);
            bias.reset    (sr, 0.020);
            cutoffsInit = false;
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
        float  panOut  = 0.0f;   // final pan (post bias + depth·amount) for scope/telemetry
        // S&H cell-hash cache — the hashed value is constant for a whole cell,
        // so rehash only when (cell, seed, reroll, mode) changes.
        juce::int64 lastCell = std::numeric_limits<juce::int64>::min();
        int    lastSeed = -1, lastReroll = -1, lastMode = -1;
        float  cellA = 0.0f;
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
        std::atomic<float>* uni;
        std::atomic<float>* bias;
        std::atomic<float>* biasFree;   // override: drop the bias headroom clamp (pan may hit the rail)
        std::atomic<float>* stepSmooth; // Steps mode: 0 = square, 100 = glide between steps
    };
    std::array<BandParams, kNumBands> bandParams {};
    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pSeed   = nullptr;
    std::atomic<float>* pSpeed  = nullptr;
    std::atomic<float>* pRouting = nullptr;   // 0 = Serial, 1 = Parallel
    std::atomic<float>* pWow     = nullptr;
    std::atomic<float>* pFlutter = nullptr;
    std::atomic<float>* pEnvAtk  = nullptr;
    std::atomic<float>* pEnvRel  = nullptr;
    std::atomic<float>* pEnvScf  = nullptr;
    std::atomic<float>* pEnvRms  = nullptr;
    std::atomic<float>* pEnvGain = nullptr;

    // Envelope follower (global detection circuit — feeds every Env-mode band).
    float envScLp = 0.0f;   // sidechain HPF one-pole state
    float envState = 0.0f;  // ballistics state
    float globalEnv = 0.0f; // 0..1, updated per sample from the input copy

    // Global wow & flutter — wet-only modulated stereo delay (RC-20-style),
    // engage-crossfaded so 0 = truly dry (no added latency).
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> wfDelay { 4096 };
    double wowPhase = 0.0, flutPhase = 0.0;
    juce::SmoothedValue<float> wfEngage;
    bool wfWasActive = false;   // block-rate: skip the whole W&F stage while fully disengaged

    // MIDI rate mode: last note frequency (Hz); 0 = no note yet (frozen).
    float midiFreq = 0.0f;

    std::atomic<bool> rerollFlag { false };
    int rerollOffset = 0;                    // hashed into random streams on CHAOS
    double currentSampleRate = 44100.0;

    std::vector<juce::ValueTree> undoStack, redoStack;
    juce::ValueTree lastCommitted;
    static constexpr int kUndoDepth = 128;

public:
    //==============================================================================
    // ── Steps engine (Phase 4.3) ──
    // Slot-stable ratchets parsed from the per-band JSON into RT-safe snapshots
    // (double-buffered, atomic index swap — message thread writes, audio reads).
    struct StepSlot  { int subdiv = 1; float vals[4] { 0, 0, 0, 0 }; bool tie = false; };  // tie = glued to the run on its left
    struct StepsData
    {
        int count = 0;
        StepSlot slots[kMaxSteps];
        // precomputed glue runs (message thread): for each cell, the run's leader
        // index and length. A glued step holds the leader's value across the run.
        int runStart[kMaxSteps] {};
        int runLen[kMaxSteps] {};
    };

    // ── UI telemetry (Phase 4.3) ──
    std::atomic<int>   lastMidiNote { -1 };
    std::array<std::atomic<float>, kNumBands> modOutDepth {};   // post-slew mod × depth
    std::array<std::atomic<float>, kNumBands> modPhase {};       // cycle phase 0..1 (steps highlight)

    // Analyzer tap: mono (L+R)/2 of the processed output, drained by the editor.
    int popAnalyzer (float* dest, int maxNum);

    // Mod scope ring: every band sampled every 64 audio samples (~750 Hz).
    static constexpr int kScopeStride = 64, kScopeRingSize = 2048;
    std::array<std::array<float, kScopeRingSize>, kNumBands> scopeRing {};
    std::array<float, kScopeRingSize> envScopeRing {};   // global envelope-follower output
    std::atomic<juce::uint32> scopeWrite { 0 };

private:
    void parseStepsSnapshot (int bandIndex);

    std::array<std::array<StepsData, 2>, kNumBands> stepsBuf {};
    std::array<std::atomic<int>, kNumBands> stepsActive {};
    int scopePhase = 0;

    juce::AbstractFifo analyzerFifo { 1 << 14 };
    std::vector<float> analyzerStore;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntropanAudioProcessor)
};
