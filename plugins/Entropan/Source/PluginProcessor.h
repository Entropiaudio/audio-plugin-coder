#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <limits>

// The DSP building blocks live in Source/dsp/ (one concern per header); this
// class keeps access-identical aliases for every relocated name, so call sites
// still read EntropanAudioProcessor::FilterCascade, ::kNumBands, ShapeTilt…
#include "dsp/EntropanSpec.h"
#include "dsp/FilterCascade.h"
#include "dsp/BandDSP.h"
#include "dsp/Modulator.h"
#include "dsp/Steps.h"
#include "dsp/ModRouting.h"
#include "dsp/BandParams.h"
#include "dsp/WowFlutter.h"
#include "dsp/EnvFollower.h"

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
    static constexpr int kNumBands = entropan::kNumBands;
    static constexpr int kMaxSteps = entropan::kMaxSteps;

    // Wow & flutter spec — rationale and derivations in dsp/EntropanSpec.h.
    static constexpr double kWowRate    = entropan::kWowRate;
    static constexpr double kFlutRate   = entropan::kFlutRate;
    static constexpr double kWowPitch   = entropan::kWowPitch;
    static constexpr double kFlutPitch  = entropan::kFlutPitch;
    static constexpr double kWowPeakS   = entropan::kWowPeakS;
    static constexpr double kFlutPeakS  = entropan::kFlutPeakS;
    static constexpr double kBaseDelayS = entropan::kBaseDelayS;
    static constexpr double kEngageS    = entropan::kEngageS;
    static constexpr double kFluxJitter = entropan::kFluxJitter;

    // Samples the wet path adds when engaged; 0 when both knobs are down.
    int wfLatencySamples() const noexcept;
    static constexpr int kNumWaves = entropan::kNumWaves;   // Sine, Tri, S&H, Chaos, Steps, Env

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
    // Route structs + dst-index layout in dsp/ModRouting.h / dsp/EntropanSpec.h.
    static constexpr int kMaxRoutes = entropan::kMaxRoutes;
    static constexpr int kDestSlotsPerBand = entropan::kDestSlotsPerBand;
    static constexpr int kNumDests = entropan::kNumDests;
    using ModRoute   = entropan::ModRoute;
    using RoutesData = entropan::RoutesData;
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
    // Band extraction + shapes — topology rationale in dsp/FilterCascade.h and
    // dsp/BandDSP.h. Aliased so the .cpp keeps its unqualified names.
    using FilterCascade = entropan::FilterCascade;
    using BellCascade   = entropan::BellCascade;
    using BandShape     = entropan::BandShape;
    static constexpr auto ShapeBell  = entropan::ShapeBell;
    static constexpr auto ShapeLow   = entropan::ShapeLow;
    static constexpr auto ShapeHigh  = entropan::ShapeHigh;
    static constexpr auto ShapeNotch = entropan::ShapeNotch;
    static constexpr auto ShapeTilt  = entropan::ShapeTilt;
    static constexpr auto kNumShapes = entropan::kNumShapes;
    using BandDSP = entropan::BandDSP;

    std::array<BandDSP, kNumBands> bands;

    //==============================================================================
    // Per-band modulation engine — see dsp/Modulator.h.
    using Modulator = entropan::Modulator;
    std::array<Modulator, kNumBands> mods;

    juce::SmoothedValue<float> outGainSm;    // linear
    juce::SmoothedValue<float> bypassSm;     // 0 = process, 1 = bypassed
    juce::SmoothedValue<float> soloSm;       // 0 = mix, 1 = soloed bands only
    juce::SmoothedValue<float> amountSm;     // 0..1 master depth
    juce::SmoothedValue<float> routingSm;    // 0 = serial, 1 = parallel — crossfaded so the flip never pops
    juce::AudioBuffer<float> dryBuffer;

    // Cached raw parameter pointers — see dsp/BandParams.h.
    using BandParams = entropan::BandParams;
    std::array<BandParams, kNumBands> bandParams {};
    entropan::GlobalParams gp;

    // Envelope follower state — see dsp/EnvFollower.h.
    entropan::EnvFollowerState env;

    // Wow & flutter transport state — see dsp/WowFlutter.h.
    entropan::WowFlutterState wf;

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
    // ── Steps engine (Phase 4.3) — structs in dsp/Steps.h ──
    using StepSlot  = entropan::StepSlot;
    using StepsData = entropan::StepsData;

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
