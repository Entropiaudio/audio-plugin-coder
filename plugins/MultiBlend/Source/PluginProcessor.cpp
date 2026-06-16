#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MultiBlendAudioProcessor::MultiBlendAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "Parameters", createParameterLayout())
{
    // Wire APVTS pointers to bands and slots for lock-free audio-thread access
    for (int b = 0; b < ParamIDs::numBands; ++b)
    {
        auto bandPrefix = "band_" + ParamIDs::bandNames[b] + "_";

        // Band-level params
        bands[(size_t) b].inputGainParam  = apvts.getRawParameterValue (bandPrefix + "input_gain");
        bands[(size_t) b].outputGainParam = apvts.getRawParameterValue (bandPrefix + "output_gain");
        bands[(size_t) b].dryWetParam     = apvts.getRawParameterValue (bandPrefix + "dry_wet");
        bands[(size_t) b].soloParam       = apvts.getRawParameterValue (bandPrefix + "solo");
        bands[(size_t) b].muteParam       = apvts.getRawParameterValue (bandPrefix + "mute");
        bands[(size_t) b].bypassParam     = apvts.getRawParameterValue (bandPrefix + "bypass");
        bands[(size_t) b].msMorphParam    = apvts.getRawParameterValue (bandPrefix + "ms_morph");

        // Slot-level params
        for (int s = 0; s < ParamIDs::slotsPerBand; ++s)
        {
            auto slotPrefix = bandPrefix + "slot_" + juce::String (s + 1) + "_";
            bands[(size_t) b].slots[(size_t) s].dryWetParam = apvts.getRawParameterValue (slotPrefix + "dry_wet");
            bands[(size_t) b].slots[(size_t) s].bypassParam = apvts.getRawParameterValue (slotPrefix + "bypass");
        }
    }

    // Attach gesture watcher to every parameter so non-chaos user input closes
    // any open "Chaos" undo transaction (subsequent user setValue lands in fresh
    // transaction → separate undo step from accumulated CHAOS clicks).
    for (auto* p : getParameters())
        p->addListener (&gestureWatcher);
}

MultiBlendAudioProcessor::GestureWatcher::GestureWatcher (MultiBlendAudioProcessor& p) : owner (p) {}

void MultiBlendAudioProcessor::GestureWatcher::parameterValueChanged (int, float) {}

void MultiBlendAudioProcessor::GestureWatcher::parameterGestureChanged (int, bool gestureIsStarting)
{
    if (! gestureIsStarting) return;
    if (owner.insideChaos.load (std::memory_order_acquire)) return;
    if (owner.undoManager.getCurrentTransactionName() == "Chaos")
        owner.undoManager.beginNewTransaction();
}

MultiBlendAudioProcessor::~MultiBlendAudioProcessor()
{
    for (auto* p : getParameters())
        p->removeListener (&gestureWatcher);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
MultiBlendAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ===== GLOBAL =====
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "global_dry_wet", 1 }, "Global Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "global_input_gain", 1 }, "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "global_output_gain", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "linear_phase", 1 }, "Linear Phase", false));

    // Crossover mode: true = Dynamic Phase (only bands with plugins are filtered;
    // unused spectrum stays bit-transparent), false = Zero Latency (always-on LR).
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "dynamic_phase", 1 }, "Dynamic Phase", true));

    // ===== CROSSOVER =====
    // Log-feel freq range — skew set so 632 Hz (geometric mean of 20 and 20k)
    // lands at normalised 0.5 → matches spectrum's log10 X axis.
    auto makeFreqRange = []
    {
        juce::NormalisableRange<float> r (20.0f, 20000.0f, 0.0f);
        r.setSkewForCentre (632.0f);
        return r;
    };

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "crossover_low_mid", 1 }, "Low-Mid Freq",
        makeFreqRange(), 200.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "crossover_low_mid_slope", 1 }, "Low-Mid Slope",
        juce::NormalisableRange<float> (6.0f, 48.0f, 0.1f), 24.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "crossover_mid_high", 1 }, "Mid-High Freq",
        makeFreqRange(), 3000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "crossover_mid_high_slope", 1 }, "Mid-High Slope",
        juce::NormalisableRange<float> (6.0f, 48.0f, 0.1f), 24.0f));

    // ===== PER-BAND =====
    const juce::StringArray bandLabels { "Low", "Mid", "High" };

    for (int b = 0; b < ParamIDs::numBands; ++b)
    {
        auto prefix = "band_" + ParamIDs::bandNames[b] + "_";
        auto label = bandLabels[b] + " ";

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "dry_wet", 1 }, label + "Dry/Wet",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "input_gain", 1 }, label + "Input Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "output_gain", 1 }, label + "Output Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { prefix + "solo", 1 }, label + "Solo", false));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { prefix + "mute", 1 }, label + "Mute", false));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { prefix + "bypass", 1 }, label + "Bypass", false));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "ms_morph", 1 }, label + "M/S Morph",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

        // ===== PER-SLOT =====
        for (int s = 1; s <= ParamIDs::slotsPerBand; ++s)
        {
            auto slotPrefix = prefix + "slot_" + juce::String (s) + "_";
            auto slotLabel = label + "Slot " + juce::String (s) + " ";

            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { slotPrefix + "dry_wet", 1 }, slotLabel + "Dry/Wet",
                juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

            params.push_back (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { slotPrefix + "bypass", 1 }, slotLabel + "Bypass", false));
        }
    }

    return { params.begin(), params.end() };
}

