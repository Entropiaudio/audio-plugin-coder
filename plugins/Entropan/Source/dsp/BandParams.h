#pragma once

#include <atomic>

namespace entropan
{
    // Cached raw parameter pointers (hot path — no string lookups per block).
    // Only the params processBlock reads DIRECTLY live here. The ten mod-matrix
    // destination slots (freq, width, lift, depth, gain, rate, inertia, phase,
    // bias, stepsmooth) are deliberately absent: their values reach the audio
    // thread through the modDests registry / modVal[] post-modulation path.
    struct BandParams
    {
        std::atomic<float>* on;
        std::atomic<float>* mode;
        std::atomic<float>* ratemode;
        std::atomic<float>* div;
        std::atomic<float>* uni;
        std::atomic<float>* freeze;     // pause the band's modulators (all six hold their value)
        std::atomic<float>* biasFree;   // override: drop the bias headroom clamp (pan may hit the rail)
        std::atomic<float>* slope;      // 0..100 %, continuous log morph: 12 → 24 → 48 dB/oct
        std::atomic<float>* solo;       // monitor this band alone (its bell, panned)
        std::atomic<float>* shape;      // Bell / Low / High / Notch / Tilt
    };

    // Same idea for the non-band parameters, owned by the processor as `gp`.
    // amount / output / flux are absent for the same reason as the band slots:
    // they are mod-matrix destinations, read via modDests / modVal[].
    struct GlobalParams
    {
        std::atomic<float>* bypass  = nullptr;
        std::atomic<float>* seed    = nullptr;
        std::atomic<float>* speed   = nullptr;
        std::atomic<float>* routing = nullptr;   // 0 = Serial, 1 = Parallel
        std::atomic<float>* wow     = nullptr;
        std::atomic<float>* flutter = nullptr;
        std::atomic<float>* envAtk  = nullptr;
        std::atomic<float>* envRel  = nullptr;
        std::atomic<float>* envScf  = nullptr;
        std::atomic<float>* envRms  = nullptr;
        std::atomic<float>* envGain = nullptr;
    };
}
