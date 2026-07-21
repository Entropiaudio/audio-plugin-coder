#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <limits>

// Moonbase licensing is compiled in unless a tool build opts out with
// -DENTROPAN_MOONBASE=0. The null-test harness compiles these same sources, and
// on an unactivated machine the lapsed-trial gate periodically silences the
// output — which would corrupt every measurement with what look like DSP bugs.
#ifndef ENTROPAN_MOONBASE
 #define ENTROPAN_MOONBASE 1
#endif

#if ENTROPAN_MOONBASE
// Forward declaration only. The full module header (obfuscation + GUI deps) is
// pulled into PluginProcessor.cpp and PluginEditor.cpp, not into every TU that
// includes this header.
namespace Moonbase { namespace JUCEClient { struct API; } }
#endif

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
    static constexpr int kNumWaves = 6;   // Sine, Tri, S&H, Chaos, Steps, Env

    EntropanAudioProcessor();
    // Defined in the .cpp: the Moonbase client is held by unique_ptr to an
    // incomplete type here, so the destructor can't be defaulted inline.
    ~EntropanAudioProcessor() override;

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

#if ENTROPAN_MOONBASE
    // Moonbase licensing client (trial + activation). Constructed in the ctor;
    // public so the editor can build the Activate screen from it. The audio
    // thread only ever calls its RT-safe processBlock() via MOONBASE_PROCESS at
    // the end of our processBlock.
    std::unique_ptr<Moonbase::JUCEClient::API> moonbaseClient;