//==============================================================================
void MultiBlendAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    // Global gain smoothing (20ms ramp)
    globalInputGain.reset (sampleRate, 0.02);
    globalOutputGain.reset (sampleRate, 0.02);
    globalDryWet.reset (sampleRate, 0.02);

    // Allocate dry buffer
    dryBuffer.setSize (2, samplesPerBlock);
    for (auto& r : rawRegion) r.setSize (2, samplesPerBlock);

    // Dynamic-phase clean-path delay lines
    juce::dsp::ProcessSpec dynSpec;
    dynSpec.sampleRate = sampleRate;
    dynSpec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    dynSpec.numChannels = 2;
    dynInputDelay.prepare (dynSpec);
    dynInputDelay.setMaximumDelayInSamples (maxDynDelay);
    for (auto& d : dynRawDelay) { d.prepare (dynSpec); d.setMaximumDelayInSamples (maxDynDelay); }
    lastDynDelay = -1;

    // Prepare crossover and band processors
    crossover.prepare (sampleRate, samplesPerBlock);

    for (auto& band : bands)
        band.prepare (sampleRate, samplesPerBlock);

    // Initial latency report (in case hosts re-prepared with loaded session)
    updateLatency();
}

void MultiBlendAudioProcessor::releaseResources()
{
    crossover.reset();

    for (auto& band : bands)
        band.releaseResources();
}

//==============================================================================
// Latency compensation — sum slot latencies per band, align with delay lines,
// report total (max-band + crossover) to host.
//==============================================================================
void MultiBlendAudioProcessor::updateLatency()
{
    int bandLat[ParamIDs::numBands] = { 0, 0, 0 };
    int maxBand = 0;
    for (int b = 0; b < ParamIDs::numBands; ++b)
    {
        bandLat[b] = bands[(size_t) b].computeLatencySamples();
        if (bandLat[b] > maxBand) maxBand = bandLat[b];
    }

    for (int b = 0; b < ParamIDs::numBands; ++b)
        bands[(size_t) b].setAlignmentDelay (maxBand - bandLat[b]);

    // Crossover latency: IIR = 0. Linear-phase FIR = kFirTaps/2 (= 1024 @ 2048 taps).
    const int crossoverLat = crossover.getLinearPhaseLatencySamples();
    const int totalLat = maxBand + crossoverLat;

    if (totalLat != getLatencySamples())
        setLatencySamples (totalLat);
}

//==============================================================================
// UndoableAction: apply a list of slot-to-slot moves. undo() reverses direction.
//==============================================================================
class SlotMoveAction : public juce::UndoableAction
{
public:
    struct Move { int sb, ss, db, ds; };

    SlotMoveAction (MultiBlendAudioProcessor& p, std::vector<Move> m)
        : processor (p), moves (std::move (m)) {}

    bool perform() override { applyMoves (moves); return true; }
    bool undo()    override
    {
        std::vector<Move> rev;
        rev.reserve (moves.size());
        for (auto it = moves.rbegin(); it != moves.rend(); ++it)
            rev.push_back ({ it->db, it->ds, it->sb, it->ss });
        applyMoves (rev);
        return true;
    }
    int getSizeInUnits() override { return (int) (moves.size() * sizeof (Move)); }

private:
    void applyMoves (const std::vector<Move>& list)
    {
        struct Pending { juce::AudioPluginInstance* ptr; juce::PluginDescription desc; int db, ds; };
        std::vector<Pending> pending;
        pending.reserve (list.size());

        // Detach all sources first so a destination that was a source is empty by attach phase
        for (auto& m : list)
        {
            auto& src = processor.getSlot (m.sb, m.ss);
            pending.push_back ({ src.detachPlugin(), src.detachDescription(), m.db, m.ds });
        }
        for (auto& p : pending)
        {
            if (p.ptr == nullptr) continue;
            processor.getSlot (p.db, p.ds).attachPlugin (p.ptr, p.desc);
        }
        processor.updateLatency();
    }

