#pragma once

#include <juce_dsp/juce_dsp.h>

namespace entropan
{
    // Wow & flutter transport state — the delay tap, the two LFO phases, and
    // the FLUX randomization. The pitch spec (rates, depths, base delay,
    // engage glide, jitter) lives in EntropanSpec.h; the per-sample tap math
    // stays in EntropanAudioProcessor::processBlock, which owns one of these
    // as `wf`.
    struct WowFlutterState
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay { 4096 };
        double wowPhase = 0.0, flutPhase = 0.0;
        // FLUX state: per-revolution random walk + rate jitter for each modulator.
        double wowWalk = 0.0, flutWalk = 0.0;      // smoothed value, bounded -1..1
        double wowWalkT = 0.0, flutWalkT = 0.0;    // target dealt on each wrap
        double wowJit = 1.0, flutJit = 1.0;        // rate multiplier for this revolution
        juce::Random rng { 0x656E7472 };           // fixed seed: same tape every session
        juce::SmoothedValue<float> engage;
        // tap-depth smoothing: block-rate depth jumps clicked while turning the knobs
        juce::SmoothedValue<float> wowDepthSm, flutDepthSm;
    };
}
