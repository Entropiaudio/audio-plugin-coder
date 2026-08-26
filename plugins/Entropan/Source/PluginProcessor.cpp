#include "PluginProcessor.h"
#include "PluginEditor.h"

#if ENTROPAN_MOONBASE   // gate default lives in PluginProcessor.h (included above)
 #include <moonbase_JUCEClient/moonbase_JUCEClient.h>   // API + MOONBASE_* macros
#endif

//==============================================================================
namespace
{
    // Log-feel range via JUCE's standard skew (centre = geometric mean).
    // IMPORTANT: custom-lambda ranges serialize skew=1 to the WebView JS lib
    // (which only knows start/end/skew) — UI and host then disagree on every
    // position. Standard skew keeps C++ and JS byte-compatible.
    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        juce::NormalisableRange<float> r (lo, hi, 0.0f);
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    }

    const juce::StringArray kModeChoices    { "Sine", "Triangle", "S&H", "Chaos", "Steps", "Env" };
    // Order must match the BandShape enum.
    const juce::StringArray kShapeChoices   { "Bell", "Low Shelf", "High Shelf", "Notch", "Tilt" };
    const juce::StringArray kRateModeChoices{ "Sync", "Free", "MIDI" };
    const juce::StringArray kDivChoices     { "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bar", "4 Bar" };
    const juce::StringArray kSpeedChoices   { "/4", "/2", "x1", "x2", "x3", "x4" };
    const juce::StringArray kRoutingChoices { "Serial", "Parallel" };

    constexpr float kBandFreqDefaults[] = { 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };

    constexpr float  kSpeedFactors[] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f };
    constexpr double kDivQuarters[]  = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };  // 1/16 … 4 bars (4/4)

    // Stateless cell hash → uniform [-1, 1]. Matches the UI-sim character:
    // loop-safe (pure function of cell), reproducible per seed, re-rollable.
    inline float cellNoise (juce::int64 cell, int band, int seed, int reroll)
    {
        juce::uint64 h = (juce::uint64) cell * 0x9E3779B97F4A7C15ULL
                       ^ (juce::uint64) (band + 1) * 0xC2B2AE3D27D4EB4FULL
                       ^ (juce::uint64) (seed + reroll * 131) * 0x165667B19E3779F9ULL;
        h ^= h >> 33; h *= 0xFF51AFD7ED558CCDULL; h ^= h >> 33;
        return (float) ((double) (h >> 11) / (double) (1ULL << 53)) * 2.0f - 1.0f;
    }

    // One-pole coefficient from a time constant: 1 - e^(-1/(T·sr)).
    // T below 0.5 ms collapses to "instant" (coefficient 1).
    inline float onePoleCoeff (double seconds, double sampleRate)
    {
        return seconds < 5.0e-4 ? 1.0f
                                : 1.0f - std::exp (-1.0f / (float) (seconds * sampleRate));
    }

    // Meta-properties: live in apvts.state for persistence but must not take
    // part in undo (stripped from snapshots, preserved across undo/redo).
    // Adding one here covers strip + restore in a single place.
    struct MetaProp { const char* name; juce::var def; };
    const MetaProp kMetaProps[] = {
        { "editorWidth",  900 },
        { "editorHeight", 560 },
        { "uiLocks", juce::String() },   // CHAOS locks
    };

    juce::ValueTree stateForUndo (const juce::ValueTree& src)
    {
        auto v = src.createCopy();
        for (const auto& mp : kMetaProps)
            v.removeProperty (mp.name, nullptr);
        return v;
    }
}

//==============================================================================
EntropanAudioProcessor::EntropanAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Seed default step patterns (sine cycle, 8 slots) so the UI has data on
    // first open. Stored as JSON strings inside the APVTS state tree.
    for (int i = 0; i < kNumBands; ++i)
    {
        if (getStepsJson (i).isEmpty())
        {
            juce::String json = "{\"count\":8,\"steps\":[";
            for (int k = 0; k < 8; ++k)
            {
                const double v = std::sin (double (k) / 8.0 * juce::MathConstants<double>::twoPi);
                json << "{\"subdiv\":1,\"vals\":[" << juce::String (v, 4) << "]}";
                if (k < 7) json << ",";
            }
            json << "]}";
            setStepsJson (i, json);
        }
        else
        {
            parseStepsSnapshot (i);
        }
    }

    lastCommitted = stateForUndo (apvts.copyState());   // undo baseline

#if ENTROPAN_MOONBASE
    // Moonbase licensing: company + product id (the id MUST match
    // Resources/moonbase_api_config.json) and the plugin version. Version comes
    // from JucePlugin_VersionString so it can never go stale against the build.
    moonbaseClient = MOONBASE_INIT_API ("Entropia Audio", "entropan", JucePlugin_VersionString);
#endif
}

// Out-of-line: the Moonbase client is a unique_ptr to a type that is incomplete
// in the header, so the destructor must be emitted where the type is complete.
EntropanAudioProcessor::~EntropanAudioProcessor()
{
    cancelPendingUpdate();   // no latency callback into a half-destroyed processor
}

// Samples the wet path adds when engaged; 0 when both knobs are down.
int EntropanAudioProcessor::wfLatencySamples() const noexcept
{
    if (currentSampleRate <= 0.0)
        return 0;
    const bool on = (gp.wow != nullptr && gp.wow->load() > 0.1f)
                 || (gp.flutter != nullptr && gp.flutter->load() > 0.1f);
    return on ? (int) std::lround (kBaseDelayS * currentSampleRate) : 0;
}

void EntropanAudioProcessor::handleAsyncUpdate()
{
    setLatencySamples (wantedLatency.load (std::memory_order_relaxed));
}

//==============================================================================
void EntropanAudioProcessor::commitUndoIfChanged()
{
    auto cur = stateForUndo (apvts.copyState());
    if (lastCommitted.isValid() && cur.isEquivalentTo (lastCommitted))
        return;   // nothing changed since the last commit — no phantom entry
    if (lastCommitted.isValid())
    {
        undoStack.push_back (lastCommitted);
        if ((int) undoStack.size() > kUndoDepth)
            undoStack.erase (undoStack.begin());
    }
    lastCommitted = cur;
    redoStack.clear();
}

bool EntropanAudioProcessor::restoreSnapshot (std::vector<juce::ValueTree>& from,
                                              std::vector<juce::ValueTree>& to)
{
    if (from.empty())
        return false;
    to.push_back (stateForUndo (apvts.copyState()));
    auto snap = from.back();
    from.pop_back();
    // keep live meta-state (window size, CHAOS locks) — snapshots carry none
    for (const auto& mp : kMetaProps)
        snap.setProperty (mp.name, apvts.state.getProperty (mp.name, mp.def), nullptr);
    apvts.replaceState (snap);
    lastCommitted = stateForUndo (apvts.copyState());
    for (int i = 0; i < kNumBands; ++i)
        parseStepsSnapshot (i);
    parseRoutesSnapshot();
    return true;
}

