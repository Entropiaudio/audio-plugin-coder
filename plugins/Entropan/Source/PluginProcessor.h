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
 * Subtractive-displacement band engine (unity-peak resonant bell per band,
 * three warm cascades crossfaded for a continuous 12→48 dB/oct slope morph),
 * lift split → equal-power pan → ±6 dB gain, six per-band modulator engines
 * (Sine/Tri/S&H/Chaos/Steps/Env) evaluated per sample, PPQ sync + free +
 * MIDI rate modes, global speed, snapshot undo/redo, analyzer + scope
 * telemetry for the WebView UI. Gate: Source/tests/NullTest.cpp.
 */
class EntropanAudioProcessor : public juce::AudioProcessor,
                               private juce::AsyncUpdater
{
public:
    static constexpr int kNumBands = 6;
    static constexpr int kMaxSteps = 16;

    // ── Wow & flutter spec ────────────────────────────────────────────────
    // Depth is stated as PEAK PITCH DEVIATION, not as a delay time, because
    // that is what the ear judges and what tape decks are specced by. A sine
    // modulating a delay swings pitch by 2*pi*rate*peakDelay, so the peak delay
    // each one needs is kWowPitch / (2*pi*kWowRate) — which means the rates can
    // be retuned without the depth character drifting with them.
    //   0.80% ~ 13.8 cents, 0.60% ~ 10.4 cents at 100%: roughly twice a badly
    //   worn cassette (0.15-0.35%), so 100% is openly an effect while ordinary
    //   tape still lands around a third of the way up the knob.
    static constexpr double kWowRate    = 0.5;      // Hz — capstan drift
    static constexpr double kFlutRate   = 6.3;      // Hz — pinch-roller flutter
    static constexpr double kWowPitch   = 0.0080;   // peak dp/p at 100%
    static constexpr double kFlutPitch  = 0.0060;
    static constexpr double kWowPeakS   = kWowPitch  / (2.0 * juce::MathConstants<double>::pi * kWowRate);
    static constexpr double kFlutPeakS  = kFlutPitch / (2.0 * juce::MathConstants<double>::pi * kFlutRate);
    // The read tap sits kBaseDelayS behind and swings +/-(wow+flutter); the base
    // only has to keep it positive, and every extra millisecond is latency the
    // host has to compensate. 1.5x the worst-case swing still leaves ~40 samples
    // of margin under the tap, twenty times what Lagrange3rd needs.
    static constexpr double kBaseDelayS = 1.5 * (kWowPeakS + kFlutPeakS);
    // Engage GLIDE, not crossfade — see the tap ramp in processBlock. The dip
    // this costs is kBaseDelayS / kEngageS in pitch, so 4.0 ms over 300 ms is
    // ~1.3%: the same order as the wow itself, which is why it reads as the
    // machine spinning up rather than as an edit.
    static constexpr double kEngageS    = 0.30;
    // FLUX — a real transport is not two sine waves. The capstan speeds and
    // slows, flutter drifts with tape tension, and no two revolutions match.
    // FLUX morphs each modulator from a pure periodic sine toward a random
    // walk and jitters its rate, re-dealt once per revolution.
    //
    // It is a CROSSFADE toward the walk, never an addition, and the walk is
    // bounded to the same +/-1 the sine occupies — so the worst-case delay
    // excursion, and therefore the reported latency, does not grow with FLUX.
    // The extra intensity comes from the rate jitter instead: pitch swing is
    // 2*pi*rate*peak, so a faster revolution swings harder for free.
    static constexpr double kFluxJitter = 0.6;   // rate wander, +/-60% at 100%

    // Samples the wet path adds when engaged; 0 when both knobs are down.
    int wfLatencySamples() const noexcept;
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

    // UI preferences (theme, tooltips, brightness) — a GLOBAL user choice, so
    // it lives in the same shared app-data dir as the step presets rather than
    // the WebView's localStorage (that sits in a unique per-instance temp
    // folder and is lost on reopen).
    static juce::File uiSettingsFile();
    juce::String getUiSettingsJson() const;
    void setUiSettingsJson (const juce::String& json);

    // ── Mod matrix: band modulators → any continuous parameter ──
    // Routes live as JSON in apvts.state ("modRoutes") → undoable + persisted.
    // Applied at block rate in the NORMALIZED param domain (skew-aware), as an
    // offset AFTER the atomic read — host automation and the UI stay untouched.
    // dst index = band·10 + slot (freq,width,lift,depth,gain,rate,inertia,
    // phase,bias,stepsmooth) or 60+ (amount,wow,flutter,output).
    static constexpr int kMaxRoutes = 16;
    static constexpr int kDestSlotsPerBand = 10;
    // +5: amount, wow, flutter, output, flux. Flux is APPENDED so every saved
    // route keeps its dst index.
    static constexpr int kNumDests = kNumBands * kDestSlotsPerBand + 5;
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
    // SUBTRACTIVE DISPLACEMENT topology (the dynamic-EQ construction).
    // The band is a UNITY-PEAK resonant bell B(x); the "residual" is literally
    // input − band, so:
    //     out = in + B(in)·lift·(g − 1)
    // Idle (g = 1) is a BIT-EXACT WIRE at every setting — stronger than the
    // old crossover-split's allpass guarantee — and the bell's centre is
    // captured at UNITY for ANY width. The previous LR crossover-split product
    // (HP@fLo · LP@fHi) attenuated its own centre as the band narrowed
    // (skirts overlap: a Q≈7 band lost ~4 dB of its own fc), which capped a
    // narrow bell's pan at ±0.5 — the "never hard pans" report. Here a hard
    // pan removes fc content COMPLETELY on the emptied side at any Q.
    // SLOPE morph: three warm cascades (2 / 4 / 8 unity-peak sections ≈
    // 12 / 24 / 48 dB per octave skirts); adjacent cascades crossfade. Every
    // cascade is 0° phase at fc, so blends can never null, and the idle path
    // does not involve the bells at all — null-exact at every morph position.
    // One cascade serves every band shape. It was band-pass only, which let it
    // store just b0/a1/a2 (BP has b1 = 0, b2 = −b0); carrying the full biquad
    // costs two multiplies per section and means low-pass and high-pass need no
    // second filter bank, only different coefficients.
    struct FilterCascade
    {
        enum Type { BandPass, LowPass, HighPass };

        static constexpr int kMaxSections = 8;
        struct BQ { float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
        int    sections = 4;
        double sampleRate = 48000.0;
        BQ     c[kMaxSections];
        float  st[2][kMaxSections][2] {};

        void configure (int n)     { sections = juce::jlimit (1, kMaxSections, n); }
        void prepare (double sr)   { sampleRate = sr; reset(); }
        void reset()               { std::memset (st, 0, sizeof (st)); }

        // forceQ > 0 pins the per-section Q instead of deriving it from the
        // width knob. TILT needs that: its low-pass and high-pass have to SUM
        // back to the input, and a resonant pair does not — each one peaks at
        // the crossover, so the two halves overlap there and neither reaches
        // its side of the field. Butterworth-ish sections sum flat, which is
        // what makes the tilt actually tilt.
        void setParams (float fc, float widthOct, Type type, double forceQ = 0.0)
        {
            const double w  = juce::jlimit (0.05, 6.0, (double) widthOct);
            double q;
            if (type == BandPass)
            {
                // composite −3 dB width = the band's width; each of n identical
                // sections must be wider: BWsec = BWtot / sqrt(2^(1/n) − 1)
                const double qT = 1.0 / (std::pow (2.0, w * 0.5) - std::pow (2.0, -w * 0.5));
                q = juce::jmax (0.1, qT * std::sqrt (std::pow (2.0, 1.0 / sections) - 1.0));
            }
            else
            {
                // Low/High have no bandwidth to speak of, so the same knob buys
                // CORNER RESONANCE instead: wide = damped, narrow = a peak at
                // the cutoff before the rolloff. Cascading n identical sections
                // multiplies the peak in dB, so the per-section Q is pulled back
                // by 1/n to keep the composite resonance roughly constant as
                // SLOPE morphs the section count.
                if (forceQ > 0.0)
                    q = forceQ;
                else
                {
                    const double qTot = 0.7071 * std::pow (16.0, juce::jlimit (0.0, 1.0, (4.0 - w) / 3.9));
                    q = 0.7071 * std::pow (qTot / 0.7071, 1.0 / (double) sections);
                }
            }
            const double w0 = juce::MathConstants<double>::twoPi
                                * juce::jlimit (10.0, sampleRate * 0.49, (double) fc) / sampleRate;
            const double cs = std::cos (w0);
            const double alpha = std::sin (w0) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            BQ k {};
            k.a1 = (float) (-2.0 * cs / a0);
            k.a2 = (float) ((1.0 - alpha) / a0);
            if (type == BandPass)      { k.b0 = (float) ( alpha / a0);          k.b1 = 0.0f;              k.b2 = -k.b0; }
            else if (type == LowPass)  { k.b0 = (float) ((1.0 - cs) * 0.5 / a0); k.b1 = 2.0f * k.b0;      k.b2 = k.b0;  }
            else                       { k.b0 = (float) ((1.0 + cs) * 0.5 / a0); k.b1 = -2.0f * k.b0;     k.b2 = k.b0;  }
            for (int s = 0; s < sections; ++s)
                c[s] = k;
        }

        float processSample (int ch, float x)
        {
            for (int s = 0; s < sections; ++s)
            {
                auto& k = c[s]; auto* z = st[ch][s];
                const float y = k.b0 * x + z[0];
                z[0] = k.b1 * x - k.a1 * y + z[1];
                z[1] = k.b2 * x - k.a2 * y;
                x = y;
            }
            return x;
        }
    };
    using BellCascade = FilterCascade;   // the bell path still reads this name

    // Band shapes. All but Tilt are a single unity-gain extraction, so they drop
    // straight into out = in + B(in)·lift·(g−1). Notch needs no filter of its
    // own — it is what the bell leaves behind.
    enum BandShape { ShapeBell = 0, ShapeLow, ShapeHigh, ShapeNotch, ShapeTilt, kNumShapes };

    struct BandDSP
    {
        BellCascade bell2, bell4, bell8;      // ≈12 / 24 / 48 dB/oct skirts
        // Second bank, only fed by Tilt: it needs a low-pass AND a high-pass at
        // once so the two halves can be panned in opposite directions. Every
        // other shape leaves these silent and pays nothing for them.
        BellCascade hi2, hi4, hi8;
        juce::SmoothedValue<float> slope01;   // 0..1 (param /100), 30 ms

        juce::SmoothedValue<float> lift;      // 0..1
        juce::SmoothedValue<float> gainLin;   // linear, from ±6 dB
        juce::SmoothedValue<float> enable;    // 0..1 engage crossfade (~30 ms)
        juce::SmoothedValue<float> depth;     // 0..1, per-sample (automation-safe)
        juce::SmoothedValue<float> bias;      // -1..1 static pan offset (resting position)
        float fcCur = 1000.0f, wCur = 1.0f;            // block-rate fc / width glide state
        float fcApplied = -1.0f, wApplied = -1.0f;     // last values pushed into the bells
        int   shapeApplied = -1;                       // a shape change must re-push too
        bool  cutoffsInit = false;

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            const double sr = spec.sampleRate;
            bell2.configure (2); bell4.configure (4); bell8.configure (8);
            bell2.prepare (sr);  bell4.prepare (sr);  bell8.prepare (sr);
            hi2.configure (2);   hi4.configure (4);   hi8.configure (8);
            hi2.prepare (sr);    hi4.prepare (sr);    hi8.prepare (sr);
            slope01.reset (sr, 0.030);
            lift.reset    (sr, 0.005);
            gainLin.reset (sr, 0.005);
            enable.reset  (sr, 0.030);
            depth.reset   (sr, 0.020);
            bias.reset    (sr, 0.020);
            cutoffsInit = false;
        }

        void resetState()
        {
            bell2.reset(); bell4.reset(); bell8.reset();
            hi2.reset();   hi4.reset();   hi8.reset();
        }

        void pushBellParams (float fc, float widthOct, int shape)
        {
            // The primary bank carries whatever the shape extracts; Notch reads
            // the bell and subtracts, so it configures as band-pass too.
            const auto tA = shape == ShapeLow  ? FilterCascade::LowPass
                          : shape == ShapeHigh ? FilterCascade::HighPass
                          : shape == ShapeTilt ? FilterCascade::LowPass
                                               : FilterCascade::BandPass;
            // Tilt pins both halves to Butterworth so they sum back to the
            // input; every other shape lets the width knob set resonance.
            // Tilt pins both halves to Q = 0.7071: two identical such sections are
            // exactly a squared Butterworth — Linkwitz-Riley 4 — whose halves sum
            // to a magnitude-flat allpass. Every other shape takes resonance from
            // the width knob.
            const double qT = shape == ShapeTilt ? 0.7071 : 0.0;
            bell2.setParams (fc, widthOct, tA, qT);
            bell4.setParams (fc, widthOct, tA, qT);
            bell8.setParams (fc, widthOct, tA, qT);
            if (shape == ShapeTilt)
            {
                hi2.setParams (fc, widthOct, FilterCascade::HighPass, qT);
                hi4.setParams (fc, widthOct, FilterCascade::HighPass, qT);
                hi8.setParams (fc, widthOct, FilterCascade::HighPass, qT);
            }
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
    juce::SmoothedValue<float> soloSm;       // 0 = mix, 1 = soloed bands only
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
        std::atomic<float>* slope;      // 0..100 %, continuous log morph: 12 → 24 → 48 dB/oct
        std::atomic<float>* solo;       // monitor this band alone (its bell, panned)
        std::atomic<float>* shape;      // Bell / Low / High / Notch / Tilt
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
    std::atomic<float>* pFlux    = nullptr;
    std::atomic<float>* pEnvAtk  = nullptr;
    std::atomic<float>* pEnvRel  = nullptr;
    std::atomic<float>* pEnvScf  = nullptr;
    std::atomic<float>* pEnvRms  = nullptr;
    std::atomic<float>* pEnvGain = nullptr;

    // Envelope follower (global detection circuit — feeds every Env-mode band).
    float envScLp = 0.0f;   // sidechain HPF one-pole state
    float envState = 0.0f;  // ballistics state
    float globalEnv = 0.0f; // 0..1, updated per sample from the input copy

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> wfDelay { 4096 };
    double wowPhase = 0.0, flutPhase = 0.0;
    // FLUX state: per-revolution random walk + rate jitter for each modulator.
    double wowWalk = 0.0, flutWalk = 0.0;      // smoothed value, bounded -1..1
    double wowWalkT = 0.0, flutWalkT = 0.0;    // target dealt on each wrap
    double wowJit = 1.0, flutJit = 1.0;        // rate multiplier for this revolution
    juce::Random fluxRng { 0x656E7472 };       // fixed seed: same tape every session
    juce::SmoothedValue<float> wfEngage;
    // tap-depth smoothing: block-rate depth jumps clicked while turning the knobs
    juce::SmoothedValue<float> wowDepthSm, flutDepthSm;

    // Host PDC. setLatencySamples() reaches the host through listener callbacks,
    // so the audio thread only publishes the wanted value and the message thread
    // applies it.
    std::atomic<int> wantedLatency { 0 };
    void handleAsyncUpdate() override;

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

                juce::AbstractFifo analyzerFifo { 1 << 17 };
    std::vector<float> analyzerStore;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntropanAudioProcessor)
};
