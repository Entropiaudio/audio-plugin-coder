#pragma once

#include <juce_core/juce_core.h>

// Compile-time spec shared by the processor, the DSP building blocks
// (Source/dsp/*), the editor and the null-test harness. The processor keeps
// class-scope aliases (EntropanAudioProcessor::kNumBands etc.) so existing
// call sites read the same as before the split.
namespace entropan
{
    constexpr int kNumBands = 6;
    constexpr int kMaxSteps = 16;
    constexpr int kNumWaves = 6;   // Sine, Tri, S&H, Chaos, Steps, Env

    // ── Mod matrix: band modulators → any continuous parameter ──
    // dst index = band·10 + slot (freq,width,lift,depth,gain,rate,inertia,
    // phase,bias,stepsmooth) or 60+ (amount,wow,flutter,output).
    constexpr int kMaxRoutes = 16;
    constexpr int kDestSlotsPerBand = 10;
    // +5: amount, wow, flutter, output, flux. Flux is APPENDED so every saved
    // route keeps its dst index.
    constexpr int kNumDests = kNumBands * kDestSlotsPerBand + 5;

    // ── Wow & flutter spec ────────────────────────────────────────────────
    // Depth is stated as PEAK PITCH DEVIATION, not as a delay time, because
    // that is what the ear judges and what tape decks are specced by. A sine
    // modulating a delay swings pitch by 2*pi*rate*peakDelay, so the peak delay
    // each one needs is kWowPitch / (2*pi*kWowRate) — which means the rates can
    // be retuned without the depth character drifting with them.
    //   0.80% ~ 13.8 cents, 0.60% ~ 10.4 cents at 100%: roughly twice a badly
    //   worn cassette (0.15-0.35%), so 100% is openly an effect while ordinary
    //   tape still lands around a third of the way up the knob.
    constexpr double kWowRate    = 0.5;      // Hz — capstan drift
    constexpr double kFlutRate   = 6.3;      // Hz — pinch-roller flutter
    constexpr double kWowPitch   = 0.0080;   // peak dp/p at 100%
    constexpr double kFlutPitch  = 0.0060;
    constexpr double kWowPeakS   = kWowPitch  / (2.0 * juce::MathConstants<double>::pi * kWowRate);
    constexpr double kFlutPeakS  = kFlutPitch / (2.0 * juce::MathConstants<double>::pi * kFlutRate);
    // The read tap sits kBaseDelayS behind and swings +/-(wow+flutter); the base
    // only has to keep it positive, and every extra millisecond is latency the
    // host has to compensate. 1.5x the worst-case swing still leaves ~40 samples
    // of margin under the tap, twenty times what Lagrange3rd needs.
    constexpr double kBaseDelayS = 1.5 * (kWowPeakS + kFlutPeakS);
    // Engage GLIDE, not crossfade — see the tap ramp in processBlock. The dip
    // this costs is kBaseDelayS / kEngageS in pitch, so 4.0 ms over 300 ms is
    // ~1.3%: the same order as the wow itself, which is why it reads as the
    // machine spinning up rather than as an edit.
    constexpr double kEngageS    = 0.30;
    // FLUX — a real transport is not two sine waves. The capstan speeds and
    // slows, flutter drifts with tape tension, and no two revolutions match.
    // FLUX morphs each modulator from a pure periodic sine toward a random
    // walk and jitters its rate, re-dealt once per revolution.
    //
    // It is a CROSSFADE toward the walk, never an addition, and the walk is
    // bounded to the same +/-1 the sine occupies — so the worst-case delay
    // excursion, and therefore the reported latency, does not grow with FLUX.
    // The extra intensity comes from the rate jitter instead: pitch swing is
    // 2*pi*rate*peak, so a faster revolution swings harder for free.
    constexpr double kFluxJitter = 0.6;   // rate wander, +/-60% at 100%
}
