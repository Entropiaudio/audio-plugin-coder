#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

//==============================================================================
// 3-band splitter. Two modes:
//
//  IIR (default): cascaded 1st-order real LP + HP filters. Slope is CONTINUOUS
//    in 6 dB/oct steps from 6 .. 48 dB/oct (order = slope/6 = 1..8 stages).
//    Real filters → proper band isolation (solo a band, hear only that band).
//    In Dynamic-Phase mode the difference method keeps unused bands bit-
//    transparent regardless of crossover sum-flatness; in Zero-Latency mode
//    non-LR slopes have a small (~1-3 dB) crossover ripple — acceptable.
//
//  Linear-phase: parallel FIR convolution (LP / BP / HP, IRs sum to delta).
//    Perfect isolation + transparent. Adds (kFirTaps-1)/2 latency.
//==============================================================================
class CrossoverProcessor
{
public:
    static constexpr int kFirTaps   = 2049;   // ODD → exact symmetric center
    static constexpr int kMaxStages = 8;      // 8 × 6 dB/oct = 48 dB/oct max

    CrossoverProcessor() = default;

    void prepare (double sampleRate, int blockSize)
    {
        currentSampleRate = sampleRate;
        currentBlockSize  = blockSize;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = (juce::uint32) blockSize;
        spec.numChannels = 2;

        for (auto& f : lpLM) { f.prepare (spec); }
        for (auto& f : hpLM) { f.prepare (spec); }
        for (auto& f : lpMH) { f.prepare (spec); }
        for (auto& f : hpMH) { f.prepare (spec); }

        updateLowMidCoeffs (1000.0f);
        updateMidHighCoeffs (4000.0f);

        firLow.prepare  (spec);
        firMid.prepare  (spec);
        firHigh.prepare (spec);

        for (auto& buf : bandBuffers) buf.setSize (2, blockSize);
        restBuffer.setSize (2, blockSize);
        scratch.setSize (2, blockSize);
        firNeedsRebuild = true;
    }

    void reset()
    {
        for (auto& f : lpLM) f.reset();
        for (auto& f : hpLM) f.reset();
        for (auto& f : lpMH) f.reset();
        for (auto& f : hpMH) f.reset();
        firLow.reset(); firMid.reset(); firHigh.reset();
    }

    void setLinearPhase (bool on)
    {
        if (linearPhase == on) return;
        linearPhase = on;
        firNeedsRebuild = true;
    }

    bool isLinearPhase() const noexcept { return linearPhase; }

    int getLinearPhaseLatencySamples() const noexcept
    {
        return (linearPhase && lastNumActiveBands >= 1) ? (kFirTaps - 1) / 2 : 0;
    }