bool EntropanAudioProcessor::undoState() { return restoreSnapshot (undoStack, redoStack); }
bool EntropanAudioProcessor::redoState() { return restoreSnapshot (redoStack, undoStack); }

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EntropanAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto pct = [] {
        return juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f);
    };

    // ─── Global (5) ───
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "seed", 1 }, "Seed", 1, 128, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "amount", 1 }, "Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "speed", 1 }, "Global Speed", kSpeedChoices, 2));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "routing", 1 }, "Routing", kRoutingChoices, 0));   // Serial (bands chain) / Parallel (independent)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Level",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    // Global wow & flutter (wet-only)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "wow", 1 }, "Wow", pct(), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "flutter", 1 }, "Flutter", pct(), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "flux", 1 }, "Flux", pct(), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Envelope follower — global detection circuit (Env mode)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "env_atk", 1 }, "Env Attack",
        logRange (1.0f, 500.0f), 20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "env_rel", 1 }, "Env Release",
        logRange (5.0f, 2000.0f), 150.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "env_scf", 1 }, "Env SC Filter",
        logRange (20.0f, 2000.0f), 20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "env_rms", 1 }, "Env RMS", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "env_gain", 1 }, "Env Gain",
        juce::NormalisableRange<float> (-36.0f, 36.0f, 0.01f), 0.0f,   // bipolar: drive up or down
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // ─── Per band (17 × 6 = 102) ───
    for (int i = 0; i < kNumBands; ++i)
    {
        const juce::String p = "b" + juce::String (i + 1) + "_";
        const juce::String label = "B" + juce::String (i + 1) + " ";

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { p + "on", 1 }, label + "Enable", false));   // default preset: no bands
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "freq", 1 }, label + "Frequency",
            logRange (20.0f, 20000.0f), kBandFreqDefaults[i],
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "width", 1 }, label + "Q",
            logRange (0.1f, 4.0f), 1.0f,
            juce::AudioParameterFloatAttributes().withLabel ("oct")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "lift", 1 }, label + "Lift", pct(), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "depth", 1 }, label + "Depth", pct(), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "gain", 1 }, label + "Gain",
            juce::NormalisableRange<float> (-6.0f, 6.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { p + "mode", 1 }, label + "Mod Type", kModeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "rate", 1 }, label + "Rate",
            logRange (0.02f, 1000.0f), 0.5f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { p + "ratemode", 1 }, label + "Rate Mode", kRateModeChoices, 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { p + "div", 1 }, label + "Interval", kDivChoices, 3));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "inertia", 1 }, label + "Inertia", pct(), 60.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "phase", 1 }, label + "Phase",
            juce::NormalisableRange<float> (0.0f, 360.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("deg")));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { p + "uni", 1 }, label + "Unipolar", false));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { p + "freeze", 1 }, label + "Freeze", false));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "bias", 1 }, label + "Bias",
            juce::NormalisableRange<float> (-100.0f, 100.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { p + "override", 1 }, label + "Bias Override", false));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "stepsmooth", 1 }, label + "Step Smooth", pct(), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "slope", 1 }, label + "Slope", pct(), 50.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));   // 0=12, 50=24, 100=48 dB/oct (log morph)
    }

    // Shape + solo, appended AFTER every other parameter on purpose: adding these
    // inside the per-band block above would shift the index of every parameter
    // that follows, and hosts that automate by index would silently re-point.
    for (int i = 0; i < kNumBands; ++i)
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "b" + juce::String (i + 1) + "_solo", 1 },
            "Band " + juce::String (i + 1) + " Solo", false));
    for (int i = 0; i < kNumBands; ++i)
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "b" + juce::String (i + 1) + "_shape", 1 },
            "Band " + juce::String (i + 1) + " Shape", kShapeChoices, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
void EntropanAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;

    for (auto& b : bands)
    {
        b.prepare (spec);
        b.resetState();
    }

    outGainSm.reset (sampleRate, 0.010);
    bypassSm.reset  (sampleRate, 0.030);
    amountSm.reset  (sampleRate, 0.020);
    routingSm.reset (sampleRate, 0.060);   // serial↔parallel crossfade (~60 ms)
    routingSm.setCurrentAndTargetValue (gp.routing != nullptr && gp.routing->load() > 0.5f ? 1.0f : 0.0f);

    dryBuffer.setSize (2, juce::jmax (samplesPerBlock * 2, 8192));   // headroom for hosts that exceed the prepared block
    analyzerStore.resize ((size_t) analyzerFifo.getTotalSize(), 0.0f);
    scopePhase = kScopeStride;

    for (auto& m : mods)
        m = Modulator {};

    wf.delay.prepare (spec);
    wf.delay.reset();
    wf.engage.reset (sampleRate, kEngageS);
    soloSm.reset (sampleRate, 0.020);
    wf.wowDepthSm.reset (sampleRate, 0.080);
    wf.flutDepthSm.reset (sampleRate, 0.080);
    wf.wowPhase = wf.flutPhase = 0.0;
    env.scLp = env.state = env.out = 0.0f;

    // Cache raw parameter pointers once (RT-safe reads afterwards).
    gp.amount = apvts.getRawParameterValue ("amount");
    gp.output = apvts.getRawParameterValue ("output");
    gp.bypass = apvts.getRawParameterValue ("bypass");
    gp.seed   = apvts.getRawParameterValue ("seed");
    gp.speed  = apvts.getRawParameterValue ("speed");
    gp.routing = apvts.getRawParameterValue ("routing");
    gp.wow     = apvts.getRawParameterValue ("wow");
    gp.flutter = apvts.getRawParameterValue ("flutter");
    gp.flux    = apvts.getRawParameterValue ("flux");
    gp.envAtk  = apvts.getRawParameterValue ("env_atk");
    gp.envRel  = apvts.getRawParameterValue ("env_rel");
    gp.envScf  = apvts.getRawParameterValue ("env_scf");
    gp.envRms  = apvts.getRawParameterValue ("env_rms");
    gp.envGain = apvts.getRawParameterValue ("env_gain");
    for (int i = 0; i < kNumBands; ++i)
    {
        const juce::String p = "b" + juce::String (i + 1) + "_";
        bandParams[(size_t) i] = {
            apvts.getRawParameterValue (p + "on"),
            apvts.getRawParameterValue (p + "freq"),
            apvts.getRawParameterValue (p + "width"),
            apvts.getRawParameterValue (p + "lift"),
            apvts.getRawParameterValue (p + "depth"),
            apvts.getRawParameterValue (p + "gain"),
            apvts.getRawParameterValue (p + "mode"),
            apvts.getRawParameterValue (p + "rate"),
            apvts.getRawParameterValue (p + "ratemode"),
            apvts.getRawParameterValue (p + "div"),
            apvts.getRawParameterValue (p + "inertia"),
            apvts.getRawParameterValue (p + "phase"),
            apvts.getRawParameterValue (p + "uni"),
            apvts.getRawParameterValue (p + "freeze"),
            apvts.getRawParameterValue (p + "bias"),
            apvts.getRawParameterValue (p + "override"),
            apvts.getRawParameterValue (p + "stepsmooth"),
            apvts.getRawParameterValue (p + "slope"),
            apvts.getRawParameterValue (p + "solo"),
            apvts.getRawParameterValue (p + "shape")
        };
    }

    // mod-matrix destination registry (raw atomic + ranged param for norm-domain offsets)
    {
        auto reg = [this] (int idx, const juce::String& id)
        {
            modDests[(size_t) idx] = { apvts.getRawParameterValue (id), apvts.getParameter (id) };
        };
        static const char* kBandSlots[kDestSlotsPerBand] =
            { "freq", "width", "lift", "depth", "gain", "rate", "inertia", "phase", "bias", "stepsmooth" };
        for (int i = 0; i < kNumBands; ++i)
            for (int s = 0; s < kDestSlotsPerBand; ++s)
                reg (i * kDestSlotsPerBand + s, "b" + juce::String (i + 1) + "_" + kBandSlots[s]);
        reg (kNumBands * kDestSlotsPerBand + 0, "amount");
        reg (kNumBands * kDestSlotsPerBand + 1, "wow");
        reg (kNumBands * kDestSlotsPerBand + 2, "flutter");
        reg (kNumBands * kDestSlotsPerBand + 3, "output");
        reg (kNumBands * kDestSlotsPerBand + 4, "flux");
    }
    parseRoutesSnapshot();

    // Publish PDC before the first block, so a session that opens with W&F
    // already up is aligned from the first sample instead of after an async hop.
    wantedLatency.store (wfLatencySamples(), std::memory_order_relaxed);
    setLatencySamples (wantedLatency.load (std::memory_order_relaxed));

