#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <cstring>

namespace entropan
{
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
}