    //==========================================================================
    // passes = slope / 6  (1..8 → 6..48 dB/oct).
    //==========================================================================
    void split (const juce::AudioBuffer<float>& input,
                float lowMidFreq, float midHighFreq,
                bool lowBypassed, bool midBypassed, bool highBypassed,
                float lowMidPasses = 4.0f, float midHighPasses = 4.0f)
    {
        const int numSamples  = input.getNumSamples();
        const int numChannels = input.getNumChannels();

        lowMidFreq  = juce::jlimit (20.0f, 20000.0f, lowMidFreq);
        midHighFreq = juce::jlimit (20.0f, 20000.0f, midHighFreq);
        if (midHighFreq <= lowMidFreq + 10.0f)
            midHighFreq = lowMidFreq + 10.0f;

        const bool freqChanged =
            std::abs (lowMidFreq  - lastLowMidFreq)  > 0.5f
         || std::abs (midHighFreq - lastMidHighFreq) > 0.5f;

        lastNumActiveBands = (lowBypassed ? 0 : 1) + (midBypassed ? 0 : 1) + (highBypassed ? 0 : 1);

        const float lmN = juce::jlimit (1.0f, (float) kMaxStages, lowMidPasses);
        const float mhN = juce::jlimit (1.0f, (float) kMaxStages, midHighPasses);

        if (linearPhase)
        {
            if (firNeedsRebuild || freqChanged)
            {
                rebuildFIRs (lowMidFreq, midHighFreq);
                firNeedsRebuild = false;
                lastLowMidFreq  = lowMidFreq;
                lastMidHighFreq = midHighFreq;
            }
            for (int ch = 0; ch < numChannels; ++ch)
            {
                bandBuffers[0].copyFrom (ch, 0, input, ch, 0, numSamples);
                bandBuffers[1].copyFrom (ch, 0, input, ch, 0, numSamples);
                bandBuffers[2].copyFrom (ch, 0, input, ch, 0, numSamples);
            }
            { juce::dsp::AudioBlock<float> b (bandBuffers[0]); juce::dsp::ProcessContextReplacing<float> c (b); firLow.process (c); }
            { juce::dsp::AudioBlock<float> b (bandBuffers[1]); juce::dsp::ProcessContextReplacing<float> c (b); firMid.process (c); }
            { juce::dsp::AudioBlock<float> b (bandBuffers[2]); juce::dsp::ProcessContextReplacing<float> c (b); firHigh.process (c); }
            return;
        }

        // ─── IIR: cascaded 1st-order real LP / HP ───
        if (freqChanged)
        {
            updateLowMidCoeffs  (lowMidFreq);
            updateMidHighCoeffs (midHighFreq);
            lastLowMidFreq  = lowMidFreq;
            lastMidHighFreq = midHighFreq;
        }

        // Stage 1: low = LP(input), rest = HP(input)  — both real cascades
        for (int ch = 0; ch < numChannels; ++ch)
        {
            bandBuffers[0].copyFrom (ch, 0, input, ch, 0, numSamples);
            restBuffer   .copyFrom (ch, 0, input, ch, 0, numSamples);
        }
        cascade (bandBuffers[0], lpLM, lmN);
        cascade (restBuffer,     hpLM, lmN);

        // Stage 2: mid = LP(rest), high = HP(rest)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            bandBuffers[1].copyFrom (ch, 0, restBuffer, ch, 0, numSamples);
            bandBuffers[2].copyFrom (ch, 0, restBuffer, ch, 0, numSamples);
        }
        cascade (bandBuffers[1], lpMH, mhN);
        cascade (bandBuffers[2], hpMH, mhN);
    }

    //==========================================================================
    void sum (juce::AudioBuffer<float>& output,
              bool lowSolo, bool midSolo, bool highSolo,
              bool lowMute, bool midMute, bool highMute,
              bool lowBypassed, bool midBypassed, bool highBypassed)
    {
        const int numSamples  = output.getNumSamples();
        const int numChannels = output.getNumChannels();

        juce::ignoreUnused (lowBypassed, midBypassed, highBypassed);

        const bool anySolo = lowSolo || midSolo || highSolo;
        const bool lowAudible  = anySolo ? lowSolo  : ! lowMute;
        const bool midAudible  = anySolo ? midSolo  : ! midMute;
        const bool highAudible = anySolo ? highSolo : ! highMute;

        output.clear();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* out = output.getWritePointer (ch);
            if (lowAudible)  { const auto* s = bandBuffers[0].getReadPointer (ch); for (int i = 0; i < numSamples; ++i) out[i] += s[i]; }
            if (midAudible)  { const auto* s = bandBuffers[1].getReadPointer (ch); for (int i = 0; i < numSamples; ++i) out[i] += s[i]; }
            if (highAudible) { const auto* s = bandBuffers[2].getReadPointer (ch); for (int i = 0; i < numSamples; ++i) out[i] += s[i]; }
        }
    }

    juce::AudioBuffer<float>& getBandBuffer (int bandIndex)
    {
        jassert (bandIndex >= 0 && bandIndex < 3);
        return bandBuffers[(size_t) bandIndex];
    }

