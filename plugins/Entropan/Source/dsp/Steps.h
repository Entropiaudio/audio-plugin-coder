#pragma once

#include "EntropanSpec.h"

namespace entropan
{
    // ── Steps engine (Phase 4.3) ──
    // Slot-stable ratchets parsed from the per-band JSON into RT-safe snapshots
    // (double-buffered, atomic index swap — message thread writes, audio reads).
    struct StepSlot  { int subdiv = 1; float vals[4] { 0, 0, 0, 0 }; bool tie = false; };  // tie = glued to the run on its left
    struct StepsData
    {
        int count = 0;
        StepSlot slots[kMaxSteps];
        // precomputed glue runs (message thread): for each cell, the run's leader
        // index and length. A glued step holds the leader's value across the run.
        int runStart[kMaxSteps] {};
        int runLen[kMaxSteps] {};
    };
}
