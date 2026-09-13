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

    // sm* mirror i0/i1/b0/fr but in the SMALL FFT's bin domain — used for a big
    // column while the big FFT hasn't produced a frame yet (startup / after an
    // overrun flush). Precomputed so computeColumns never recomputes per bin.
    struct ColGeom { int i0 = 1, i1 = 2, b0 = 1; float fr = 0; bool useBig = false;
                     int smI0 = 1, smI1 = 2, smB0 = 1; float smFr = 0; };

    inline double colF0 (int b)            { return 20.0 * std::pow (1000.0, (double) b / kBins); }
    inline double colF1 (int b)            { return 20.0 * std::pow (1000.0, (double) (b + 1) / kBins); }

    // one column's bin geometry at FFT size N — the single source both the
    // primary and the small-FFT-fallback geometry are built from.
    inline void geomForN (int b, int N, double sr, int& b0, float& fr, int& i0, int& i1)
    {
        const double f0 = colF0 (b), f1 = colF1 (b);
        const double fc = std::sqrt (f0 * f1);
        const double binHz = sr / (double) N;
        b0 = juce::jlimit (1, N / 2 - 2, (int) (fc / binHz));
        fr = (float) juce::jlimit (0.0, 1.0, fc / binHz - b0);
        i0 = juce::jlimit (1, N / 2 - 1, (int) (f0 / binHz));
        i1 = juce::jlimit (i0 + 1, N / 2, (int) std::ceil (f1 / binHz));
    }

    inline void buildGeometry (double sr, ColGeom* g /*kBins*/)
    {
        for (int b = 0; b < kBins; ++b)
        {
            auto& cg = g[b];
            const double fc = std::sqrt (colF0 (b) * colF1 (b));
            cg.useBig = fc < kSplitHz;
            geomForN (b, cg.useBig ? kBigSize : kSmallSize, sr, cg.b0, cg.fr, cg.i0, cg.i1);
            if (cg.useBig)   // small-FFT fallback geometry for this big column
                geomForN (b, kSmallSize, sr, cg.smB0, cg.smFr, cg.smI0, cg.smI1);
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
            // big column with no big frame yet → its precomputed small-FFT geometry
            const bool sm = cg.useBig && ! big;
            const int  i0 = sm ? cg.smI0 : cg.i0, i1 = sm ? cg.smI1 : cg.i1, b0 = sm ? cg.smB0 : cg.b0;
            const float fr = sm ? cg.smFr : cg.fr;
            float mag;
            if (i1 - i0 >= 2) { mag = 0; for (int k = i0; k < i1; ++k) mag = juce::jmax (mag, m[(size_t) k]); }
            else              mag = m[(size_t) b0] * (1.0f - fr) + m[(size_t) (b0 + 1)] * fr;
            const double db = juce::Decibels::gainToDecibels ((double) mag / (double) (N / 4), -80.0);
            outDb[b] = (float) juce::jlimit (-60.0, 0.0, db);
        }
    }
}