#if ENTROPAN_MOONBASE
    // Give the trial/lock signal interrupter the current rate so its timing is
    // correct. No-op while licensed or in trial.
    MOONBASE_PREPARE_TO_PLAY (sampleRate, samplesPerBlock);
#endif
}

bool EntropanAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo in → stereo out only (spectral panning needs two output channels).
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void EntropanAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    if (rerollFlag.exchange (false))
    {
        ++rerollOffset;   // shifts every random stream (S&H)
        for (int i = 0; i < kNumBands; ++i)   // kick the Lorenz attractors
        {
            auto& m = mods[(size_t) i];
            m.lx = 0.1 + 0.2 * (double) cellNoise (rerollOffset, i, (int) gp.seed->load(), 0);
            m.ly = 0.0; m.lz = 0.0;
        }
    }

    // ── MIDI rate mode: track last note-on (mono, last-note priority) ──
    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            midiFreq = (float) juce::MidiMessage::getMidiNoteInHertz (note);
            lastMidiNote.store (note);
        }
    }
    midiMessages.clear();   // consumed, not passed through

    // ── transport ──
    double bpm = 120.0, ppq = 0.0;
    bool hasPpq = false, playing = false;
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            if (const auto t = pos->getBpm())         bpm = *t;
            if (const auto q = pos->getPpqPosition()) { ppq = *q; hasPpq = true; }
            playing = pos->getIsPlaying();
        }
    }
    bpm = juce::jlimit (20.0, 999.0, bpm);
    const double quartersPerSample = (bpm / 60.0) / currentSampleRate;

    // ── dry copy for the bypass crossfade ──
    dryBuffer.setSize (2, numSamples, false, false, true);
    for (int ch = 0; ch < 2; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ── per-block parameter targets ──
    const int   seedV    = (int) gp.seed->load();
    const float speedV   = kSpeedFactors[juce::jlimit (0, 5, (int) gp.speed->load())];

    // ── mod matrix: offset destinations in the NORMALIZED domain (block rate).
    // Source = each band's post-slew modulator (last block's value: mods tick
    // per sample below), through the SOURCE BAND'S POLARITY: BI swings the
    // destination ±depth around the knob, UNI pushes one-way from it (sign of
    // depth = direction). Applied post-read → host automation/UI untouched.
    // Per-band consumer mask: bit t set → waveform t has a consumer this block
    // (the band's own MODE, OR'd in below, or any route tapping it here). Only
    // masked waveforms tick in the sample loop — with no routes that is one
    // waveform per band instead of six. A dormant waveform holds its value
    // (same spirit as FREEZE) and catches up over one slew constant when a
    // route first taps it; the pan's own waveform is always masked in, so the
    // audible path is bit-identical. Routes swap only at block boundaries
    // (RT snapshot), so the mask cannot go stale mid-block.
    juce::uint8 waveMask[kNumBands] = {};
    {
        const auto& rd = routesBuf[(size_t) routesActive.load (std::memory_order_acquire)];
        float acc[kNumDests];
        bool  touched[kNumDests] = {};
        for (int r = 0; r < rd.count; ++r)
        {
            const auto& rt = rd.routes[r];
            if (rt.dst < 0 || rt.dst >= kNumDests || modDests[(size_t) rt.dst].param == nullptr)
                continue;
            if (! touched[rt.dst]) { acc[rt.dst] = 0.0f; touched[rt.dst] = true; }
            const int src = juce::jlimit (0, kNumBands - 1, rt.src);
            // stype picks the source band's waveform; −1 follows its MODE (legacy)
            const int ty = rt.stype >= 0 && rt.stype < kNumWaves
                             ? rt.stype
                             : juce::jlimit (0, kNumWaves - 1, (int) bandParams[(size_t) src].mode->load());
            waveMask[src] = (juce::uint8) (waveMask[src] | (1u << ty));
            const float raw = mods[(size_t) src].value[ty];
            const bool srcUni = bandParams[(size_t) src].uni->load() > 0.5f;
            const float s = srcUni ? (raw + 1.0f) * 0.5f    // 0..1, one-way
                                   : raw;                   // −1..1, both ways
            acc[rt.dst] += s * rt.depth * 0.01f;
        }
        for (int d = 0; d < kNumDests; ++d)
        {
            const auto& md = modDests[(size_t) d];
            if (md.raw == nullptr) continue;
            const float raw = md.raw->load();
            modVal[(size_t) d] = touched[d]
                ? md.param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, md.param->convertTo0to1 (raw) + acc[d]))
                : raw;
        }
    }
    const float* bandMod = modVal.data();                       // band d = i*10+slot
    const float* globMod = modVal.data() + kNumBands * kDestSlotsPerBand;   // amount,wow,flutter,output

    amountSm.setTargetValue (globMod[0] * 0.01f);

    struct BandBlock   // per-block modulator config, gathered outside the sample loop
    {
        int    mode = 0, rateMode = 0;
        double cycPerSample = 0.0;    // free/MIDI: cycle increment per sample
        double quartersPerCycle = 1.0;// sync: musical cycle length
        float  phaseOff = 0.0f;
        float  lorenzDt = 0.0f;
        bool   uni = false;
        bool   freeze = false;     // pause: hold all six waveform values, stop the clock
        bool   biasFree = false;   // override: bias not headroom-clamped → pan may reach the rail
        juce::uint8 waves = 0;     // consumer mask: which waveforms tick this block
        int    zone = 0;           // slope blend pair: 0 = bell2↔bell4, 1 = bell4↔bell8
        bool   solo = false;       // monitor this band alone
        int    shape = 0;          // BandShape
        const StepsData* steps = nullptr;   // RT snapshot, resolved once per block
    };
    std::array<BandBlock, kNumBands> bb;
    bool anyEnv = false;   // any band in Env mode → run the envelope detector

    for (int i = 0; i < kNumBands; ++i)
    {
        auto& b  = bands[(size_t) i];
        auto& m  = mods[(size_t) i];
        auto& pp = bandParams[(size_t) i];
        auto& cfg = bb[(size_t) i];

        const bool on = pp.on->load() > 0.5f;
        b.enable.setTargetValue (on ? 1.0f : 0.0f);

        const float* mv = bandMod + i * kDestSlotsPerBand;   // this band's post-matrix values

        // Band edges from centre + width (octaves), clamped into the audible
        // range with a guaranteed f_lo < f_hi ordering.
        const float freq  = mv[0];
        const float width = mv[1];
        // Shape must be known BEFORE the coefficient push below, which depends
        // on it — cfg is rebuilt every block, so reading it later left the push
        // looking at a default-constructed 0.
        cfg.shape = juce::jlimit (0, kNumShapes - 1, (int) pp.shape->load());
        // SLOPE morph: smoothed 0..1; zone picked at block rate (0 = 12↔24,
        // 1 = 24↔48). All three bell cascades stay warm all the time — a
        // cascade entering the blend cold would step the output.
        b.slope01.setTargetValue (juce::jlimit (0.0f, 1.0f, pp.slope->load() * 0.01f));
        cfg.zone = b.slope01.getCurrentValue() < 0.5f ? 0 : 1;
        // Hold the OUT-of-zone cascade at zero state so its next entry (weight
        // ramps from 0) is a clean ring-up from silence, not a stale-state
        // thump. bell4 is live in both zones and never reset here.
        if (cfg.shape == ShapeTilt)
        {
            // Tilt runs ONLY the 2-section banks (fixed LR4); everything else
            // idles and is held reset so a shape change re-enters cleanly.
            b.bell4.reset(); b.bell8.reset(); b.hi4.reset(); b.hi8.reset();
        }
        else if (cfg.zone == 0) { b.bell8.reset(); b.hi8.reset(); }
        else                    { b.bell2.reset(); b.hi2.reset(); }

        const float fcT = juce::jlimit (20.0f, 20000.0f, freq);
        const float wT  = juce::jlimit (0.05f, 6.0f, width);
        if (! b.cutoffsInit) { b.fcCur = fcT; b.wCur = wT; b.cutoffsInit = true; }
        // block-rate glide (~40 ms) — no zipper while dragging freq/Q
        const float ck = juce::jmin (1.0f, (float) numSamples / (0.040f * (float) currentSampleRate));
        b.fcCur += (fcT - b.fcCur) * ck;
        b.wCur  += (wT  - b.wCur)  * ck;
        // push coefficients only while actually gliding
        if (std::abs (b.fcCur - b.fcApplied) > 0.01f || std::abs (b.wCur - b.wApplied) > 0.0005f
            || cfg.shape != b.shapeApplied)
        {
            b.pushBellParams (b.fcCur, b.wCur, cfg.shape);
            b.fcApplied = b.fcCur;
            b.wApplied  = b.wCur;
            b.shapeApplied = cfg.shape;
        }

        b.lift.setTargetValue (mv[2] * 0.01f);
        b.gainLin.setTargetValue (juce::Decibels::decibelsToGain (mv[4]));

        // ── modulator config ──
        cfg.mode     = juce::jlimit (0, kNumWaves - 1, (int) pp.mode->load());
        cfg.rateMode = (int) pp.ratemode->load();
        cfg.phaseOff = mv[7] / 360.0f;
        cfg.uni      = pp.uni->load() > 0.5f;
        cfg.freeze   = pp.freeze->load() > 0.5f;
        cfg.biasFree = pp.biasFree->load() > 0.5f;
        cfg.solo     = pp.solo->load() > 0.5f;
        // routes' bits (gathered in the matrix scan) + the pan's own waveform
        cfg.waves    = (juce::uint8) (waveMask[i] | (1u << cfg.mode));
        cfg.steps    = &stepsBuf[(size_t) i][(size_t) stepsActive[(size_t) i].load (std::memory_order_acquire)];
        // Env consumed by anything on this band (its MODE or any route bit 5)
        // → the global detector must run. Replaces the separate route scan.
        anyEnv = anyEnv || (cfg.waves & (1u << 5)) != 0;
        b.depth.setTargetValue (mv[3] * 0.01f);
        b.bias.setTargetValue (juce::jlimit (-1.0f, 1.0f, mv[8] * 0.01f));

        double rateHz = 0.0;
        if (cfg.rateMode == 0)        // sync
        {
            cfg.quartersPerCycle = kDivQuarters[juce::jlimit (0, 6, (int) pp.div->load())] / speedV;
            rateHz = (bpm / 60.0) / cfg.quartersPerCycle;
        }
        else if (cfg.rateMode == 1)   // free (up to audio rate)
        {
            rateHz = (double) mv[5] * speedV;
        }
        else                          // MIDI: last note frequency (0 = frozen)
        {
            rateHz = (double) midiFreq * speedV;
        }
        cfg.cycPerSample = rateHz / currentSampleRate;

        // Lorenz integration step scales with rate; clamped for stability.
        cfg.lorenzDt = (float) juce::jmin (0.02, cfg.cycPerSample * 1.2);

        // Inertia → per-sample one-pole slew. Live on EVERY mode; reach stays
        // owned by DEPTH (never shrunk by inertia except by physical necessity):
        //   Sine/Tri   — mild lag, slew corner kept ABOVE the mod rate so the
        //                waveform passes at ~full amplitude (reach preserved)
        //   S&H/Steps  — slew capped at cyclePeriod/3.5 → pan always ARRIVES at
        //                the full target before the next deal
        //   Chaos      — free viscosity (an unbounded wanderer by nature)
        const float  inertia = mv[6] * 0.01f;
        const double i2 = (double) inertia * inertia;
        const double periodS = cfg.cycPerSample > 1.0e-9
                                 ? 1.0 / (cfg.cycPerSample * currentSampleRate) : 1.0e9;
        // All six waveforms run per band (any can feed the mod matrix), so every
        // one gets its own coefficient — same formulas as when it was the single
        // mode-selected engine. The browser sim (index.html tickMods) is an
        // APPROXIMATION for the little scope, not a mirror: it lumps Steps in
        // with S&H and never reads SMOOTH. That costs nothing today because
        // Steps shows the step editor rather than the scope, and the editor
        // models the glide term itself.
        for (int t = 0; t < kNumWaves; ++t)
        {
            double slewT;
            if (t == 2)                               // S&H
                slewT = juce::jmin (0.004 + i2 * 2.0, periodS / 3.5);
            else if (t == 4)                          // Steps — dedicated SMOOTH (square → glide)
            {
                const float sm = mv[9] * 0.01f;   // 0 = square, 1 = glide
                const int cnt = cfg.steps->count;
                const double stepDur = cnt > 0 ? periodS / (double) cnt : periodS;
                const double glide = (double) sm * sm * stepDur * 1.5;  // corner scales with one step
                // SMOOTH = 0 used to mean slewT = 0, i.e. coefficient 1 — the pan
                // jumped between step values in a single sample. That is a
                // discontinuity in the pan gains at EVERY edge, and a fast
                // sequencer delivers dozens per second, so "square" clicked.
                // Every other waveform already carries a floor (S&H and Chaos
                // 4 ms, Env 5 ms); Steps was the one branch without one.
                // 1.5 ms is short enough to still read as square, and the
                // stepDur/6 cap stops the floor from eating the step itself
                // once the rate climbs.
                slewT = juce::jmax (glide, juce::jmin (0.0015, stepDur / 6.0));
            }
            else if (t == 3)                          // Chaos — free viscosity (unbounded wanderer)
                slewT = juce::jmin (2.0, 0.004 + i2 * 2.0);
            else if (t == 5)                          // Env — absolute floor, NOT rate-derived
                // The detector ripples at 2× the programme frequency; without a
                // floor (smooth=0 → coeff=1) the pan tracked that ripple
                // per-sample — audio-rate AM heard as noise that grew with ENV
                // GAIN. 5 ms kills the ripple while still feeling instant;
                // SMOOTH adds glide. periodS is meaningless here — Env follows
                // the audio, not the band's RATE. (B67, T26)
                slewT = 0.005 + i2 * 0.5;
            else                                      // Sine / Tri
                slewT = i2 * 0.10 * periodS;          // corner ≈ 1.6·rate at max
            m.slewCoeff[t] = onePoleCoeff (slewT, currentSampleRate);
        }
    }

    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (globMod[3]));
    bypassSm.setTargetValue (gp.bypass->load() > 0.5f ? 1.0f : 0.0f);
    // Any band armed puts the whole plugin in solo; a soloed band that is
    // switched off contributes silence, which is the useful answer (you hear
    // that you soloed nothing) rather than falling back to the mix.
    {
        bool anySolo = false;
        for (int i = 0; i < kNumBands; ++i)
            anySolo = anySolo || bb[(size_t) i].solo;
        soloSm.setTargetValue (anySolo ? 1.0f : 0.0f);
    }
    // Serial chains bands; Parallel runs each on the dry input. The topology is
    // CONTINUOUS in rx = routingSm (0..1): band input lerps chain→dry and both
    // recombination laws blend, so flipping the switch never steps the filter
    // inputs (a hard swap popped). Settled endpoints are bit-exact serial/parallel.
    routingSm.setTargetValue (gp.routing->load() > 0.5f ? 1.0f : 0.0f);

    // ── envelope-follower coefficients (global detection circuit) ──
    const bool  envRms  = gp.envRms->load() > 0.5f;
    const float envAtkC = onePoleCoeff (juce::jmax (1.0f, gp.envAtk->load()) * 0.001, currentSampleRate);
    const float envRelC = onePoleCoeff (juce::jmax (1.0f, gp.envRel->load()) * 0.001, currentSampleRate);
    const float envScC  = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * gp.envScf->load() / (float) currentSampleRate);
    const float envGainLin = juce::Decibels::decibelsToGain (gp.envGain->load());   // detector drive

    // ── wow & flutter setup ──
    const float wowAmt  = globMod[1] * 0.01f;
    const float flutAmt = globMod[2] * 0.01f;
    const bool  wfOn    = (wowAmt > 0.001f || flutAmt > 0.001f);
    wf.engage.setTargetValue (wfOn ? 1.0f : 0.0f);
    const double wowInc  = kWowRate  / currentSampleRate;
    const double flutInc = kFlutRate / currentSampleRate;
    const float  fluxAmt = juce::jlimit (0.0f, 1.0f, globMod[4] * 0.01f);
    // The walk settles over about a quarter revolution, so it reads as drift
    // rather than as a stepped sample-and-hold.
    const float  wowWalkC  = (float) juce::jmin (1.0, 4.0 * wowInc);
    const float  flutWalkC = (float) juce::jmin (1.0, 4.0 * flutInc);
    const float  baseDelay = (float) (kBaseDelayS * currentSampleRate);
    // tap depths are SMOOTHED per sample — a block-rate depth change jumps the
    // delay read position (up to ms) and clicks while the knobs move (T35)
    wf.wowDepthSm.setTargetValue  (wowAmt  * (float) (kWowPeakS  * currentSampleRate));
    wf.flutDepthSm.setTargetValue (flutAmt * (float) (kFlutPeakS * currentSampleRate));

    // Host PDC. The wet path reads the line baseDelay behind, so engaging adds
    // exactly that many samples and idling adds none. Decided from the RAW
    // parameters rather than the modulated values on purpose: a mod source
    // sweeping WOW across zero would otherwise toggle the reported latency
    // every few blocks, and hosts rebuild delay compensation on every change.
    // (Cost: a route that drives W&F from a knob sitting at 0 stays
    // uncompensated — a mod-only engage is not a steady state to report.)
    {
        const int want = wfLatencySamples();
        if (want != wantedLatency.load (std::memory_order_relaxed))
        {
            wantedLatency.store (want, std::memory_order_relaxed);
            triggerAsyncUpdate();
        }
    }

    // The delay line is FED unconditionally (see the per-sample tap below) so
    // it is always warm — the old skip-and-reset-on-engage scheme blended the
    // engage crossfade into a half-empty line: the tap sits ~17 ms deep while
    // the fade runs 20 ms, so the first engaged blocks mixed in silence and
    // thumped (T35 argmax landed exactly on the engage block). Only the TAP
    // and the mix are gated now; pushing two samples is negligible.
    const bool wfActive = wfOn || wf.engage.getCurrentValue() > 1.0e-4f;

    // ── per-sample cascade ──
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const float* dryL = dryBuffer.getReadPointer (0);
    const float* dryR = dryBuffer.getReadPointer (1);

    for (int s = 0; s < numSamples; ++s)
    {
        float xl = left[s], xr = right[s];
        const float xinL = xl, xinR = xr;      // dry-in — the parallel side reads this
        float accL = 0.0f, accR = 0.0f;        // parallel: summed per-band displacements
        float soloL = 0.0f, soloR = 0.0f;      // solo: summed per-band bells, panned
        const float amountV = amountSm.getNextValue();
        const float rx = routingSm.getNextValue();       // 0 = serial … 1 = parallel
        const float sxw = 1.0f - rx;                     // serial-side weight

        // envelope detector (from the pre-process input copy) — only while some
        // band is in Env mode; otherwise env.out holds its last value (unseen)
        if (anyEnv)
        {
            const float inMono = 0.5f * (dryL[s] + dryR[s]) * envGainLin;
            env.scLp += envScC * (inMono - env.scLp);      // one-pole LP …
            const float hp = inMono - env.scLp;           // … input minus LP = SC high-pass
            const float det = envRms ? hp * hp : std::abs (hp);
            env.state += (det > env.state ? envAtkC : envRelC) * (det - env.state);
            const float e = envRms ? std::sqrt (juce::jmax (0.0f, env.state)) : env.state;
            env.out = juce::jlimit (0.0f, 1.0f, e * 2.0f);
        }

        for (int i = 0; i < kNumBands; ++i)
        {
            auto& b   = bands[(size_t) i];
            auto& m   = mods[(size_t) i];
            auto& cfg = bb[(size_t) i];

            const float e      = b.enable.getNextValue();
            const float liftV  = b.lift.getNextValue();
            const float gainV  = b.gainLin.getNextValue();
            const float depthV = b.depth.getNextValue();   // advance even when
            const float biasV  = b.bias.getNextValue();    // disabled — no lag on re-enable

            if (e < 1.0e-4f)
                continue;   // fully disengaged: stage is a wire (filters stay cold)

            // ── modulator tick (per sample) ──
            // FREEZE pauses the band's whole modulation clock: phase holds, no
            // new targets, no slew — all six values (and any routes fed from
            // them) hold perfectly still. Depth/bias/gain smoothing stays live
            // above, so the held position still responds to the knobs. Free-run
            // resumes seamlessly; sync recomputes from song position on
            // release (same jump as a playhead relocate).
            if (! cfg.freeze) {
            if (cfg.rateMode == 0 && hasPpq && playing)
            {
                // Sync while rolling: phase is a pure function of song position
                // → loop jumps and relocates land exactly on the grid.
                const double q = ppq + (double) s * quartersPerSample;
                m.phase = q / cfg.quartersPerCycle;
            }
            else
            {
                m.phase += cfg.cycPerSample;   // free-run (also sync w/ stopped transport)
            }

            const double cyc = m.phase + (double) cfg.phaseOff;
            const double frac = cyc - std::floor (cyc);

            // ── all six waveforms tick from this one clock ──
            // The pan takes value[mode]; the mod matrix may tap ANY of them, so
            // S&H can drive freq while Sine drives gain from the same band.
            // (Different RATES per destination = route from another band — each
            // band has its own clock.)

            // Only waveforms with a consumer this block tick (cfg.waves) — a
            // dormant one holds its value and slews back in when first tapped.
            const juce::uint8 wm = cfg.waves;

            // Sine / Tri: pure functions of phase
            if (wm & (1u << 0)) m.target[0] = std::sin ((float) frac * juce::MathConstants<float>::twoPi);
            if (wm & (1u << 1)) m.target[1] = 1.0f - 4.0f * std::abs ((float) frac - 0.5f);

            if (wm & (1u << 2))
            { // S&H: new deal each cycle boundary (phase-offset agnostic)
              // (smooth it with the SMOOTH fader for a glide)
                const auto shCell = (juce::int64) std::floor (m.phase);
                if (shCell != m.lastCell || seedV != m.lastSeed || rerollOffset != m.lastReroll)
                {   // hash only when the cell (or its inputs) change
                    m.cellA = cellNoise (shCell, i, seedV, rerollOffset);
                    m.lastCell = shCell; m.lastSeed = seedV; m.lastReroll = rerollOffset;
                }
                m.target[2] = m.cellA;
            }

            if (wm & (1u << 3))
            { // Chaos: Lorenz, x-component normalised (one stream per band)
                const float dt = cfg.lorenzDt;
                const double nx = m.lx + 10.0 * (m.ly - m.lx) * dt;
                const double ny = m.ly + (m.lx * (28.0 - m.lz) - m.ly) * dt;
                const double nz = m.lz + (m.lx * m.ly - (8.0 / 3.0) * m.lz) * dt;
                m.lx = nx; m.ly = ny; m.lz = nz;
                if (! std::isfinite (m.lx)) { m.lx = 0.1; m.ly = 0.0; m.lz = 0.0; }
                m.target[3] = juce::jlimit (-1.0f, 1.0f, (float) (m.lx / 18.0));
            }

            if (wm & (1u << 4))
            { // Steps: slot-stable ratchets + glue runs (RT snapshot, per block)
                const auto& sd = *cfg.steps;
                if (sd.count > 0)
                {
                    const double sp = frac * (double) sd.count;         // position in cells
                    const int cell = juce::jlimit (0, sd.count - 1, (int) sp);
                    const int rs  = sd.runStart[cell];                  // glued run leader
                    const int rl  = juce::jmax (1, sd.runLen[cell]);
                    const auto& sl = sd.slots[rs];                      // leader holds the value
                    const double local = (sp - (double) rs) / (double) rl;   // 0..1 across the run
                    const int sub = juce::jlimit (0, sl.subdiv - 1, (int) (local * (double) sl.subdiv));
                    m.target[4] = sl.vals[sub];
                }
                else
                    m.target[4] = 0.0f;
            }

            // Env: global envelope follower
            if (wm & (1u << 5))
                m.target[5] = juce::jlimit (-1.0f, 1.0f, env.out * 2.0f - 1.0f);

            // each consumed waveform slews independently (its own coefficient)
            for (int t = 0; t < kNumWaves; ++t)
                if (wm & (1u << t))
                    m.value[t] += (m.target[t] - m.value[t]) * m.slewCoeff[t];
            }   // ! cfg.freeze

            // pan takes the MODE-selected waveform → uni/bipolar transform →
            // static bias → depth·amount. Bipolar: swing L↔R through centre.
            // Unipolar: (v+1)/2 = centre→one side (works for every waveform,
            // incl. Env). Bias offsets the resting centre. panOut = the final
            // pan, so scope/telemetry show it exactly.
            const float panV0 = m.value[cfg.mode];   // mode pre-clamped at block rate
            const float mv    = cfg.uni ? (panV0 + 1.0f) * 0.5f : panV0;
            // Limit bias to the headroom left by the swing so bias + full swing
            // never crosses ±1 (no rail-clipping / flattened modulation). Override
            // (biasFree) lifts that cap so a hard bias can keep full depth — the
            // final jlimit then clamps the pan at the rail (intentional clip).
            const float reach   = juce::jlimit (0.0f, 1.0f, depthV * amountV);
            const float biasMax = cfg.biasFree ? 1.0f : (1.0f - reach);
            const float biasC   = juce::jlimit (-biasMax, biasMax, biasV);
            const float panV = juce::jlimit (-1.0f, 1.0f, biasC + mv * depthV * amountV);
            m.panOut = panV;

            // Serial (rx→0): band N processes band N-1's output (running xl).
            // Parallel (rx→1): every band processes the same dry input (xinL).
            // Mid-crossfade the input lerps between them — no step, no pop.
            const float inL = xl + (xinL - xl) * rx;
            const float inR = xr + (xinR - xr) * rx;

            // subtractive displacement: band = unity-peak bell (full centre
            // capture at ANY width); residual = in − band by construction.
            // The zone (block-rate) picks the blend pair — bell4 is live in
            // BOTH zones, the out-of-zone cascade enters at weight 0. Only the
            // two in-zone cascades run; the dormant one is held reset (below)
            // so its next entry is a clean zero-state ramp, not a stale ring.
            const int   zone = cfg.zone;
            const float s2 = b.slope01.getNextValue() * 2.0f;   // advance even when tilt ignores it

            // equal-power pan (balance law, ×√2 so centre = unity)
            const float theta = (panV + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float gL = std::cos (theta) * juce::MathConstants<float>::sqrt2 * gainV;
            const float gR = std::sin (theta) * juce::MathConstants<float>::sqrt2 * gainV;

            float outL, outR;
            float sBandL = 0.0f, sBandR = 0.0f;   // this band's solo-bus contribution

            if (cfg.shape == ShapeTilt)
            {
                // TILT is a REAL Linkwitz-Riley LR4 split, taken fully wet.
                // Earlier displacement-based attempts could not tilt hard —
                // symmetric resonant pair 1.9:1, Butterworth pair 1.14:1,
                // in−LP inverted the lows — all the same fact: adding a
                // phase-shifted extraction to DRY signal cancels by vector sum.
                // LR4's halves are phase-ALIGNED with each other and sum to a
                // magnitude-flat allpass, so replacing the signal with
                // low·g + high·gMirror separates completely. The cost, stated
                // plainly: an ENGAGED tilt at centre is an allpass of the
                // input — RMS-identical, not bit-identical (T43j asserts the
                // RMS). The band-off wire stays exact via the enable fade.
                // Slope is fixed at LR4 for tilt: complementarity requires the
                // exact Butterworth Q split, which the identical-section 4/8
                // cascades do not have.
                const float loL = b.bell2.processSample (0, inL), loR = b.bell2.processSample (1, inR);
                const float hiL = b.hi2.processSample (0, inL),  hiR = b.hi2.processSample (1, inR);
                const float thetaM = (-panV + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                const float mL = std::cos (thetaM) * juce::MathConstants<float>::sqrt2 * gainV;
                const float mR = std::sin (thetaM) * juce::MathConstants<float>::sqrt2 * gainV;
                // LIFT scales the pan gains toward unity rather than blending
                // wet against dry — in-vs-allpass blending combs at the corner
                // (where the allpass sits at −1·in), and this way lift 0 is the
                // flat allpass, full lift the full tilt, nothing in between dips.
                const float gLp = 1.0f + liftV * (gL - 1.0f), gRp = 1.0f + liftV * (gR - 1.0f);
                const float mLp = 1.0f + liftV * (mL - 1.0f), mRp = 1.0f + liftV * (mR - 1.0f);
                outL = loL * gLp + hiL * mLp;
                outR = loR * gRp + hiR * mRp;
                sBandL = outL; sBandR = outR;     // solo = the tilted split itself
            }
            else
            {
                const float t  = juce::jlimit (0.0f, 1.0f, s2 - (float) zone);
                const float wLo = 1.0f - t, wHi = t;

                const float b4L = b.bell4.processSample (0, inL), b4R = b.bell4.processSample (1, inR);
                float bandL, bandR;
                if (zone == 0)
                {
                    const float b2L = b.bell2.processSample (0, inL), b2R = b.bell2.processSample (1, inR);
                    bandL = wLo * b2L + wHi * b4L;  bandR = wLo * b2R + wHi * b4R;
                }
                else
                {
                    const float b8L = b.bell8.processSample (0, inL), b8R = b.bell8.processSample (1, inR);
                    bandL = wLo * b4L + wHi * b8L;  bandR = wLo * b4R + wHi * b8R;
                }
                // The primary bank already carries the shape's own coefficients
                // (band-pass / low-pass / high-pass). NOTCH is the one shape
                // with no filter of its own — it is what the bell leaves behind.
                if (cfg.shape == ShapeNotch) { bandL = inL - bandL; bandR = inR - bandR; }

                // out = in + band·lift·(g−1): g = 1 (idle) ⇒ out ≡ in, bit-exact
                outL = inL + bandL * liftV * (gL - 1.0f);
                outR = inR + bandR * liftV * (gR - 1.0f);
                // Solo bus: the band's own content, panned — band·lift·g, NOT
                // the displacement band·lift·(g−1) the mix path adds (that
                // inverts and nulls at centre).
                sBandL = bandL * liftV * gL;
                sBandR = bandR * liftV * gR;
            }

            if (cfg.solo)
            {
                soloL += sBandL * e;
                soloR += sBandR * e;
            }

            // Parallel side: add only the pan/gain DISPLACEMENT — overlapping
            // bands SUM their movement instead of the later one re-panning the
            // earlier. Serial side: chain replacement. Both weighted by rx.
            accL += (outL - inL) * e * rx;
            accR += (outR - inR) * e * rx;
            xl = xl + (outL - xl) * e * sxw;
            xr = xr + (outR - xr) * e * sxw;
        }
        // settled: rx=0 → pure serial chain; rx=1 → dry + displacements
        xl = xl * sxw + (xinL + accL) * rx;
        xr = xr * sxw + (xinR + accR) * rx;

        // Solo replaces the mix, crossfaded so arming it is not an edit. The
        // bus is summed unconditionally above but costs nothing when nothing is
        // soloed, and routing does not enter: solo is a monitor tap on the
        // bells themselves, identical in Serial and Parallel.
        {
            const float sv = soloSm.getNextValue();
            if (sv > 0.0f)
            {
                xl += (soloL - xl) * sv;
                xr += (soloR - xr) * sv;
            }
        }

        // scope ring: sample every band's live mod value every 64 samples —
        // captured IN the loop (a post-loop fill repeats the block's final
        // value and draws as a staircase)
        if (--scopePhase <= 0)
        {
            scopePhase = kScopeStride;
            const auto w = scopeWrite.load (std::memory_order_relaxed);
            for (int i = 0; i < kNumBands; ++i)
                scopeRing[(size_t) i][(size_t) (w & (kScopeRingSize - 1))] =
                    mods[(size_t) i].panOut;   // already the final pan (bias + depth·amount)
            envScopeRing[(size_t) (w & (kScopeRingSize - 1))] = env.out;
            scopeWrite.store (w + 1, std::memory_order_release);
        }

        // global wow & flutter (wet-only modulated delay, engage-crossfaded).
        // The line is fed EVERY sample so engaging never reads an empty buffer.
        wf.delay.pushSample (0, xl);
        wf.delay.pushSample (1, xr);
        if (wfActive)
        {
            const float eng = wf.engage.getNextValue();
            // FLUX: jitter the rate and morph the shape toward a random walk,
            // re-dealt once per revolution — no two turns of the capstan alike.
            // At flux = 0 both reduce to exactly the old pure sine.
            wf.wowPhase += wowInc * wf.wowJit;
            if (wf.wowPhase >= 1.0)
            {
                wf.wowPhase -= 1.0;
                wf.wowWalkT = wf.rng.nextDouble() * 2.0 - 1.0;
                wf.wowJit   = 1.0 + fluxAmt * kFluxJitter * (wf.rng.nextDouble() * 2.0 - 1.0);
            }
            wf.flutPhase += flutInc * wf.flutJit;
            if (wf.flutPhase >= 1.0)
            {
                wf.flutPhase -= 1.0;
                wf.flutWalkT = wf.rng.nextDouble() * 2.0 - 1.0;
                wf.flutJit   = 1.0 + fluxAmt * kFluxJitter * (wf.rng.nextDouble() * 2.0 - 1.0);
            }
            wf.wowWalk  += (wf.wowWalkT  - wf.wowWalk)  * wowWalkC;
            wf.flutWalk += (wf.flutWalkT - wf.flutWalk) * flutWalkC;

            const float wowSin  = std::sin ((float) wf.wowPhase  * juce::MathConstants<float>::twoPi);
            const float flutSin = std::sin ((float) wf.flutPhase * juce::MathConstants<float>::twoPi);
            // Crossfade, so |shape| never exceeds the sine's own 1 and the tap
            // headroom (and the reported latency) is unaffected by FLUX.
            const float wowShape  = wowSin  + (float) (wf.wowWalk  - (double) wowSin)  * fluxAmt;
            const float flutShape = flutSin + (float) (wf.flutWalk - (double) flutSin) * fluxAmt;
            const float wowMod  = wowShape  * wf.wowDepthSm.getNextValue();
            const float flutMod = flutShape * wf.flutDepthSm.getNextValue();
            // Engage by GLIDING THE TAP from ~0 up to the base, and take the
            // tap as the output. The old scheme crossfaded dry against the
            // delayed copy, which is a comb: the first null sits at
            // 1/(2*baseDelay) — 124 Hz here — and it swept in during the fade,
            // gutting that band by ~20 dB. That was the engage click (T39).
            // One gliding tap is a single signal path throughout; the only
            // cost is a brief pitch bend, which is what tape does anyway.
            // Slight L/R divergence on wow for width; flutter stays common-mode.
            const float dTargetL = baseDelay + wowMod + flutMod;
            const float dTargetR = baseDelay - wowMod + flutMod;
            wf.delay.setDelay (juce::jlimit (1.0f, 4000.0f, 1.0f + eng * (dTargetL - 1.0f)));
            const float dl = wf.delay.popSample (0);
            wf.delay.setDelay (juce::jlimit (1.0f, 4000.0f, 1.0f + eng * (dTargetR - 1.0f)));
            const float dr = wf.delay.popSample (1);
            xl = dl;
            xr = dr;
        }
        else
        {
            // keep read/write pointers in lockstep while idle — DelayLine's
            // tap desyncs if samples are pushed without matching pops
            wf.delay.popSample (0, baseDelay, true);
            wf.delay.popSample (1, baseDelay, true);
        }

        const float og = outGainSm.getNextValue();
        const float by = bypassSm.getNextValue();

        xl *= og;  xr *= og;

        // crossfaded bypass back to the dry input copy
        left[s]  = xl + (dryL[s] - xl) * by;
        right[s] = xr + (dryR[s] - xr) * by;
    }

    // ── UI telemetry: post-slew mod × depth + cycle phase, once per block ──
    for (int i = 0; i < kNumBands; ++i)
    {
        modOutDepth[(size_t) i].store (mods[(size_t) i].panOut, std::memory_order_relaxed);
        for (int t = 0; t < kNumWaves; ++t)   // mod-matrix sources: every waveform
            modSrcVal[(size_t) (i * kNumWaves + t)].store (mods[(size_t) i].value[t], std::memory_order_relaxed);
        // Frozen band: hold the LAST phase telemetry too. m.phase itself is
        // frozen, but phaseOff re-derives from the live Phase parameter every
        // block — without this gate, host automation of Phase made the Steps
        // highlight sweep while the audio was provably frozen.
        if (! bb[(size_t) i].freeze)
        {
            const double cyc = mods[(size_t) i].phase + (double) bb[(size_t) i].phaseOff;
            modPhase[(size_t) i].store ((float) (cyc - std::floor (cyc)), std::memory_order_relaxed);
        }
    }


    // ── analyzer tap: mono of the processed output ──
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        analyzerFifo.prepareToWrite (numSamples, start1, size1, start2, size2);
        int s2 = 0;
        for (int k = 0; k < size1; ++k, ++s2)
            analyzerStore[(size_t) (start1 + k)] = 0.5f * (left[s2] + right[s2]);
        for (int k = 0; k < size2; ++k, ++s2)
            analyzerStore[(size_t) (start2 + k)] = 0.5f * (left[s2] + right[s2]);
        analyzerFifo.finishedWrite (size1 + size2);
        if (size1 + size2 < numSamples)   // overflow: a gap now exists in the stream
            analyzerDropped.fetch_add (1, std::memory_order_relaxed);
    }

#if ENTROPAN_MOONBASE
    // Licensing gate (RT-safe). While licensed — and during the trial — this is
    // a no-op: one atomic read. Once a trial lapses with no purchase the module
    // periodically silences the FINAL output, so the plugin can be auditioned
    // but not used in production. Applied to the whole block after all DSP.
    // The only earlier return in this function is the zero-length-block case,
    // which has nothing to interrupt — no bypass hole.
    MOONBASE_PROCESS (buffer);
#endif
}