#endif

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

    // ── Mod matrix: band modulators → any continuous parameter ──
    // Routes live as JSON in apvts.state ("modRoutes") → undoable + persisted.
    // Applied at block rate in the NORMALIZED param domain (skew-aware), as an
    // offset AFTER the atomic read — host automation and the UI stay untouched.
    // dst index = band·10 + slot (freq,width,lift,depth,gain,rate,inertia,
    // phase,bias,stepsmooth) or 60+ (amount,wow,flutter,output).
    static constexpr int kMaxRoutes = 16;
    static constexpr int kDestSlotsPerBand = 10;
    static constexpr int kNumDests = kNumBands * kDestSlotsPerBand + 4;
    // stype: which of the source band's six waveforms drives the route
    // (0..5 explicit; −1 = follow the band's selected MODE — the pre-B68
    // behaviour, kept so old sessions load identically).
    struct ModRoute  { int src = 0; int stype = -1; int dst = -1; float depth = 0.0f; };  // depth −100..+100
    struct RoutesData { int count = 0; ModRoute routes[kMaxRoutes]; };
    juce::String getRoutesJson() const;
    void setRoutesJson (const juce::String& json);

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
    // Selectable-order Linkwitz-Riley splitter: 12 / 24 / 48 dB per octave
    // (LR2 / LR4 / LR8). The LR property LP+HP = allpass holds at every order —
    // that is what keeps the band recombination null-clean. LR2's inherent
    // polarity quirk (complementary sum needs −HP) is folded into the HP output
    // here, so every consumer just sums the two branches like before.
    struct LRSplitter
    {
        static constexpr int kMaxSections = 4;         // LR8 = 4 biquads per branch
        struct BQ { float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
        int    numSections = 2;                        // LR4 default
        bool   invertHp = false;                       // true only for LR2
        double sampleRate = 48000.0;
        float  qs[kMaxSections] {};
        BQ     lpC[kMaxSections], hpC[kMaxSections];
        float  lpS[2][kMaxSections][2] {}, hpS[2][kMaxSections][2] {};   // TDF2 state
        float  lastCutoff = -1.0f;

        void setSlope (int idx)                        // 0 = 12, 1 = 24, 2 = 48 dB/oct
        {
            static constexpr float kQ2[] = { 0.5f };                                    // LR2
            static constexpr float kQ4[] = { 0.70710678f, 0.70710678f };                // LR4 = B2²
            static constexpr float kQ8[] = { 0.54119610f, 1.30656296f,                  // LR8 = B4²
                                             0.54119610f, 1.30656296f };
            const float* q = idx == 0 ? kQ2 : idx == 2 ? kQ8 : kQ4;
            numSections    = idx == 0 ? 1   : idx == 2 ? 4   : 2;
            invertHp       = (idx == 0);
            for (int s = 0; s < numSections; ++s) qs[s] = q[s];
            lastCutoff = -1.0f;                        // force coefficient rebuild
        }

        void prepare (double sr) { sampleRate = sr; reset(); }
        void reset()
        {
            std::memset (lpS, 0, sizeof (lpS));
            std::memset (hpS, 0, sizeof (hpS));
        }

        void setCutoffFrequency (float f)
        {
            if (f == lastCutoff) return;
            lastCutoff = f;
            const double w0 = juce::MathConstants<double>::twoPi
                                * juce::jlimit (10.0, sampleRate * 0.49, (double) f) / sampleRate;
            const double cw = std::cos (w0), sw = std::sin (w0);
            for (int s = 0; s < numSections; ++s)
            {
                const double alpha = sw / (2.0 * qs[s]);
                const double a0 = 1.0 + alpha;
                const float a1 = (float) (-2.0 * cw / a0);
                const float a2 = (float) ((1.0 - alpha) / a0);
                lpC[s] = { (float) ((1.0 - cw) * 0.5 / a0), (float) ((1.0 - cw) / a0),
                           (float) ((1.0 - cw) * 0.5 / a0), a1, a2 };
                hpC[s] = { (float) ((1.0 + cw) * 0.5 / a0), (float) (-(1.0 + cw) / a0),
                           (float) ((1.0 + cw) * 0.5 / a0), a1, a2 };
            }
        }

        // complementary pair: outLo + outHi always reconstructs to an allpass
        void processSample (int ch, float x, float& outLo, float& outHi)
        {
            float lo = x, hi = x;
            for (int s = 0; s < numSections; ++s)
            {
                { auto& c = lpC[s]; auto* st = lpS[ch][s];
                  const float y = c.b0 * lo + st[0];
                  st[0] = c.b1 * lo - c.a1 * y + st[1];
                  st[1] = c.b2 * lo - c.a2 * y;
                  lo = y; }
                { auto& c = hpC[s]; auto* st = hpS[ch][s];
                  const float y = c.b0 * hi + st[0];
                  st[0] = c.b1 * hi - c.a1 * y + st[1];
                  st[1] = c.b2 * hi - c.a2 * y;
                  hi = y; }
            }
            outLo = lo;
            outHi = invertHp ? -hi : hi;
        }
    };

    //==============================================================================
    // Per-band DSP stage: 3-way LR split with allpass-compensated low branch.
    //   input → splitLo → {low, rest};  rest → splitHi → {band, high}
    //   low → apLow (LP+HP sum = allpass at f_hi) → residual = lowAP + high
    //   band → lift split → pan/gain → out = residual + unlifted + panned
    struct BandDSP
    {
        LRSplitter splitLo, splitHi, apLow;
        int slopeApplied = -1;   // last-applied slope index (change ⇒ reconfigure + reset)

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
            const double sr = spec.sampleRate;
            splitLo.prepare (sr);
            splitHi.prepare (sr);
            apLow.prepare (sr);
            slopeApplied = -1;   // reapply slope + cutoffs after rate change
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
        // ALL six waveforms run off this one clock, each with its own slew —
        // the pan uses value[mode], the mod matrix can tap any of them
        // independently (S&H on freq while Sine drives gain, etc.).
        float  value[kNumWaves] {};      // post-slew outputs (-1..1), one per waveform
        float  target[kNumWaves] {};
        float  slewCoeff[kNumWaves] {};  // per-sample one-pole coefficients
        double lx = 0.1, ly = 0.0, lz = 0.0;   // Lorenz state (one stream per band)
        float  panOut  = 0.0f;   // final pan (post bias + depth·amount) for scope/telemetry
        // S&H cell-hash cache — the hashed value is constant for a whole cell,
        // so rehash only when (cell, seed, reroll) changes.
        juce::int64 lastCell = std::numeric_limits<juce::int64>::min();
        int    lastSeed = -1, lastReroll = -1;
        float  cellA = 0.0f;
    };
    std::array<Modulator, kNumBands> mods;

    juce::SmoothedValue<float> outGainSm;    // linear
    juce::SmoothedValue<float> bypassSm;     // 0 = process, 1 = bypassed
    juce::SmoothedValue<float> amountSm;     // 0..1 master depth
    juce::SmoothedValue<float> routingSm;    // 0 = serial, 1 = parallel — crossfaded so the flip never pops
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
        std::atomic<float>* freeze;     // pause the band's modulators (all six hold their value)
        std::atomic<float>* bias;
        std::atomic<float>* biasFree;   // override: drop the bias headroom clamp (pan may hit the rail)
        std::atomic<float>* stepSmooth; // Steps mode: 0 = square, 100 = glide between steps
        std::atomic<float>* slope;      // crossover slope choice: 0=12, 1=24, 2=48 dB/oct
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
    std::array<std::atomic<float>, kNumBands> modOutDepth {};   // final pan (bias + depth·amount) — UI rings/indicator
    std::array<std::atomic<float>, kNumBands> modPhase {};       // cycle phase 0..1 (steps highlight)
    // raw post-slew source values −1..1, one per (band, waveform) — the UI's
    // rings/markers need whichever waveform each route actually taps
    std::array<std::atomic<float>, kNumBands * kNumWaves> modSrcVal {};

    // Analyzer tap: mono (L+R)/2 of the processed output, drained by the editor.
    int popAnalyzer (float* dest, int maxNum);
    // counts analyzer-tap overruns; the editor flushes + re-accumulates when it
    // moves, so a spliced (gap-containing) window is never FFT'd
    std::atomic<int> analyzerDropped { 0 };

    // Mod scope ring: every band sampled every 64 audio samples (~750 Hz).
    static constexpr int kScopeStride = 64, kScopeRingSize = 2048;
    std::array<std::array<float, kScopeRingSize>, kNumBands> scopeRing {};
    std::array<float, kScopeRingSize> envScopeRing {};   // global envelope-follower output
    std::atomic<juce::uint32> scopeWrite { 0 };

private:
    void parseStepsSnapshot (int bandIndex);
    void parseRoutesSnapshot();   // message thread → RT snapshot (like steps)

    std::array<std::array<StepsData, 2>, kNumBands> stepsBuf {};
    std::array<std::atomic<int>, kNumBands> stepsActive {};

    // mod-matrix RT snapshot + destination registry
    std::array<RoutesData, 2> routesBuf {};
    std::atomic<int> routesActive { 0 };
    struct ModDest { std::atomic<float>* raw = nullptr; juce::RangedAudioParameter* param = nullptr; };
    std::array<ModDest, kNumDests> modDests {};
    std::array<float, kNumDests> modVal {};   // per-block post-modulation values (natural units)
    int scopePhase = 0;

    // 4× the editor's 16384-pt FFT window: ~1.4 s of UI-thread stall tolerance
    // before the tap must drop. At exactly 1× (B77 briefly), any stall > 341 ms
    // overflowed, spliced the analysis window, and drew LF garbage.
    juce::AbstractFifo analyzerFifo { 1 << 16 };
    std::vector<float> analyzerStore;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntropanAudioProcessor)
};