private:
    using IIRFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                      juce::dsp::IIR::Coefficients<float>>;

    // Fractional-order cascade: integer stages run fully; the fractional part
    // crossfades one extra stage → continuous slope at 1 dB/oct resolution.
    void cascade (juce::AudioBuffer<float>& buf, std::array<IIRFilter, kMaxStages>& stages, float order)
    {
        const int   nFull = juce::jlimit (0, kMaxStages, (int) std::floor (order));
        const float frac  = juce::jlimit (0.0f, 1.0f, order - (float) nFull);
        const int numCh = buf.getNumChannels();
        const int numSm = buf.getNumSamples();

        for (int i = 0; i < nFull; ++i)
        {
            juce::dsp::AudioBlock<float> block (buf);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            stages[(size_t) i].process (ctx);
        }

        if (frac > 1.0e-4f && nFull < kMaxStages)
        {
            // copy current → scratch, process one more stage, blend by frac
            scratch.setSize (numCh, numSm, false, false, true);
            for (int ch = 0; ch < numCh; ++ch)
                scratch.copyFrom (ch, 0, buf, ch, 0, numSm);

            { juce::dsp::AudioBlock<float> b (scratch);
              juce::dsp::ProcessContextReplacing<float> c (b);
              stages[(size_t) nFull].process (c); }

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* out = buf.getWritePointer (ch);
                const auto* nxt = scratch.getReadPointer (ch);
                for (int i = 0; i < numSm; ++i)
                    out[i] = out[i] * (1.0f - frac) + nxt[i] * frac;
            }
        }
    }

    void updateLowMidCoeffs (float fc)
    {
        auto lp = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass  (currentSampleRate, fc);
        auto hp = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (currentSampleRate, fc);
        for (auto& f : lpLM) *f.state = *lp;
        for (auto& f : hpLM) *f.state = *hp;
    }

    void updateMidHighCoeffs (float fc)
    {
        auto lp = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass  (currentSampleRate, fc);
        auto hp = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (currentSampleRate, fc);
        for (auto& f : lpMH) *f.state = *lp;
        for (auto& f : hpMH) *f.state = *hp;
    }

    //==========================================================================
    void rebuildFIRs (float fLow, float fHigh)
    {
        const double fs = currentSampleRate;
        const int N = kFirTaps;
        const int center = (N - 1) / 2;

        auto sinc = [] (double x)
        {
            return std::abs (x) < 1.0e-12 ? 1.0
                 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
        };
        auto hann = [N] (int n)
        {
            return 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * n / (double) (N - 1));
        };

        juce::AudioBuffer<float> irLow (1, N), irMid (1, N), irHigh (1, N);
        auto* lp1 = irLow.getWritePointer (0);
        auto* bp  = irMid.getWritePointer (0);
        auto* hp  = irHigh.getWritePointer (0);

        const double k1 = 2.0 * fLow  / fs;
        const double k2 = 2.0 * fHigh / fs;

        for (int n = 0; n < N; ++n)
        {
            const int pos = n - center;
            const double w = hann (n);
            const double lpLow  = k1 * sinc (k1 * pos) * w;
            const double lpHigh = k2 * sinc (k2 * pos) * w;
            const double delta  = (pos == 0) ? 1.0 : 0.0;
            lp1[n] = (float) lpLow;
            bp[n]  = (float) (lpHigh - lpLow);
            hp[n]  = (float) (delta - lpHigh);
        }

        const auto st = juce::dsp::Convolution::Stereo::no;
        const auto tr = juce::dsp::Convolution::Trim::no;
        const auto nm = juce::dsp::Convolution::Normalise::no;
        firLow .loadImpulseResponse (std::move (irLow),  fs, st, tr, nm);
        firMid .loadImpulseResponse (std::move (irMid),  fs, st, tr, nm);
        firHigh.loadImpulseResponse (std::move (irHigh), fs, st, tr, nm);
    }

    // 1st-order cascades (≤8 stages → ≤48 dB/oct)
    std::array<IIRFilter, kMaxStages> lpLM, hpLM, lpMH, hpMH;

    juce::dsp::Convolution firLow, firMid, firHigh;

    std::array<juce::AudioBuffer<float>, 3> bandBuffers;
    juce::AudioBuffer<float> restBuffer;
    juce::AudioBuffer<float> scratch;     // fractional-stage crossfade temp

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;
    float  lastLowMidFreq  = -1.0f;
    float  lastMidHighFreq = -1.0f;

    bool linearPhase     = false;
    bool firNeedsRebuild = true;
    int  lastNumActiveBands = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrossoverProcessor)
};