int EntropanAudioProcessor::popAnalyzer (float* dest, int maxNum)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    analyzerFifo.prepareToRead (maxNum, start1, size1, start2, size2);
    int n = 0;
    for (int k = 0; k < size1; ++k)
        dest[n++] = analyzerStore[(size_t) (start1 + k)];
    for (int k = 0; k < size2; ++k)
        dest[n++] = analyzerStore[(size_t) (start2 + k)];
    analyzerFifo.finishedRead (n);
    return n;
}

//==============================================================================
juce::String EntropanAudioProcessor::getStepsJson (int bandIndex) const
{
    return apvts.state.getProperty ("steps_b" + juce::String (bandIndex + 1), juce::String()).toString();
}

void EntropanAudioProcessor::setStepsJson (int bandIndex, const juce::String& json)
{
    apvts.state.setProperty ("steps_b" + juce::String (bandIndex + 1), json, nullptr);
    parseStepsSnapshot (bandIndex);
}

//==============================================================================
juce::String EntropanAudioProcessor::getLocksJson() const
{
    return apvts.state.getProperty ("uiLocks", juce::String()).toString();
}

void EntropanAudioProcessor::setLocksJson (const juce::String& json)
{
    apvts.state.setProperty ("uiLocks", json, nullptr);
}

//==============================================================================
juce::File EntropanAudioProcessor::stepPresetsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Entropia").getChildFile ("Entropan")
               .getChildFile ("step-presets.json");
}

