#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    // Log-feel range via JUCE's standard skew (centre = geometric mean).
    // IMPORTANT: custom-lambda ranges serialize skew=1 to the WebView JS lib
    // (which only knows start/end/skew) → UI and host disagree on every
    // position. Standard skew keeps C++ and JS byte-compatible.
    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        juce::NormalisableRange<float> r (lo, hi, 0.0f);
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    }

    const juce::StringArray kModeChoices { "Clean", "Warm", "Crush" };

    // Window size lives in the same tree for persistence, but must not take
    // part in undo (a resize would otherwise create/restore undo steps).
    juce::ValueTree stateForUndo (const juce::ValueTree& src)
    {
        auto v = src.createCopy();
        v.removeProperty ("editorWidth", nullptr);
        v.removeProperty ("editorHeight", nullptr);
        return v;
    }
}

//==============================================================================
__PLUGIN___AudioProcessor::__PLUGIN___AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    lastCommitted = stateForUndo (apvts.copyState());
}

juce::AudioProcessorValueTreeState::ParameterLayout __PLUGIN___AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 }, "Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "cutoff", 1 }, "Cutoff",
        logRange (20.0f, 20000.0f), 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Mode", kModeChoices, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

//==============================================================================
void __PLUGIN___AudioProcessor::commitUndoIfChanged()
{
    auto cur = stateForUndo (apvts.copyState());
    if (lastCommitted.isValid() && cur.isEquivalentTo (lastCommitted))
        return;
    if (lastCommitted.isValid())
    {
        undoStack.push_back (lastCommitted);
        if ((int) undoStack.size() > kUndoDepth)
            undoStack.erase (undoStack.begin());
    }
    lastCommitted = cur;
    redoStack.clear();
}

bool __PLUGIN___AudioProcessor::undoState()
{
    if (undoStack.empty())
        return false;
    redoStack.push_back (stateForUndo (apvts.copyState()));
    auto snap = undoStack.back();
    undoStack.pop_back();
    snap.setProperty ("editorWidth",  apvts.state.getProperty ("editorWidth",  720), nullptr);
    snap.setProperty ("editorHeight", apvts.state.getProperty ("editorHeight", 480), nullptr);
    apvts.replaceState (snap);
    lastCommitted = stateForUndo (apvts.copyState());
    return true;
}

bool __PLUGIN___AudioProcessor::redoState()
{
    if (redoStack.empty())
        return false;
    undoStack.push_back (stateForUndo (apvts.copyState()));
    auto snap = redoStack.back();
    redoStack.pop_back();
    snap.setProperty ("editorWidth",  apvts.state.getProperty ("editorWidth",  720), nullptr);
    snap.setProperty ("editorHeight", apvts.state.getProperty ("editorHeight", 480), nullptr);
    apvts.replaceState (snap);
    lastCommitted = stateForUndo (apvts.copyState());
    return true;
}

//==============================================================================
void __PLUGIN___AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;

    gain.prepare (spec);
    gain.setRampDurationSeconds (0.02);
    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    bypassSm.reset (sampleRate, 0.030);
    dryBuffer.setSize (2, juce::jmax (samplesPerBlock * 2, 8192));

    pGain   = apvts.getRawParameterValue ("gain");
    pCutoff = apvts.getRawParameterValue ("cutoff");
    pMode   = apvts.getRawParameterValue ("mode");
    pBypass = apvts.getRawParameterValue ("bypass");
}

bool __PLUGIN___AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && out == layouts.getMainInputChannelSet();
}

void __PLUGIN___AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    dryBuffer.setSize (2, numSamples, false, false, true);
    for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ── demo DSP: gain → lowpass. Replace with your own. ──
    gain.setGainDecibels (pGain->load());
    filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, pCutoff->load()));

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    gain.process (ctx);
    filter.process (ctx);
    juce::ignoreUnused (pMode);   // wire the 'mode' choice into your DSP here

    // crossfaded bypass to the dry copy
    bypassSm.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    for (int s = 0; s < numSamples; ++s)
    {
        const float by = bypassSm.getNextValue();
        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[s] = d[s] + (dryBuffer.getSample (ch, s) - d[s]) * by;
        }
    }
}

//==============================================================================
void __PLUGIN___AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void __PLUGIN___AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
            undoStack.clear();
            redoStack.clear();
            lastCommitted = stateForUndo (apvts.copyState());
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* __PLUGIN___AudioProcessor::createEditor()
{
    return new __PLUGIN___AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new __PLUGIN___AudioProcessor();
}