    MultiBlendAudioProcessor& processor;
    std::vector<Move> moves;
};

//==============================================================================
// Capture / restore a slot's plugin (description + state) for undoable load/unload.
//==============================================================================
MultiBlendAudioProcessor::SlotPluginState
MultiBlendAudioProcessor::captureSlot (int band, int slot)
{
    SlotPluginState s;
    auto* plugin = getSlot (band, slot).getPluginInstance();
    if (plugin != nullptr)
    {
        s.loaded = true;
        s.desc = plugin->getPluginDescription();
        plugin->getStateInformation (s.state);
    }
    return s;
}

void MultiBlendAudioProcessor::restoreSlot (int band, int slot, const SlotPluginState& s)
{
    if (! s.loaded)
    {
        getSlot (band, slot).unloadPlugin();
        updateLatency();
        return;
    }

    const double sr = getSampleRate();
    const int    bs = getBlockSize();
    auto stateCopy = s.state;
    hostManager.createPluginAsync (s.desc, sr, bs,
        [this, band, slot, stateCopy] (std::unique_ptr<juce::AudioPluginInstance> inst,
                                       const juce::String& err)
        {
            if (inst == nullptr) { DBG ("restoreSlot failed: " + err); return; }
            if (stateCopy.getSize() > 0)
                inst->setStateInformation (stateCopy.getData(), (int) stateCopy.getSize());
            getSlot (band, slot).loadPlugin (std::move (inst));
            updateLatency();
        });
}

namespace
{
    class SlotPluginAction : public juce::UndoableAction
    {
    public:
        SlotPluginAction (MultiBlendAudioProcessor& p, int band, int slot,
                          MultiBlendAudioProcessor::SlotPluginState before,
                          MultiBlendAudioProcessor::SlotPluginState after)
            : proc (p), b (band), s (slot),
              stBefore (std::move (before)), stAfter (std::move (after)) {}

        bool perform() override { proc.restoreSlot (b, s, stAfter);  return true; }
        bool undo()    override { proc.restoreSlot (b, s, stBefore); return true; }
        int getSizeInUnits() override
        {
            return (int) (stBefore.state.getSize() + stAfter.state.getSize() + 256);
        }
    private:
        MultiBlendAudioProcessor& proc;
        int b, s;
        MultiBlendAudioProcessor::SlotPluginState stBefore, stAfter;
    };
}

void MultiBlendAudioProcessor::pushSlotPluginAction (int band, int slot,
                                                     SlotPluginState before, SlotPluginState after,
                                                     const juce::String& name)
{
    closeChaosIfOpen();
    undoManager.beginNewTransaction (name);
    undoManager.perform (new SlotPluginAction (*this, band, slot, std::move (before), std::move (after)));
}

//==============================================================================
// Chaos plugin shuffle — atomic permutation of LOADED plugins across UNLOCKED slots.
//==============================================================================
void MultiBlendAudioProcessor::shufflePlugins (const std::set<int>& lockedCoords)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    struct SlotRef { int band; int slot; int key; };
    std::vector<SlotRef> unlocked;
    std::vector<SlotRef> loadedAndUnlocked;

    for (int b = 0; b < ParamIDs::numBands; ++b)
    {
        for (int s = 0; s < ParamIDs::slotsPerBand; ++s)
        {
            int key = b * ParamIDs::slotsPerBand + s;
            if (lockedCoords.count (key) > 0) continue;
            unlocked.push_back ({ b, s, key });
            if (bands[(size_t) b].slots[(size_t) s].isLoaded())
                loadedAndUnlocked.push_back ({ b, s, key });
        }
    }

    if (loadedAndUnlocked.size() < 2) return;

    // Fisher-Yates shuffle the unlocked destinations
    juce::Random rng (juce::Random::getSystemRandom().nextInt64());
    std::vector<int> destIdx (unlocked.size());
    for (size_t i = 0; i < destIdx.size(); ++i) destIdx[i] = (int) i;
    for (int i = (int) destIdx.size() - 1; i > 0; --i)
    {
        int j = rng.nextInt (i + 1);
        std::swap (destIdx[(size_t) i], destIdx[(size_t) j]);
    }

    // First N destinations (one per loaded plugin)
    std::vector<SlotRef> dests;
    for (size_t i = 0; i < loadedAndUnlocked.size(); ++i)
        dests.push_back (unlocked[(size_t) destIdx[i]]);

    // Build list of moves (src → dst) and push as a single undoable action.
    // No-op moves (src == dst) excluded; reduces undo noise.
    std::vector<SlotMoveAction::Move> moveList;
    moveList.reserve (loadedAndUnlocked.size());
    for (size_t i = 0; i < loadedAndUnlocked.size(); ++i)
    {
        const auto& src = loadedAndUnlocked[i];
        const auto& dst = dests[i];
        if (src.key == dst.key) continue;
        moveList.push_back ({ src.band, src.slot, dst.band, dst.slot });
    }
    if (moveList.empty()) return;

    undoManager.perform (new SlotMoveAction (*this, std::move (moveList)));
}