juce::String EntropanAudioProcessor::getStepPresetsJson() const
{
    const auto f = stepPresetsFile();
    return f.existsAsFile() ? f.loadFileAsString() : juce::String ("{}");
}

void EntropanAudioProcessor::setStepPresetsJson (const juce::String& json)
{
    auto f = stepPresetsFile();
    f.getParentDirectory().createDirectory();   // best-effort; ignore failure
    f.replaceWithText (json);
}

juce::File EntropanAudioProcessor::uiSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Entropia").getChildFile ("Entropan")
               .getChildFile ("ui-settings.json");
}

juce::String EntropanAudioProcessor::getUiSettingsJson() const
{
    const auto f = uiSettingsFile();
    return f.existsAsFile() ? f.loadFileAsString() : juce::String ("{}");
}

void EntropanAudioProcessor::setUiSettingsJson (const juce::String& json)
{
    auto f = uiSettingsFile();
    f.getParentDirectory().createDirectory();   // best-effort; ignore failure
    f.replaceWithText (json);
}

//==============================================================================
juce::String EntropanAudioProcessor::getRoutesJson() const
{
    return apvts.state.getProperty ("modRoutes", juce::String()).toString();
}

void EntropanAudioProcessor::setRoutesJson (const juce::String& json)
{
    apvts.state.setProperty ("modRoutes", json, nullptr);
    parseRoutesSnapshot();
}

