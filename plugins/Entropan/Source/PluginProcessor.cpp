#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    const juce::StringArray kModeChoices    { "Sine", "Triangle", "S&H", "Drift", "Chaos", "Steps" };
    const juce::StringArray kRateModeChoices{ "Sync", "Free", "MIDI" };
    const juce::StringArray kDivChoices     { "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bar", "4 Bar" };
    const juce::StringArray kSpeedChoices   { "/4", "/2", "x1", "x2", "x3", "x4" };

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

    inline float smoothstep01 (float t) { return t * t * (3.0f - 2.0f * t); }

    // Editor window size lives in the same tree for persistence but must not
    // take part in undo (a resize would otherwise create/restore undo steps).
    juce::ValueTree stateForUndo (const juce::ValueTree& src)
    {
        auto v = src.createCopy();
        v.removeProperty ("editorWidth", nullptr);
        v.removeProperty ("editorHeight", nullptr);
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

bool EntropanAudioProcessor::undoState()
{
    if (undoStack.empty())
        return false;
    redoStack.push_back (stateForUndo (apvts.copyState()));
    auto snap = undoStack.back();
    undoStack.pop_back();
    // keep the live window size — snapshots carry no editor props
    snap.setProperty ("editorWidth",  apvts.state.getProperty ("editorWidth",  900), nullptr);
    snap.setProperty ("editorHeight", apvts.state.getProperty ("editorHeight", 560), nullptr);
    apvts.replaceState (snap);
    lastCommitted = stateForUndo (apvts.copyState());
    for (int i = 0; i < kNumBands; ++i)
        parseStepsSnapshot (i);
    return true;
}

bool EntropanAudioProcessor::redoState()
{
    if (redoStack.empty())
        return false;
    undoStack.push_back (stateForUndo (apvts.copyState()));
    auto snap = redoStack.back();
    redoStack.pop_back();
    snap.setProperty ("editorWidth",  apvts.state.getProperty ("editorWidth",  900), nullptr);
    snap.setProperty ("editorHeight", apvts.state.getProperty ("editorHeight", 560), nullptr);
    apvts.replaceState (snap);
    lastCommitted = stateForUndo (apvts.copyState());
    for (int i = 0; i < kNumBands; ++i)
        parseStepsSnapshot (i);
    return true;
}

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
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Level",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    // ─── Per band (12 × 6 = 72) ───
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
            juce::ParameterID { p + "depth", 1 }, label + "Depth", pct(), 50.0f,
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
    }

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

    dryBuffer.setSize (2, juce::jmax (samplesPerBlock * 2, 8192));   // headroom for hosts that exceed the prepared block
    analyzerStore.resize ((size_t) analyzerFifo.getTotalSize(), 0.0f);
    scopePhase = kScopeStride;

    for (auto& m : mods)
        m = Modulator {};

    // Cache raw parameter pointers once (RT-safe reads afterwards).
    pAmount = apvts.getRawParameterValue ("amount");
    pOutput = apvts.getRawParameterValue ("output");
    pBypass = apvts.getRawParameterValue ("bypass");
    pSeed   = apvts.getRawParameterValue ("seed");
    pSpeed  = apvts.getRawParameterValue ("speed");
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
            apvts.getRawParameterValue (p + "phase")
        };
    }
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
        ++rerollOffset;   // shifts every random stream (S&H / Drift)
        for (int i = 0; i < kNumBands; ++i)   // kick the Lorenz attractors
        {
            auto& m = mods[(size_t) i];
            m.lx = 0.1 + 0.2 * (double) cellNoise (rerollOffset, i, (int) pSeed->load(), 0);
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
    const int   seedV    = (int) pSeed->load();
    const float speedV   = kSpeedFactors[juce::jlimit (0, 5, (int) pSpeed->load())];
    amountSm.setTargetValue (pAmount->load() * 0.01f);

    struct BandBlock   // per-block modulator config, gathered outside the sample loop
    {
        int    mode = 0, rateMode = 0;
        double cycPerSample = 0.0;    // free/MIDI: cycle increment per sample
        double quartersPerCycle = 1.0;// sync: musical cycle length
        float  phaseOff = 0.0f;
        float  lorenzDt = 0.0f;
    };
    std::array<BandBlock, kNumBands> bb;

    for (int i = 0; i < kNumBands; ++i)
    {
        auto& b  = bands[(size_t) i];
        auto& m  = mods[(size_t) i];
        auto& pp = bandParams[(size_t) i];
        auto& cfg = bb[(size_t) i];

        const bool on = pp.on->load() > 0.5f;
        b.enable.setTargetValue (on ? 1.0f : 0.0f);

        // Band edges from centre + width (octaves), clamped into the audible
        // range with a guaranteed f_lo < f_hi ordering.
        const float freq  = pp.freq->load();
        const float width = pp.width->load();
        const float fLoT = juce::jlimit (20.0f, 20000.0f, freq * std::pow (2.0f, -width * 0.5f));
        const float fHiT = juce::jlimit (fLoT * 1.02f, 20500.0f, freq * std::pow (2.0f,  width * 0.5f));
        if (! b.cutoffsInit) { b.fLoCur = fLoT; b.fHiCur = fHiT; b.cutoffsInit = true; }
        // block-rate glide (~40 ms) — no zipper while dragging freq/Q
        const float ck = juce::jmin (1.0f, (float) numSamples / (0.040f * (float) currentSampleRate));
        b.fLoCur += (fLoT - b.fLoCur) * ck;
        b.fHiCur += (fHiT - b.fHiCur) * ck;
        b.splitLo.setCutoffFrequency (b.fLoCur);
        b.splitHi.setCutoffFrequency (b.fHiCur);
        b.apLow.setCutoffFrequency  (b.fHiCur);

        b.lift.setTargetValue (pp.lift->load() * 0.01f);
        b.gainLin.setTargetValue (juce::Decibels::decibelsToGain (pp.gain->load()));

        // ── modulator config ──
        cfg.mode     = (int) pp.mode->load();
        cfg.rateMode = (int) pp.ratemode->load();
        cfg.phaseOff = pp.phase->load() / 360.0f;
        b.depth.setTargetValue (pp.depth->load() * 0.01f);

        double rateHz = 0.0;
        if (cfg.rateMode == 0)        // sync
        {
            cfg.quartersPerCycle = kDivQuarters[juce::jlimit (0, 6, (int) pp.div->load())] / speedV;
            rateHz = (bpm / 60.0) / cfg.quartersPerCycle;
        }
        else if (cfg.rateMode == 1)   // free (up to audio rate)
        {
            rateHz = (double) pp.rate->load() * speedV;
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
        //   Drift/Chaos— free viscosity (unbounded wanderers by nature)
        const float  inertia = pp.inertia->load() * 0.01f;
        const double i2 = (double) inertia * inertia;
        const double periodS = cfg.cycPerSample > 1.0e-9
                                 ? 1.0 / (cfg.cycPerSample * currentSampleRate) : 1.0e9;
        double slewT;
        if (cfg.mode == 2 || cfg.mode == 5)          // S&H / Steps
            slewT = juce::jmin (0.004 + i2 * 2.0, periodS / 3.5);
        else if (cfg.mode == 3 || cfg.mode == 4)     // Drift / Chaos
            slewT = juce::jmin (2.0, 0.004 + i2 * 2.0);
        else                                          // Sine / Tri
            slewT = i2 * 0.10 * periodS;              // corner ≈ 1.6·rate at max
        m.slewCoeff = slewT < 5.0e-4 ? 1.0f
                    : 1.0f - std::exp (-1.0f / ((float) slewT * (float) currentSampleRate));
    }

    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    // ── per-sample cascade ──
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int s = 0; s < numSamples; ++s)
    {
        float xl = left[s], xr = right[s];
        const float amountV = amountSm.getNextValue();

        for (int i = 0; i < kNumBands; ++i)
        {
            auto& b   = bands[(size_t) i];
            auto& m   = mods[(size_t) i];
            auto& cfg = bb[(size_t) i];

            const float e      = b.enable.getNextValue();
            const float liftV  = b.lift.getNextValue();
            const float gainV  = b.gainLin.getNextValue();
            const float depthV = b.depth.getNextValue();   // advance even when
                                                           // disabled — no lag on re-enable

            if (e < 1.0e-4f)
                continue;   // fully disengaged: stage is a wire (filters stay cold)

            // ── modulator tick (per sample) ──
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
            const auto   cell = (juce::int64) std::floor (cfg.mode == 3 ? cyc : m.phase); // drift interpolates on offset cycle

            switch (cfg.mode)
            {
                case 0: m.target = std::sin ((float) frac * juce::MathConstants<float>::twoPi); break;
                case 1: m.target = 1.0f - 4.0f * std::abs ((float) frac - 0.5f); break;
                case 2: // S&H: new deal each cycle boundary (phase-offset agnostic)
                    m.target = cellNoise ((juce::int64) std::floor (m.phase), i, seedV, rerollOffset);
                    break;
                case 3: // Drift: value-noise between cell endpoints
                {
                    const float a = cellNoise (cell,     i, seedV, rerollOffset);
                    const float c = cellNoise (cell + 1, i, seedV, rerollOffset);
                    m.target = a + (c - a) * smoothstep01 ((float) frac);
                    break;
                }
                case 4: // Chaos: Lorenz, x-component normalised
                {
                    const float dt = cfg.lorenzDt;
                    const double nx = m.lx + 10.0 * (m.ly - m.lx) * dt;
                    const double ny = m.ly + (m.lx * (28.0 - m.lz) - m.ly) * dt;
                    const double nz = m.lz + (m.lx * m.ly - (8.0 / 3.0) * m.lz) * dt;
                    m.lx = nx; m.ly = ny; m.lz = nz;
                    if (! std::isfinite (m.lx)) { m.lx = 0.1; m.ly = 0.0; m.lz = 0.0; }
                    m.target = juce::jlimit (-1.0f, 1.0f, (float) (m.lx / 18.0));
                    break;
                }
                case 5: // Steps: slot-stable ratchets (RT snapshot)
                {
                    const auto& sd = stepsBuf[(size_t) i][(size_t) stepsActive[(size_t) i].load (std::memory_order_acquire)];
                    if (sd.count > 0)
                    {
                        double sp = frac * (double) sd.count;
                        int slot = juce::jlimit (0, sd.count - 1, (int) sp);
                        const auto& sl = sd.slots[slot];
                        const int sub = juce::jlimit (0, sl.subdiv - 1, (int) ((sp - (double) slot) * (double) sl.subdiv));
                        m.target = sl.vals[sub];
                    }
                    else
                        m.target = 0.0f;
                    break;
                }
                default:
                    m.target = 0.0f;
                    break;
            }

            // inertia slew (per-mode policy), then depth · master amount
            m.value += (m.target - m.value) * m.slewCoeff;
            const float panV = juce::jlimit (-1.0f, 1.0f, m.value * depthV * amountV);

            // 3-way split (processSample yields the complementary LP/HP pair)
            float lowL = 0, restL = 0, lowR = 0, restR = 0;
            b.splitLo.processSample (0, xl, lowL, restL);
            b.splitLo.processSample (1, xr, lowR, restR);

            float bandL = 0, highL = 0, bandR = 0, highR = 0;
            b.splitHi.processSample (0, restL, bandL, highL);
            b.splitHi.processSample (1, restR, bandR, highR);

            // allpass-match the low branch at f_hi (LR property: LP+HP = AP)
            float apLoL = 0, apHiL = 0, apLoR = 0, apHiR = 0;
            b.apLow.processSample (0, lowL, apLoL, apHiL);
            b.apLow.processSample (1, lowR, apLoR, apHiR);
            const float lowApL = apLoL + apHiL;
            const float lowApR = apLoR + apHiR;

            // lift split + equal-power pan (balance law, ×√2 so centre = unity)
            const float theta = (panV + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float gL = std::cos (theta) * juce::MathConstants<float>::sqrt2 * gainV;
            const float gR = std::sin (theta) * juce::MathConstants<float>::sqrt2 * gainV;

            const float outL = (lowApL + highL) + bandL * (1.0f - liftV) + bandL * liftV * gL;
            const float outR = (lowApR + highR) + bandR * (1.0f - liftV) + bandR * liftV * gR;

            // engage crossfade between wire and processed stage
            xl = xl + (outL - xl) * e;
            xr = xr + (outR - xr) * e;
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
                    mods[(size_t) i].value * bands[(size_t) i].depth.getCurrentValue();
            scopeWrite.store (w + 1, std::memory_order_release);
        }

        const float og = outGainSm.getNextValue();
        const float by = bypassSm.getNextValue();

        xl *= og;  xr *= og;

        // crossfaded bypass back to the dry input copy
        left[s]  = xl + (dryBuffer.getSample (0, s) - xl) * by;
        right[s] = xr + (dryBuffer.getSample (1, s) - xr) * by;
    }

    // ── UI telemetry: post-slew mod × depth + cycle phase, once per block ──
    for (int i = 0; i < kNumBands; ++i)
    {
        modOutDepth[(size_t) i].store (mods[(size_t) i].value * bands[(size_t) i].depth.getCurrentValue(),
                                       std::memory_order_relaxed);
        const double cyc = mods[(size_t) i].phase + (double) bb[(size_t) i].phaseOff;
        modPhase[(size_t) i].store ((float) (cyc - std::floor (cyc)), std::memory_order_relaxed);
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
    }
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
                    if (auto* vals = so->getProperty ("vals").getArray())
                        for (int v = 0; v < juce::jmin (4, vals->size()); ++v)
                            slot.vals[v] = juce::jlimit (-1.0f, 1.0f, (float) (double) (*vals)[v]);
                }
            }
        }
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