//==============================================================================
// Single-shot CHAOS: open transaction → randomize knobs → shuffle → close.
// Everything collapses into ONE undo step.
//==============================================================================
void MultiBlendAudioProcessor::triggerChaos (const std::set<int>& lockedSlotCoords,
                                             const std::set<juce::String>& lockedParamIds)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Extend any open "Chaos" transaction so multiple successive CHAOS clicks
    // (including auto-timer fires) collapse into ONE undo step.
    if (undoManager.getCurrentTransactionName() != "Chaos")
        undoManager.beginNewTransaction ("Chaos");

    juce::ignoreUnused (lockedParamIds);   // CHAOS no longer touches knob params

    // Flag set during shuffle so the gesture-listener (which fires on any
    // setValue) doesn't close our transaction. Shuffle doesn't touch params,
    // but flag kept for safety / future expansion.
    insideChaos.store (true, std::memory_order_release);
    struct Reset { std::atomic<bool>& f; ~Reset() { f.store (false, std::memory_order_release); } } _r { insideChaos };

    // CHAOS = plugin-insert shuffle only. Band knobs (In/Out/D-W/M-S) untouched.
    shufflePlugins (lockedSlotCoords);

    // Do NOT close transaction — successive CHAOS clicks extend same undo step.
}

bool MultiBlendAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void MultiBlendAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    // Clear unused output channels
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    // 1. Capture dry signal (before any processing)
    dryBuffer.makeCopyOf (buffer, true);

    // Push input samples to FFT FIFO (input layer of spectrum analyzer)
    pushSamplesToFFT (buffer, FFTChannel::Input);

    // 2. Apply global input gain
    float inputGainDb = apvts.getRawParameterValue ("global_input_gain")->load();
    globalInputGain.setTargetValue (juce::Decibels::decibelsToGain (inputGainDb));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= globalInputGain.getNextValue();
    }

    // 3. Read crossover frequencies and band bypass states
    float lowMidFreq  = apvts.getRawParameterValue ("crossover_low_mid")->load();
    float midHighFreq = apvts.getRawParameterValue ("crossover_mid_high")->load();

    bool lowBypassed  = bands[0].bypassParam && bands[0].bypassParam->load() > 0.5f;
    bool midBypassed  = bands[1].bypassParam && bands[1].bypassParam->load() > 0.5f;
    bool highBypassed = bands[2].bypassParam && bands[2].bypassParam->load() > 0.5f;

    // Per-crossover slope → fractional cascade order (6 dB/oct per stage).
    // slope/6 gives a continuous order; fractional part crossfades a stage → 1 dB res.
    float lowMidSlope  = apvts.getRawParameterValue ("crossover_low_mid_slope")->load();
    float midHighSlope = apvts.getRawParameterValue ("crossover_mid_high_slope")->load();
    float lowMidPasses  = juce::jlimit (1.0f, 8.0f, lowMidSlope  / 6.0f);
    float midHighPasses = juce::jlimit (1.0f, 8.0f, midHighSlope / 6.0f);

    // Linear-phase toggle — switches crossover to FIR convolution path
    bool linPhase = apvts.getRawParameterValue ("linear_phase")->load() > 0.5f;
    if (linPhase != crossover.isLinearPhase())
    {
        crossover.setLinearPhase (linPhase);
        juce::MessageManager::callAsync ([this]() { updateLatency(); });
    }

    bool dynamicPhase = apvts.getRawParameterValue ("dynamic_phase")->load() > 0.5f
                        && ! linPhase;   // linear-phase mode supersedes (always splits)

    // Read solo/mute states
    bool lowSolo  = bands[0].soloParam && bands[0].soloParam->load() > 0.5f;
    bool midSolo  = bands[1].soloParam && bands[1].soloParam->load() > 0.5f;
    bool highSolo = bands[2].soloParam && bands[2].soloParam->load() > 0.5f;
    bool lowMute  = bands[0].muteParam && bands[0].muteParam->load() > 0.5f;
    bool midMute  = bands[1].muteParam && bands[1].muteParam->load() > 0.5f;
    bool highMute = bands[2].muteParam && bands[2].muteParam->load() > 0.5f;
    const bool soloMute[3]   = { lowSolo, midSolo, highSolo };
    const bool muteState[3]  = { lowMute, midMute, highMute };
    const bool anySolo = lowSolo || midSolo || highSolo;

    // 4. Split into 3 frequency bands (LR or FIR regions)
    crossover.split (buffer, lowMidFreq, midHighFreq,
                     lowBypassed, midBypassed, highBypassed,
                     lowMidPasses, midHighPasses);

    if (dynamicPhase && ! anySolo)
    {
        // ─── DYNAMIC PHASE ─── process-the-difference.
        // out = delayed(input) − Σ delayed(raw touched) + Σ processed(active).
        // Untouched bands stay as the (delayed) input → bit-transparent, zero phase.
        // Processed bands emerge at the reported latency L (plugin lag + band
        // alignment), so delay input + raw snapshots by L to keep phase coherent.

        // Match clean-path delay to reported latency
        int L = juce::jlimit (0, maxDynDelay, getLatencySamples());
        if (L != lastDynDelay)
        {
            dynInputDelay.setDelay ((float) L);
            for (auto& d : dynRawDelay) d.setDelay ((float) L);
            lastDynDelay = L;
        }

        bool touched[3];
        for (int b = 0; b < 3; ++b)
        {
            touched[b] = bands[(size_t) b].hasActivePlugin() || muteState[b];
            // Snapshot ALL raw regions (keeps every delay line warm even when unused)
            rawRegion[(size_t) b].makeCopyOf (crossover.getBandBuffer (b), true);
        }

        // Process active bands in place (emerge at latency L)
        for (int b = 0; b < 3; ++b)
            if (bands[(size_t) b].hasActivePlugin())
                bands[(size_t) b].processBlock (crossover.getBandBuffer (b), midiMessages);

        // Delay clean input + all raw snapshots by L
        { juce::dsp::AudioBlock<float> blk (buffer);
          juce::dsp::ProcessContextReplacing<float> ctx (blk); dynInputDelay.process (ctx); }
        for (int b = 0; b < 3; ++b)
        { juce::dsp::AudioBlock<float> blk (rawRegion[(size_t) b]);
          juce::dsp::ProcessContextReplacing<float> ctx (blk); dynRawDelay[(size_t) b].process (ctx); }

        // Combine
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* out = buffer.getWritePointer (ch);
            for (int b = 0; b < 3; ++b)
            {
                if (! touched[b]) continue;
                const auto* raw = rawRegion[(size_t) b].getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i) out[i] -= raw[i];   // remove raw
                if (! muteState[b])
                {
                    const auto* proc = crossover.getBandBuffer (b).getReadPointer (ch);
                    for (int i = 0; i < numSamples; ++i) out[i] += proc[i]; // add processed
                }
            }
        }
    }
    else
    {
        // ─── ZERO LATENCY / SOLO / LINEAR PHASE ─── classic full 3-band sum.
        for (int b = 0; b < ParamIDs::numBands; ++b)
            bands[(size_t) b].processBlock (crossover.getBandBuffer (b), midiMessages);

        crossover.sum (buffer, lowSolo, midSolo, highSolo,
                       lowMute, midMute, highMute,
                       lowBypassed, midBypassed, highBypassed);
        juce::ignoreUnused (soloMute);
    }

    // 4. Apply global output gain
    float outputGainDb = apvts.getRawParameterValue ("global_output_gain")->load();
    globalOutputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainDb));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= globalOutputGain.getNextValue();
    }

    // 5. Apply global dry/wet
    float dryWetPct = apvts.getRawParameterValue ("global_dry_wet")->load();
    globalDryWet.setTargetValue (dryWetPct / 100.0f);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float w = globalDryWet.getNextValue();
            wet[i] = dry[i] * (1.0f - w) + wet[i] * w;
        }
    }

    // Push final output to FFT FIFO (output layer of spectrum analyzer)
    pushSamplesToFFT (buffer, FFTChannel::Output);
}