void EntropanAudioProcessor::parseRoutesSnapshot()
{
    // Message-thread only. Writes the inactive snapshot, then publishes it.
    const auto parsed = juce::JSON::parse (getRoutesJson());
    const int inactive = 1 - routesActive.load();
    auto& rd = routesBuf[(size_t) inactive];
    rd = RoutesData {};

    if (auto* obj = parsed.getDynamicObject())
        if (auto* arr = obj->getProperty ("routes").getArray())
            for (const auto& rv : *arr)
            {
                if (rd.count >= kMaxRoutes) break;
                if (auto* ro = rv.getDynamicObject())
                {
                    auto& rt = rd.routes[rd.count];
                    rt.src   = juce::jlimit (0, kNumBands - 1, (int) ro->getProperty ("src"));
                    // absent (old sessions) → −1 = follow the band's MODE
                    rt.stype = ro->hasProperty ("stype")
                                 ? juce::jlimit (-1, kNumWaves - 1, (int) ro->getProperty ("stype"))
                                 : -1;
                    rt.dst   = (int) ro->getProperty ("dst");
                    rt.depth = juce::jlimit (-100.0f, 100.0f, (float) (double) ro->getProperty ("depth"));
                    // A modulation may not modulate its OWN parameters: all six
                    // waveforms share the band's clock, so band N → band N's
                    // rate/smooth/phase/stepsmooth is self-feedback. The UI
                    // refuses the drop; this also drops any such route arriving
                    // from an old session. (Audio slots + cross-band stay legal.)
                    // Mirrors SELF_MOD_SLOTS in index.html — change both together.
                    static constexpr int kSelfModSlots[] = { 5, 6, 7, 9 };   // rate, smooth, phase, stepsmooth
                    const int slot = rt.dst % kDestSlotsPerBand;
                    const bool selfMod = rt.dst < kNumBands * kDestSlotsPerBand
                                      && rt.dst / kDestSlotsPerBand == rt.src
                                      && std::find (std::begin (kSelfModSlots), std::end (kSelfModSlots), slot)
                                             != std::end (kSelfModSlots);
                    if (rt.dst >= 0 && rt.dst < kNumDests && ! selfMod)
                        ++rd.count;
                }
            }

    routesActive.store (inactive, std::memory_order_release);
}

