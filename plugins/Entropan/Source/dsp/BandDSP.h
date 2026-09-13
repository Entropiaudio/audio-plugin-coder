#pragma once

#include <juce_dsp/juce_dsp.h>
#include "FilterCascade.h"

namespace entropan
{
    using BellCascade = FilterCascade;   // the bell path still reads this name

    // Band shapes. All but Tilt are a single unity-gain extraction, so they drop
    // straight into out = in + B(in)·lift·(g−1). Notch needs no filter of its
    // own — it is what the bell leaves behind.
    enum BandShape { ShapeBell = 0, ShapeLow, ShapeHigh, ShapeNotch, ShapeTilt, kNumShapes };

    struct BandDSP
    {
        BellCascade bell2, bell4, bell8;      // ≈12 / 24 / 48 dB/oct skirts
        // Second bank, only fed by Tilt: it needs a low-pass AND a high-pass at
        // once so the two halves can be panned in opposite directions. Every
        // other shape leaves these silent and pays nothing for them.
        BellCascade hi2, hi4, hi8;
        juce::SmoothedValue<float> slope01;   // 0..1 (param /100), 30 ms

        juce::SmoothedValue<float> lift;      // 0..1
        juce::SmoothedValue<float> gainLin;   // linear, from ±6 dB
        juce::SmoothedValue<float> enable;    // 0..1 engage crossfade (~30 ms)
        juce::SmoothedValue<float> depth;     // 0..1, per-sample (automation-safe)
        juce::SmoothedValue<float> bias;      // -1..1 static pan offset (resting position)
        float fcCur = 1000.0f, wCur = 1.0f;            // block-rate fc / width glide state
        float fcApplied = -1.0f, wApplied = -1.0f;     // last values pushed into the bells
        int   shapeApplied = -1;                       // a shape change must re-push too
        bool  cutoffsInit = false;

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            const double sr = spec.sampleRate;
            bell2.configure (2); bell4.configure (4); bell8.configure (8);
            bell2.prepare (sr);  bell4.prepare (sr);  bell8.prepare (sr);
            hi2.configure (2);   hi4.configure (4);   hi8.configure (8);
            hi2.prepare (sr);    hi4.prepare (sr);    hi8.prepare (sr);
            slope01.reset (sr, 0.030);
            lift.reset    (sr, 0.005);
            gainLin.reset (sr, 0.005);
            enable.reset  (sr, 0.030);
            depth.reset   (sr, 0.020);
            bias.reset    (sr, 0.020);
            cutoffsInit = false;
        }

        void resetState()
        {
            bell2.reset(); bell4.reset(); bell8.reset();
            hi2.reset();   hi4.reset();   hi8.reset();
        }

        void pushBellParams (float fc, float widthOct, int shape)
        {
            // The primary bank carries whatever the shape extracts; Notch reads
            // the bell and subtracts, so it configures as band-pass too.
            const auto tA = shape == ShapeLow  ? FilterCascade::LowPass
                          : shape == ShapeHigh ? FilterCascade::HighPass
                          : shape == ShapeTilt ? FilterCascade::LowPass
                                               : FilterCascade::BandPass;
            // Tilt pins both halves to Butterworth so they sum back to the
            // input; every other shape lets the width knob set resonance.
            // Tilt pins both halves to Q = 0.7071: two identical such sections are
            // exactly a squared Butterworth — Linkwitz-Riley 4 — whose halves sum
            // to a magnitude-flat allpass. Every other shape takes resonance from
            // the width knob.
            const double qT = shape == ShapeTilt ? 0.7071 : 0.0;
            bell2.setParams (fc, widthOct, tA, qT);
            bell4.setParams (fc, widthOct, tA, qT);
            bell8.setParams (fc, widthOct, tA, qT);
            if (shape == ShapeTilt)
            {
                hi2.setParams (fc, widthOct, FilterCascade::HighPass, qT);
                hi4.setParams (fc, widthOct, FilterCascade::HighPass, qT);
                hi8.setParams (fc, widthOct, FilterCascade::HighPass, qT);
            }
        }
    };
}
