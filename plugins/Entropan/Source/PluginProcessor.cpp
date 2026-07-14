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
    using P = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto pct = [] (float def) {
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
            juce::ParameterID { p + "lift", 1 }, label + "Lift", pct (100.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "depth", 1 }, label + "Depth", pct (50.0f), 50.0f,
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
            juce::ParameterID { p + "inertia", 1 }, label + "Inertia", pct (60.0f), 60.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "phase", 1 }, label + "Phase",
            juce::NormalisableRange<float> (0.0f, 360.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("deg")));
    }

    return { params.begin(), params.end() };
}

//==============================================================================
void EntropanAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    // DSP graph allocation lands in Phase 4.1.
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

    if (buffer.getNumSamples() == 0)
        return;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Phase 4.0: clean passthrough. Band splitter cascade + lift/pan/gain
    // stages arrive in Phase 4.1; modulators + MIDI tracking in 4.2.
    juce::ignoreUnused (midiMessages);

    if (rerollFlag.exchange (false))
    {
        // Modulator random-state re-deal happens here once engines exist (4.2).
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