void EntropanAudioProcessor::parseStepsSnapshot (int bandIndex)
{
    // Message-thread only. Writes the inactive snapshot, then publishes it.
    const auto json = getStepsJson (bandIndex);
    const auto parsed = juce::JSON::parse (json);

    const int inactive = 1 - stepsActive[(size_t) bandIndex].load();
    auto& sd = stepsBuf[(size_t) bandIndex][(size_t) inactive];
    sd = StepsData {};

    if (auto* obj = parsed.getDynamicObject())
    {
        const auto steps = obj->getProperty ("steps");
        if (auto* arr = steps.getArray())
        {
            sd.count = juce::jlimit (0, kMaxSteps, arr->size());
            for (int k = 0; k < sd.count; ++k)
            {
                auto& slot = sd.slots[k];
                if (auto* so = (*arr)[k].getDynamicObject())
                {
                    const int subdiv = (int) so->getProperty ("subdiv");
                    slot.subdiv = (subdiv == 2 || subdiv == 4) ? subdiv : 1;
                    slot.tie = k > 0 && (bool) so->getProperty ("tie");   // cell 0 can never be tied
                    if (auto* vals = so->getProperty ("vals").getArray())
                        for (int v = 0; v < juce::jmin (4, vals->size()); ++v)
                            slot.vals[v] = juce::jlimit (-1.0f, 1.0f, (float) (double) (*vals)[v]);
                }
            }
        }
    }

    // precompute glue runs: a maximal span of [leader, tied, tied, …]
    for (int k = 0; k < sd.count; )
    {
        int j = k + 1;
        while (j < sd.count && sd.slots[j].tie) ++j;   // run = [k, j)
        for (int c = k; c < j; ++c) { sd.runStart[c] = k; sd.runLen[c] = j - k; }
        k = j;
    }

    stepsActive[(size_t) bandIndex].store (inactive, std::memory_order_release);
}

//==============================================================================
void EntropanAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // APVTS state already carries the step-pattern JSON properties.
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EntropanAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
            for (int i = 0; i < kNumBands; ++i)
                parseStepsSnapshot (i);
            parseRoutesSnapshot();
            undoStack.clear();
            redoStack.clear();
            lastCommitted = stateForUndo (apvts.copyState());
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* EntropanAudioProcessor::createEditor()
{
    return new EntropanAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EntropanAudioProcessor();
}
