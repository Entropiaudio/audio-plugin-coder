#pragma once

#include <juce_dsp/juce_dsp.h>

// Shared analyzer resample math — compiled into BOTH the editor and the test
// harness, so the gate exercises the real display pipeline, not a replica.
//
// Multi-resolution: a 16384-pt FFT feeds columns above kAnalyzerSplitHz (fast,
// ~341 ms window), a 65536-pt FFT feeds the columns below it (sharp lows,
// ~1.4 s window — the standard trade, same as commercial analyzers). Both use
// Blackman-Harris (sidelobes below the 60 dB display floor).
namespace EntropanAnalyzer
{
    constexpr int    kSmallOrder = 14, kSmallSize = 1 << kSmallOrder;
    constexpr int    kBigOrder   = 16, kBigSize   = 1 << kBigOrder;
    constexpr int    kBins       = 256;
    constexpr double kSplitHz    = 500.0;

    struct ColGeom { int i0 = 1, i1 = 2, b0 = 1; float fr = 0; bool useBig = false; };

    inline double colF0 (int b)            { return 20.0 * std::pow (1000.0, (double) b / kBins); }
    inline double colF1 (int b)            { return 20.0 * std::pow (1000.0, (double) (b + 1) / kBins); }

    inline void buildGeometry (double sr, ColGeom* g /*kBins*/)
    {
        for (int b = 0; b < kBins; ++b)
        {
            const double f0 = colF0 (b), f1 = colF1 (b);
            const double fc = std::sqrt (f0 * f1);
            const bool big  = fc < kSplitHz;
            const int  N    = big ? kBigSize : kSmallSize;
            const double binHz = sr / (double) N;
            auto& cg = g[b];
            cg.useBig = big;
            cg.b0 = juce::jlimit (1, N / 2 - 2, (int) (fc / binHz));
            cg.fr = (float) juce::jlimit (0.0, 1.0, fc / binHz - cg.b0);
            cg.i0 = juce::jlimit (1, N / 2 - 1, (int) (f0 / binHz));
            cg.i1 = juce::jlimit (cg.i0 + 1, N / 2, (int) std::ceil (f1 / binHz));
        }
    }

    // smallMags: |FFT| of the last kSmallSize samples; bigMags: of the last
    // kBigSize (nullptr until enough audio has accumulated — the small FFT
    // then covers the lows too, at its coarser resolution).
    inline void computeColumns (const ColGeom* g, const float* smallMags,
                                const float* bigMags, float* outDb /*kBins*/)
    {
        for (int b = 0; b < kBins; ++b)
        {
            const auto& cg = g[b];
            const bool big = cg.useBig && bigMags != nullptr;
            const float* m = big ? bigMags : smallMags;
            const int    N = big ? kBigSize : kSmallSize;
            int i0 = cg.i0, i1 = cg.i1, b0 = cg.b0; float fr = cg.fr;
            if (cg.useBig && ! big)   // fallback: recompute for the small FFT
            {
                const double binHz = 0;  juce::ignoreUnused (binHz);
                const double f0 = colF0 (b), f1 = colF1 (b), fc = std::sqrt (f0 * f1);
                const double sBin = (double) kSmallSize / (double) kBigSize;   // ratio of bin widths
                b0 = juce::jlimit (1, kSmallSize / 2 - 2, (int) (cg.b0 * sBin + 0.5));
                fr = 0.0f;
                i0 = juce::jlimit (1, kSmallSize / 2 - 1, (int) (cg.i0 * sBin));
                i1 = juce::jlimit (i0 + 1, kSmallSize / 2, (int) std::ceil (cg.i1 * sBin));
                juce::ignoreUnused (f0, f1, fc);
            }
            float mag;
            if (i1 - i0 >= 2) { mag = 0; for (int k = i0; k < i1; ++k) mag = juce::jmax (mag, m[(size_t) k]); }
            else              mag = m[(size_t) b0] * (1.0f - fr) + m[(size_t) (b0 + 1)] * fr;
            const double db = juce::Decibels::gainToDecibels ((double) mag / (double) (N / 4), -80.0);
            outDb[b] = (float) juce::jlimit (-60.0, 0.0, db);
        }
    }
}
