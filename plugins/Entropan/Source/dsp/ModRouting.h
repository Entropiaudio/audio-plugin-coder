#pragma once

#include "EntropanSpec.h"

namespace entropan
{
    // Mod-matrix routes (see kMaxRoutes / kNumDests in EntropanSpec.h for the
    // dst-index layout). Routes live as JSON in apvts.state ("modRoutes") →
    // undoable + persisted; applied at block rate in the NORMALIZED param
    // domain (skew-aware), as an offset AFTER the atomic read — host
    // automation and the UI stay untouched.
    // stype: which of the source band's six waveforms drives the route
    // (0..5 explicit; −1 = follow the band's selected MODE — the pre-B68
    // behaviour, kept so old sessions load identically).
    struct ModRoute  { int src = 0; int stype = -1; int dst = -1; float depth = 0.0f; };  // depth −100..+100
    struct RoutesData { int count = 0; ModRoute routes[kMaxRoutes]; };
}
