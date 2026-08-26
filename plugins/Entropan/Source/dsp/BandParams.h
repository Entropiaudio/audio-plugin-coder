#pragma once

#include <atomic>

namespace entropan
{
    // Cached raw parameter pointers (hot path — no string lookups per block)
    struct BandParams
    {
        std::atomic<float>* on;
        std::atomic<float>* freq;
        std::atomic<float>* width;
        std::atomic<float>* lift;
        std::atomic<float>* depth;
        std::atomic<float>* gain;
        std::atomic<float>* mode;
        std::atomic<float>* rate;
        std::atomic<float>* ratemode;
        std::atomic<float>* div;
        std::atomic<float>* inertia;
        std::atomic<float>* phase;
        std::atomic<float>* uni;
        std::atomic<float>* freeze;     // pause the band's modulators (all six hold their value)
        std::atomic<float>* bias;
        std::atomic<float>* biasFree;   // override: drop the bias headroom clamp (pan may hit the rail)
        std::atomic<float>* stepSmooth; // Steps mode: 0 = square, 100 = glide between steps
        std::atomic<float>* slope;      // 0..100 %, continuous log morph: 12 → 24 → 48 dB/oct
        std::atomic<float>* solo;       // monitor this band alone (its bell, panned)
        std::atomic<float>* shape;      // Bell / Low / High / Notch / Tilt
    };

    // Same idea for the non-band parameters, owned by the processor as `gp`.
    struct GlobalParams
    {
        std::atomic<float>* amount  = nullptr;
        std::atomic<float>* output  = nullptr;
        std::atomic<float>* bypass  = nullptr;
        std::atomic<float>* seed    = nullptr;
        std::atomic<float>* speed   = nullptr;
        std::atomic<float>* routing = nullptr;   // 0 = Serial, 1 = Parallel
        std::atomic<float>* wow     = nullptr;
        std::atomic<float>* flutter = nullptr;
        std::atomic<float>* flux    = nullptr;
        std::atomic<float>* envAtk  = nullptr;
        std::atomic<float>* envRel  = nullptr;
        std::atomic<float>* envScf  = nullptr;
        std::atomic<float>* envRms  = nullptr;
        std::atomic<float>* envGain = nullptr;
    };
}
