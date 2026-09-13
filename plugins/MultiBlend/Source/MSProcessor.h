#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

//==============================================================================
// Mid/Side morph processor.
//   morph = 0.0  → plugin processes MID component only; original Side preserved
//   morph = 0.5  → no encoding (full stereo passes through plugin as-is)
//   morph = 1.0  → plugin processes SIDE component only; original Mid preserved
//
// Intermediate values blend between processed and original by morph distance
// from 0.5.
//==============================================================================
class MSProcessor
{
public:
    MSProcessor() = default;

    void prepare (int maxBlockSize)
    {
        preserveBuffer.setSize (2, maxBlockSize);
    }

    //==========================================================================
    // Encode: store originals, then write component-only signal to buffer.
    //==========================================================================
    void encodeForMorph (juce::AudioBuffer<float>& buffer, float morph)
    {
        currentMorph = morph;
        if (std::abs (morph - 0.5f) < 0.005f) return;  // no-op at center

        const int numSamples = buffer.getNumSamples();
        if (buffer.getNumChannels() < 2) return;

        auto* L  = buffer.getWritePointer (0);
        auto* R  = buffer.getWritePointer (1);
        auto* pL = preserveBuffer.getWritePointer (0);
        auto* pR = preserveBuffer.getWritePointer (1);

        const bool processMid = (morph < 0.5f);

        for (int i = 0; i < numSamples; ++i)
        {
            const float l = L[i];
            const float r = R[i];
            pL[i] = l;  // save originals for decode
            pR[i] = r;

            const float mid  = (l + r) * 0.5f;
            const float side = (l - r) * 0.5f;

            if (processMid)
            {
                // Plugin sees mid in both channels (mono mid signal).
                L[i] = mid;
                R[i] = mid;
            }
            else
            {
                // Plugin sees side encoded as anti-phase stereo (L = side, R = -side).
                // After plugin processes, decode recovers it.
                L[i] = side;
                R[i] = -side;
            }
        }
    }

    //==========================================================================
    // Decode: recombine plugin output with preserved original component.
    // Output = blend(original, reconstructed) by |morph - 0.5| * 2.
    //==========================================================================
    void decodeFromMorph (juce::AudioBuffer<float>& buffer)
    {
        if (std::abs (currentMorph - 0.5f) < 0.005f) return;

        const int numSamples = buffer.getNumSamples();
        if (buffer.getNumChannels() < 2) return;

        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        const auto* pL = preserveBuffer.getReadPointer (0);
        const auto* pR = preserveBuffer.getReadPointer (1);

        const bool processedMid = (currentMorph < 0.5f);

        // 0 at center (0.5), 1 at extreme (0 or 1) — controls processing strength
        const float intensity = std::abs (currentMorph - 0.5f) * 2.0f;
        const float dry = 1.0f - intensity;

        for (int i = 0; i < numSamples; ++i)
        {
            const float origL = pL[i];
            const float origR = pR[i];
            const float origMid  = (origL + origR) * 0.5f;
            const float origSide = (origL - origR) * 0.5f;

            float reconL, reconR;

            if (processedMid)
            {
                // L,R contain plugin-processed mid; extract new mid (avg of channels)
                const float newMid = (L[i] + R[i]) * 0.5f;
                // Recombine with preserved side
                reconL = newMid + origSide;
                reconR = newMid - origSide;
            }
            else
            {
                // L = processed_side, R = -processed_side. Recover side.
                const float newSide = (L[i] - R[i]) * 0.5f;
                reconL = origMid + newSide;
                reconR = origMid - newSide;
            }

            // Blend dry original with fully reconstructed result by intensity
            L[i] = origL * dry + reconL * intensity;
            R[i] = origR * dry + reconR * intensity;
        }
    }

private:
    juce::AudioBuffer<float> preserveBuffer;
    float currentMorph = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MSProcessor)
};
