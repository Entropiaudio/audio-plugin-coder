#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    // Log-shaped range: value = min * (max/min)^norm — must mirror the maps in
    // ui/public/index.html (MAPS.log) so UI and host agree on every position.
    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        juce::NormalisableRange<float> r (lo, hi,
            [lo, hi] (float, float, float norm)  { return lo * std::pow (hi / lo, norm); },
            [lo, hi] (float, float, float value) { return std::log (value / lo) / std::log (hi / lo); });
        return r;
    }

    const juce::StringArray kModeChoices    { "Sine", "Triangle", "S&H", "Drift", "Chaos", "Steps" };
    const juce::StringArray kRateModeChoices{ "Sync", "Free", "MIDI" };
    const juce::StringArray kDivChoices     { "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bar", "4 Bar" };
    const juce::StringArray kSpeedChoices   { "/4", "/2", "x1", "x2", "x3", "x4" };

    constexpr float kBandFreqDefaults[] = { 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
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
    }
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
            juce::ParameterID { p + "on", 1 }, label + "Enable", i == 0));
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

    dryBuffer.setSize (2, samplesPerBlock);

    // Cache raw parameter pointers once (RT-safe reads afterwards).
    pAmount = apvts.getRawParameterValue ("amount");
    pOutput = apvts.getRawParameterValue ("output");
    pBypass = apvts.getRawParameterValue ("bypass");
    for (int i = 0; i < kNumBands; ++i)
    {
        const juce::String p = "b" + juce::String (i + 1) + "_";
        bandParams[(size_t) i] = {
            apvts.getRawParameterValue (p + "on"),
            apvts.getRawParameterValue (p + "freq"),
            apvts.getRawParameterValue (p + "width"),
            apvts.getRawParameterValue (p + "lift"),
            apvts.getRawParameterValue (p + "depth"),
            apvts.getRawParameterValue (p + "gain")
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
    juce::ignoreUnused (midiMessages);   // consumed by MIDI rate mode in 4.2

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    if (rerollFlag.exchange (false))
    {
        // Modulator random-state re-deal happens here once engines exist (4.2).
    }

    // ── dry copy for the bypass crossfade ──
    dryBuffer.setSize (2, numSamples, false, false, true);
    for (int ch = 0; ch < 2; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ── per-block parameter targets ──
    const float amount = pAmount->load() * 0.01f;

    for (int i = 0; i < kNumBands; ++i)
    {
        auto& b  = bands[(size_t) i];
        auto& pp = bandParams[(size_t) i];

        const bool on = pp.on->load() > 0.5f;
        b.enable.setTargetValue (on ? 1.0f : 0.0f);

        // Band edges from centre + width (octaves), clamped into the audible
        // range with a guaranteed f_lo < f_hi ordering.
        const float freq  = pp.freq->load();
        const float width = pp.width->load();
        const float fLo = juce::jlimit (20.0f, 20000.0f, freq * std::pow (2.0f, -width * 0.5f));
        const float fHi = juce::jlimit (fLo * 1.02f, 20500.0f, freq * std::pow (2.0f,  width * 0.5f));
        b.splitLo.setCutoffFrequency (fLo);
        b.splitHi.setCutoffFrequency (fHi);
        b.apLow.setCutoffFrequency  (fHi);

        b.lift.setTargetValue (pp.lift->load() * 0.01f);
        b.gainLin.setTargetValue (juce::Decibels::decibelsToGain (pp.gain->load()));

        // Phase 4.1: STATIC pan target = depth · amount (verifies the whole
        // pan path audibly). Modulators replace this in Phase 4.2.
        b.pan.setTargetValue (juce::jlimit (-1.0f, 1.0f, (pp.depth->load() * 0.01f) * amount));
    }

    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    // ── per-sample cascade ──
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int s = 0; s < numSamples; ++s)
    {
        float xl = left[s], xr = right[s];

        for (int i = 0; i < kNumBands; ++i)
        {
            auto& b = bands[(size_t) i];

            const float e      = b.enable.getNextValue();
            const float liftV  = b.lift.getNextValue();
            const float gainV  = b.gainLin.getNextValue();
            const float panV   = b.pan.getNextValue();

            if (e < 1.0e-4f)
                continue;   // fully disengaged: stage is a wire (filters stay cold)

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

        const float og = outGainSm.getNextValue();
        const float by = bypassSm.getNextValue();

        xl *= og;  xr *= og;

        // crossfaded bypass back to the dry input copy
        left[s]  = xl + (dryBuffer.getSample (0, s) - xl) * by;
        right[s] = xr + (dryBuffer.getSample (1, s) - xr) * by;
    }
}

//==============================================================================
juce::String EntropanAudioProcessor::getStepsJson (int bandIndex) const
{
    return apvts.state.getProperty ("steps_b" + juce::String (bandIndex + 1), juce::String()).toString();
}

void EntropanAudioProcessor::setStepsJson (int bandIndex, const juce::String& json)
{
    apvts.state.setProperty ("steps_b" + juce::String (bandIndex + 1), json, nullptr);
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
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
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
