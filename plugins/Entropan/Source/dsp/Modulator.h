#pragma once

#include <juce_core/juce_core.h>
#include <limits>
#include "EntropanSpec.h"

namespace entropan
{
    // Per-band modulation engine (Phase 4.2). Evaluated PER SAMPLE — free mode
    // reaches audio rate with no control-grid artifacts. Random modes use
    // stateless cell hashes → loop-safe, reproducible per seed, re-rollable.
    struct Modulator
    {
        double phase   = 0.0;    // cycle position 0..1 (free/MIDI accumulate; sync derives from PPQ)
        // ALL six waveforms run off this one clock, each with its own slew —
        // the pan uses value[mode], the mod matrix can tap any of them
        // independently (S&H on freq while Sine drives gain, etc.).
        float  value[kNumWaves] {};      // post-slew outputs (-1..1), one per waveform
        float  target[kNumWaves] {};
        float  slewCoeff[kNumWaves] {};  // per-sample one-pole coefficients
        double lx = 0.1, ly = 0.0, lz = 0.0;   // Lorenz state (one stream per band)
        float  panOut  = 0.0f;   // final pan (post bias + depth·amount) for scope/telemetry
        // S&H cell-hash cache — the hashed value is constant for a whole cell,
        // so rehash only when (cell, seed, reroll) changes.
        juce::int64 lastCell = std::numeric_limits<juce::int64>::min();
        int    lastSeed = -1, lastReroll = -1;
        float  cellA = 0.0f;
    };
}