//==============================================================================
juce::AudioProcessorEditor* MultiBlendAudioProcessor::createEditor()
{
    return new MultiBlendAudioProcessorEditor (*this);
}

bool MultiBlendAudioProcessor::hasEditor() const { return true; }
const juce::String MultiBlendAudioProcessor::getName() const { return JucePlugin_Name; }
bool MultiBlendAudioProcessor::acceptsMidi() const { return false; }
bool MultiBlendAudioProcessor::producesMidi() const { return false; }
bool MultiBlendAudioProcessor::isMidiEffect() const { return false; }
double MultiBlendAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int MultiBlendAudioProcessor::getNumPrograms() { return 1; }
int MultiBlendAudioProcessor::getCurrentProgram() { return 0; }
void MultiBlendAudioProcessor::setCurrentProgram (int) {}
const juce::String MultiBlendAudioProcessor::getProgramName (int) { return {}; }
void MultiBlendAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void MultiBlendAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("MultiBlendState");

    // Save APVTS parameters
    auto state = apvts.copyState();
    xml->addChildElement (state.createXml().release());

    // Save hosted plugin states
    auto hostedXml = std::make_unique<juce::XmlElement> ("HostedPlugins");
    for (int b = 0; b < ParamIDs::numBands; ++b)
        for (int s = 0; s < ParamIDs::slotsPerBand; ++s)
            bands[(size_t) b].slots[(size_t) s].saveState (*hostedXml, b, s);
    xml->addChildElement (hostedXml.release());

    copyXmlToBinary (*xml, destData);
}

void MultiBlendAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml != nullptr && xml->hasTagName ("MultiBlendState"))
    {
        // Restore APVTS parameters
        if (auto* paramsXml = xml->getChildByName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*paramsXml));

        // Restore hosted plugin states
        if (auto* hostedXml = xml->getChildByName ("HostedPlugins"))
            for (int b = 0; b < ParamIDs::numBands; ++b)
                for (int s = 0; s < ParamIDs::slotsPerBand; ++s)
                    bands[(size_t) b].slots[(size_t) s].restoreState (*hostedXml, b, s, hostManager);
    }
}

//==============================================================================
// Spectrum FFT — audio thread pushes to ring buffers; UI thread reads.
void MultiBlendAudioProcessor::pushSamplesToFFT (const juce::AudioBuffer<float>& buffer,
                                                   FFTChannel which)
{
    auto& fifo = (which == FFTChannel::Input) ? fifoInput : fifoOutput;
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numCh == 0) return;

    // Sliding ring buffer: always retain the most-recent fftSize samples.
    int idx = fifo.idx.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            s += buffer.getSample (ch, i);
        s /= (float) numCh;

        fifo.samples[(size_t) idx] = s;
        idx = (idx + 1) & (fftSize - 1);
    }
    fifo.idx.store (idx, std::memory_order_relaxed);
    fifo.ready.store (true, std::memory_order_release);   // data always fresh
}

bool MultiBlendAudioProcessor::getNextFFTBlock (float* outMagnitudes, int numBins,
                                                  FFTChannel which)
{
    auto& fifo = (which == FFTChannel::Input) ? fifoInput : fifoOutput;
    if (! fifo.ready.load (std::memory_order_acquire))
        return false;

    // Copy the latest fftSize samples in time order (oldest→newest) starting
    // just after the write head. Overlapping windows → smooth ~30 Hz updates.
    const int writeIdx = fifo.idx.load (std::memory_order_acquire);
    std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
    {
        const int r = (writeIdx + i) & (fftSize - 1);
        fftBuffer[(size_t) i] = fifo.samples[(size_t) r];
    }

    spectrumWindow.multiplyWithWindowingTable (fftBuffer.data(), (size_t) fftSize);
    spectrumFFT.performFrequencyOnlyForwardTransform (fftBuffer.data());

    const int half = fftSize / 2;
    const int out = juce::jmin (numBins, half);
    for (int i = 0; i < out; ++i)
    {
        float mag = fftBuffer[(size_t) i] / (float) fftSize;
        float db = juce::Decibels::gainToDecibels (mag + 1.0e-9f, -100.0f);
        outMagnitudes[i] = juce::jlimit (0.0f, 1.0f, (db + 100.0f) / 100.0f);
    }
    return true;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiBlendAudioProcessor();
}
