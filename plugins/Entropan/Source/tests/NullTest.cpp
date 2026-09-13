/*
  Entropan Phase 4.1 gate: null / energy tests for the splitter cascade.

  T1  all bands off                     → output == input (exact wire)
  T2  band 1 on, lift 0                 → RMS preserved (allpass-flat)
  T3  band 1 on, lift 100, depth 0      → RMS preserved (centre pan unity)
  T4  band 1 on, lift 100, depth 100    → energy moves right (pan works)

  Exit code 0 = all pass.
*/

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../AnalyzerResample.h"
#include <cstdio>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlock      = 512;
    constexpr int    kWarmBlocks = 40;    // let 30 ms smoothers settle
    constexpr int    kTestBlocks = 200;   // ~2.1 s of audio

    void setParam (EntropanAudioProcessor& p, const juce::String& id, float denormValue)
    {
        auto* param = p.apvts.getParameter (id);
        jassert (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (denormValue));
    }

    struct Rms { double inL = 0, inR = 0, outL = 0, outR = 0; int n = 0; };

    // Runs warmup + measurement with seeded noise; returns RMS + max wire error.
    Rms run (EntropanAudioProcessor& p, double& maxDiff)
    {
        juce::Random rng (0x51DE);
        juce::AudioBuffer<float> buf (2, kBlock), pre (2, kBlock);
        juce::MidiBuffer midi;
        Rms r;
        maxDiff = 0;

        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int s = 0; s < kBlock; ++s)
                    d[s] = rng.nextFloat() * 2.0f - 1.0f;
            }
            for (int ch = 0; ch < 2; ++ch)
                pre.copyFrom (ch, 0, buf, ch, 0, kBlock);

            p.processBlock (buf, midi);

            if (blk < kWarmBlocks)
                continue;

            for (int s = 0; s < kBlock; ++s)
            {
                const double il = pre.getSample (0, s), ir = pre.getSample (1, s);
                const double ol = buf.getSample (0, s), orr = buf.getSample (1, s);
                r.inL += il * il;  r.inR += ir * ir;
                r.outL += ol * ol; r.outR += orr * orr;
                maxDiff = juce::jmax (maxDiff, std::abs (ol - il), std::abs (orr - ir));
                ++r.n;
            }
        }
        return r;
    }

    double dB (double x) { return 10.0 * std::log10 (juce::jmax (x, 1.0e-30)); }

    bool check (const char* name, bool cond)
    {
        std::printf ("%-44s %s\n", name, cond ? "PASS" : "FAIL");
        return cond;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // message manager for APVTS
    bool ok = true;

    // ── T1: all bands off → exact wire ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 0.0f);
        double maxDiff = 0;
        run (p, maxDiff);
        ok &= check ("T1 all-off null (max |out-in|)", maxDiff < 1.0e-6);
        std::printf ("    maxDiff = %.3g\n", maxDiff);
    }

    // ── T2: band on, lift 0 → allpass-flat energy ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_lift", 0.0f);
        double maxDiff = 0;
        const auto r = run (p, maxDiff);
        const double dL = dB (r.outL / r.inL), dR = dB (r.outR / r.inR);
        ok &= check ("T2 lift-0 energy flat (<0.05 dB)", std::abs (dL) < 0.05 && std::abs (dR) < 0.05);
        std::printf ("    dL = %+.4f dB, dR = %+.4f dB\n", dL, dR);
    }

    // ── T3: lift 100, depth 0 → centre pan unity ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_lift", 100.0f);
        setParam (p, "b1_depth", 0.0f);
        double maxDiff = 0;
        const auto r = run (p, maxDiff);
        const double dL = dB (r.outL / r.inL), dR = dB (r.outR / r.inR);
        ok &= check ("T3 centre-pan energy flat (<0.05 dB)", std::abs (dL) < 0.05 && std::abs (dR) < 0.05);
        std::printf ("    dL = %+.4f dB, dR = %+.4f dB\n", dL, dR);
    }

    // ── T4: in-band sine, mod pinned hard right (sine mode, phase 90°, slow) ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f);
        setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);
        setParam (p, "b1_depth", 100.0f);
        setParam (p, "amount", 100.0f);
        setParam (p, "b1_ratemode", 1.0f);   // free
        setParam (p, "b1_rate", 0.02f);      // ~glacial: sin stays near +1 for the test
        setParam (p, "b1_phase", 90.0f);
        setParam (p, "b1_inertia", 0.0f);

        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 1000.0 / kSampleRate;
        double inE = 0, outLE = 0, outRE = 0;

        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk)
        {
            for (int s = 0; s < kBlock; ++s)
            {
                const float v = (float) std::sin (phase);
                phase += inc;
                buf.setSample (0, s, v);
                buf.setSample (1, s, v);
            }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            for (int s = 0; s < kBlock; ++s)
            {
                inE   += 0.5;   // sine RMS² = 0.5
                outLE += juce::square ((double) buf.getSample (0, s));
                outRE += juce::square ((double) buf.getSample (1, s));
            }
        }
        const double dL = dB ((outLE / inE)), dR = dB ((outRE / inE));
        // hard right: L keeps only filter skirts (≪ −6 dB), R ≈ +3 dB (×√2)
        ok &= check ("T4 in-band sine pans hard right", dL < -6.0 && dR > 2.0 && dR < 4.0);
        std::printf ("    dL = %+.2f dB, dR = %+.2f dB\n", dL, dR);
    }

    // ── T7 (reach-safety): same as T4 but inertia 100% — sine bypasses slew,
    //    swing must still reach hard right (depth independent of inertia) ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f);
        setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);
        setParam (p, "b1_depth", 100.0f);
        setParam (p, "amount", 100.0f);
        setParam (p, "b1_ratemode", 1.0f);
        setParam (p, "b1_rate", 2.0f);        // fast enough that naive slew would shrink the swing
        setParam (p, "b1_inertia", 100.0f);   // ← the whole point

        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 1000.0 / kSampleRate;
        // pan sweeps the full sine, so average energies cancel — instead track
        // per-block L/R balance extremes: both rails must actually be reached.
        double balMin = 1.0e9, balMax = -1.0e9;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk)
        {
            for (int s = 0; s < kBlock; ++s)
            {
                const float v = (float) std::sin (phase);
                phase += inc;
                buf.setSample (0, s, v);
                buf.setSample (1, s, v);
            }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            double eL = 1.0e-12, eR = 1.0e-12;
            for (int s = 0; s < kBlock; ++s)
            {
                eL += juce::square ((double) buf.getSample (0, s));
                eR += juce::square ((double) buf.getSample (1, s));
            }
            const double bal = dB (eR / eL);
            balMin = juce::jmin (balMin, bal);
            balMax = juce::jmax (balMax, bal);
        }
        ok &= check ("T7 full reach at inertia 100%", balMax > 8.0 && balMin < -8.0);
        std::printf ("    balance extremes: %+.1f dB … %+.1f dB\n", balMin, balMax);
    }

    // ── T5: audio-rate sine mod (200 Hz) keeps total energy (equal-power law) ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f);
        setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);
        setParam (p, "b1_depth", 100.0f);
        setParam (p, "b1_ratemode", 1.0f);
        setParam (p, "b1_rate", 200.0f);     // audio-rate panning
        setParam (p, "b1_inertia", 0.0f);
        double maxDiff = 0;
        const auto r = run (p, maxDiff);
        const double dTot = dB ((r.outL + r.outR) / (r.inL + r.inR));
        ok &= check ("T5 audio-rate mod keeps energy (<0.3 dB)", std::abs (dTot) < 0.3);
        std::printf ("    dTotal = %+.4f dB\n", dTot);
    }

    // ── T6: S&H stateless streams → two runs are bit-identical ──
    {
        auto render = [] (std::vector<float>& out)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_mode", 2.0f);       // S&H
            setParam (p, "b1_ratemode", 1.0f);
            setParam (p, "b1_rate", 8.0f);
            setParam (p, "b1_depth", 100.0f);
            juce::Random rng (0xBEEF);
            juce::AudioBuffer<float> buf (2, kBlock);
            juce::MidiBuffer midi;
            for (int blk = 0; blk < 60; ++blk)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* d = buf.getWritePointer (ch);
                    for (int s2 = 0; s2 < kBlock; ++s2)
                        d[s2] = rng.nextFloat() * 2.0f - 1.0f;
                }
                p.processBlock (buf, midi);
                for (int s2 = 0; s2 < kBlock; ++s2)
                    out.push_back (buf.getSample (0, s2));
            }
        };
        std::vector<float> a, b;
        render (a); render (b);
        ok &= check ("T6 S&H runs are deterministic", a == b);
    }

    // ── T8: Steps engine — pattern of all +1 pins hard right (parse + clock) ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        p.setStepsJson (0, R"({"count":4,"steps":[{"subdiv":1,"vals":[1]},{"subdiv":2,"vals":[1,1]},{"subdiv":1,"vals":[1]},{"subdiv":4,"vals":[1,1,1,1]}]})");
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f);
        setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);
        setParam (p, "b1_depth", 100.0f);
        setParam (p, "b1_mode", 4.0f);       // Steps
        setParam (p, "b1_ratemode", 1.0f);
        setParam (p, "b1_rate", 2.0f);
        setParam (p, "b1_inertia", 0.0f);

        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 1000.0 / kSampleRate;
        double inE = 0, outLE = 0, outRE = 0;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk)
        {
            for (int s = 0; s < kBlock; ++s)
            {
                const float v = (float) std::sin (phase);
                phase += inc;
                buf.setSample (0, s, v); buf.setSample (1, s, v);
            }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            for (int s = 0; s < kBlock; ++s)
            {
                inE += 0.5;
                outLE += juce::square ((double) buf.getSample (0, s));
                outRE += juce::square ((double) buf.getSample (1, s));
            }
        }
        const double dL = dB (outLE / inE), dR = dB (outRE / inE);
        ok &= check ("T8 steps pattern drives pan", dL < -6.0 && dR > 2.0 && dR < 4.0);
        std::printf ("    dL = %+.2f dB, dR = %+.2f dB\n", dL, dR);
    }

    // ── T9: CPU — 6 bands, mixed modes, 10 s @48k. Informational + ceiling. ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        for (int i = 0; i < 6; ++i)
        {
            const juce::String b = "b" + juce::String (i + 1) + "_";
            setParam (p, b + "on", 1.0f);
            setParam (p, b + "mode", (float) i);          // sine, tri, s&h, chaos, steps, env
            setParam (p, b + "ratemode", 1.0f);
            setParam (p, b + "rate", i == 0 ? 200.0f : 2.0f);  // one audio-rate band
            setParam (p, b + "depth", 100.0f);
        }
        juce::Random rng (7);
        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        const int blocks = (int) (10.0 * kSampleRate / kBlock);
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int blk = 0; blk < blocks; ++blk)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int s2 = 0; s2 < kBlock; ++s2)
                    d[s2] = rng.nextFloat() * 2.0f - 1.0f;
            }
            p.processBlock (buf, midi);
        }
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double rtPct = secs / 10.0 * 100.0;
        ok &= check ("T9 CPU 6 bands (< 25% realtime)", rtPct < 25.0);
        std::printf ("    %.2f%% of realtime (%.1f ms for 10 s)\n", rtPct, secs * 1000.0);
    }

    // ── T10: undo / redo restores full state (params + steps) ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        auto depth = [&] { return p.apvts.getParameter ("b1_depth")->getValue(); };

        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_depth", 50.0f);
        p.commitUndoIfChanged();                 // baseline: depth 50
        const float d50 = depth();

        setParam (p, "b1_depth", 90.0f);
        p.commitUndoIfChanged();                 // edit: depth 90
        const float d90 = depth();

        const bool u1 = p.undoState();           // → back to 50
        const float afterUndo = depth();
        const bool r1 = p.redoState();           // → forward to 90
        const float afterRedo = depth();

        const bool pass = u1 && r1
            && std::abs (afterUndo - d50) < 1.0e-4f
            && std::abs (afterRedo - d90) < 1.0e-4f
            && p.canUndo();                       // 50-checkpoint still available
        ok &= check ("T10 undo/redo restores state", pass);
        std::printf ("    d50=%.3f d90=%.3f undo=%.3f redo=%.3f\n", d50, d90, afterUndo, afterRedo);
    }

    // ── T11: editor-size changes must NOT create undo steps; session load resets history ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        p.commitUndoIfChanged();                          // settle baseline
        const bool undoBefore = p.canUndo();

        p.apvts.state.setProperty ("editorWidth", 1200, nullptr);   // simulate resize
        p.apvts.state.setProperty ("editorHeight", 747, nullptr);
        p.commitUndoIfChanged();
        const bool resizeMadeStep = p.canUndo() != undoBefore;      // must stay false

        setParam (p, "b1_depth", 77.0f);                            // real edit
        p.commitUndoIfChanged();
        const bool editMadeStep = p.canUndo();

        juce::MemoryBlock state;
        p.getStateInformation (state);
        p.setStateInformation (state.getData(), (int) state.getSize());  // host load
        const bool historyCleared = ! p.canUndo() && ! p.canRedo();

        ok &= check ("T11 undo ignores resize; load resets", ! resizeMadeStep && editMadeStep && historyCleared);
    }

    // ── T12: wow/flutter at 0 stays null; engaged, it changes the signal ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        double md = 0; run (p, md);                          // all default (wf 0)
        const bool nullOff = md < 1.0e-6;
        setParam (p, "wow", 60.0f); setParam (p, "flutter", 40.0f);
        double md2 = 0; run (p, md2);
        ok &= check ("T12 wow/flutter: off=null, on=active", nullOff && md2 > 1.0e-3);
        std::printf ("    off maxDiff=%.2g, on maxDiff=%.3f\n", md, md2);
    }

    // ── T13: Env mode — steady loud in-band tone drives the pan right ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 100.0f);
        setParam (p, "b1_mode", 5.0f);    // Env
        setParam (p, "env_atk", 5.0f);    setParam (p, "env_rel", 50.0f);
        setParam (p, "b1_inertia", 0.0f);

        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        double outLE = 0, outRE = 0;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
            for (int s = 0; s < kBlock; ++s) { const float v = (float) std::sin (phase); phase += inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            for (int s = 0; s < kBlock; ++s) { outLE += juce::square((double)buf.getSample(0,s)); outRE += juce::square((double)buf.getSample(1,s)); }
        }
        ok &= check ("T13 env-follower pans loud signal right", outRE > outLE * 2.0);
        std::printf ("    R/L energy ratio = %.2f\n", outRE / juce::jmax(1e-9, outLE));
    }

    // ── T14: unipolar modulator biases the pan to one side (never crosses centre) ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 100.0f);
        setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 0.5f);
        setParam (p, "b1_inertia", 0.0f);  setParam (p, "b1_uni", 1.0f);   // unipolar
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        double outLE = 0, outRE = 0;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            for (int s = 0; s < kBlock; ++s) { outLE += juce::square((double)buf.getSample(0,s)); outRE += juce::square((double)buf.getSample(1,s)); }
        }
        ok &= check ("T14 unipolar biases pan to one side", outRE > outLE * 1.08);
        std::printf ("    R/L energy = %.2f (bipolar would be ~1)\n", outRE / juce::jmax(1e-9, outLE));
    }

    // ── T15: bias offsets the resting pan (depth 0 → pan pinned at bias) ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 0.0f);   // no swing
        setParam (p, "b1_bias", 100.0f);  // hard right
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        double outLE = 0, outRE = 0;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            for (int s = 0; s < kBlock; ++s) { outLE += juce::square((double)buf.getSample(0,s)); outRE += juce::square((double)buf.getSample(1,s)); }
        }
        ok &= check ("T15 bias pins pan (depth 0, bias +100)", dB(outLE/(outLE+outRE)) < -12.0 && outRE > outLE * 10.0);
        std::printf ("    R/L = %.1f\n", outRE / juce::jmax(1e-9, outLE));
    }

    // ── T16: bias limited to headroom — full swing + max bias never clips a rail flat ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 100.0f);   // full swing → headroom 0
        setParam (p, "b1_bias", 100.0f);  // request hard right; must be limited to ~0
        setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 2.0f); setParam (p, "b1_inertia", 0.0f);
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        double balMin = 1e9, balMax = -1e9;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            double eL=1e-12,eR=1e-12;
            for (int s = 0; s < kBlock; ++s){ eL+=juce::square((double)buf.getSample(0,s)); eR+=juce::square((double)buf.getSample(1,s)); }
            const double b = dB(eR/eL); balMin=juce::jmin(balMin,b); balMax=juce::jmax(balMax,b);
        }
        // depth 100 + amount 100 → headroom 0 → bias forced to centre → symmetric swing, both rails
        ok &= check ("T16 bias limited to headroom (both rails, symmetric)", balMax > 8.0 && balMin < -8.0);
        std::printf ("    balance extremes: %+.1f … %+.1f dB\n", balMin, balMax);
    }

    // ── T18: override drops the headroom clamp — full depth + full bias stays one-sided ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 100.0f);   // same as T16…
        setParam (p, "b1_bias", 100.0f);
        setParam (p, "b1_override", 1.0f);   // …but override → bias NOT clamped → hard-right, wobble off the rail
        setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 2.0f); setParam (p, "b1_inertia", 0.0f);
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        double balMin = 1e9, balMax = -1e9;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            double eL=1e-12,eR=1e-12;
            for (int s = 0; s < kBlock; ++s){ eL+=juce::square((double)buf.getSample(0,s)); eR+=juce::square((double)buf.getSample(1,s)); }
            const double b = dB(eR/eL); balMin=juce::jmin(balMin,b); balMax=juce::jmax(balMax,b);
        }
        // biased hard right: reaches the right rail (balMax high) but never the left (balMin stays ≳ 0),
        // where T16 with the same depth/bias was symmetric (balMin < -8).
        ok &= check ("T18 override → one-sided (right rail only)", balMax > 8.0 && balMin > -3.0);
        std::printf ("    balance extremes: %+.1f … %+.1f dB\n", balMin, balMax);
    }

    // ── T17: CHAOS locks — persist with the session, but stay out of undo ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        const juce::String locks = R"([["freq","rate"],[],["mode"],[],[],[]])";

        p.commitUndoIfChanged();                       // settle baseline
        const bool undoBefore = p.canUndo();

        p.setLocksJson (locks);                        // user locks a couple of params
        p.commitUndoIfChanged();
        const bool lockMadeUndoStep = p.canUndo() != undoBefore;   // must stay false
        const bool roundTrip = p.getLocksJson() == locks;

        // locks must survive an undo of a real edit
        setParam (p, "b1_depth", 33.0f);
        p.commitUndoIfChanged();
        const bool undid = p.undoState();
        const bool locksSurviveUndo = p.getLocksJson() == locks;
        const bool locksSurviveRedo = p.redoState() && p.getLocksJson() == locks;

        // and must ride along with session save/load
        juce::MemoryBlock state;
        p.getStateInformation (state);
        EntropanAudioProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        const bool locksPersist = q.getLocksJson() == locks;

        ok &= check ("T17 locks persist, excluded from undo",
                     ! lockMadeUndoStep && roundTrip && undid
                     && locksSurviveUndo && locksSurviveRedo && locksPersist);
        std::printf ("    undoStep=%d roundTrip=%d undo=%d redo=%d persist=%d\n",
                     (int) lockMadeUndoStep, (int) roundTrip, (int) locksSurviveUndo,
                     (int) locksSurviveRedo, (int) locksPersist);
    }

    // ── T19: step SMOOTH glides the pan — smooth 100 differs from square, stays finite ──
    {
        auto runSteps = [] (float smooth)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
            setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 100.0f);
            setParam (p, "b1_mode", 4.0f);           // Steps
            setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 6.0f);
            setParam (p, "b1_stepsmooth", smooth);
            // alternating hard L/R step pattern → maximal square jumps to smooth
            p.setStepsJson (0, "{\"count\":4,\"steps\":["
                               "{\"subdiv\":1,\"vals\":[1.0]},{\"subdiv\":1,\"vals\":[-1.0]},"
                               "{\"subdiv\":1,\"vals\":[1.0]},{\"subdiv\":1,\"vals\":[-1.0]}]}");
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
            std::vector<float> capL;
            for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                for (int s = 0; s < kBlock; ++s) capL.push_back (buf.getSample (0, s));
            }
            return capL;
        };
        const auto square = runSteps (0.0f);
        const auto glide  = runSteps (100.0f);
        bool finite = true; double maxDiff = 0.0;
        for (size_t k = 0; k < square.size(); ++k) {
            finite = finite && std::isfinite (square[k]) && std::isfinite (glide[k]);
            maxDiff = juce::jmax (maxDiff, (double) std::abs (square[k] - glide[k]));
        }
        ok &= check ("T19 step SMOOTH glides pan (finite, differs from square)", finite && maxDiff > 0.05);
        std::printf ("    max |square − glide| = %.3f\n", maxDiff);
    }

    // ── T20: Parallel routing — a single band still pans equal-power (like T4) ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "routing", 1.0f);       // Parallel
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 100.0f);
        setParam (p, "amount", 100.0f);
        setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 0.02f);
        setParam (p, "b1_phase", 90.0f);   setParam (p, "b1_inertia", 0.0f);
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        double inE = 0, outLE = 0, outRE = 0;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            for (int s = 0; s < kBlock; ++s) { inE += 0.5; outLE += juce::square((double)buf.getSample(0,s)); outRE += juce::square((double)buf.getSample(1,s)); }
        }
        const double dL = dB (outLE/inE), dR = dB (outRE/inE);
        ok &= check ("T20 parallel: single band pans hard right", dL < -6.0 && dR > 2.0 && dR < 4.0);
        std::printf ("    dL = %+.2f dB, dR = %+.2f dB\n", dL, dR);
    }

    // ── T21: overlapping bands route differently — Serial ≠ Parallel ──
    {
        auto run2 = [] (float routing, std::vector<float>& outL)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "routing", routing);
            for (int b = 1; b <= 2; ++b) {   // two bands on the SAME region, opposite bias
                const juce::String pre = "b" + juce::String (b) + "_";
                setParam (p, pre + "on", 1.0f);
                setParam (p, pre + "freq", 1000.0f); setParam (p, pre + "width", 2.0f);
                setParam (p, pre + "lift", 100.0f);  setParam (p, pre + "depth", 0.0f);
                setParam (p, pre + "bias", b == 1 ? 100.0f : -100.0f);   // one hard R, one hard L
            }
            setParam (p, "amount", 100.0f);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
            for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                for (int s = 0; s < kBlock; ++s) outL.push_back (buf.getSample (0, s));
            }
        };
        std::vector<float> ser, par, def;
        run2 (0.0f, ser); run2 (1.0f, par);
        // default (routing param left unset) must equal explicit Serial
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            for (int b = 1; b <= 2; ++b) {
                const juce::String pre = "b" + juce::String (b) + "_";
                setParam (p, pre + "on", 1.0f);
                setParam (p, pre + "freq", 1000.0f); setParam (p, pre + "width", 2.0f);
                setParam (p, pre + "lift", 100.0f);  setParam (p, pre + "depth", 0.0f);
                setParam (p, pre + "bias", b == 1 ? 100.0f : -100.0f);
            }
            setParam (p, "amount", 100.0f);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
            for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                for (int s = 0; s < kBlock; ++s) def.push_back (buf.getSample (0, s));
            }
        }
        double maxDiff = 0.0, defVsSerial = 0.0;
        for (size_t k = 0; k < ser.size(); ++k) {
            maxDiff     = juce::jmax (maxDiff,     (double) std::abs (ser[k] - par[k]));
            defVsSerial = juce::jmax (defVsSerial, (double) std::abs (ser[k] - def[k]));
        }
        ok &= check ("T21 overlap: serial ≠ parallel, default == serial", maxDiff > 0.05 && defVsSerial < 1.0e-6);
        std::printf ("    max |serial − parallel| = %.3f, |default − serial| = %.2g\n", maxDiff, defVsSerial);
    }

    // ── T22: glued steps hold one value across the run ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 100.0f);
        setParam (p, "amount", 100.0f);
        setParam (p, "b1_mode", 4.0f);           // Steps
        setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 4.0f);
        // 4 cells: hard-right leader glued over cells 2-3-4, so the whole cycle = +1 (hard right)
        p.setStepsJson (0, "{\"count\":4,\"steps\":["
                           "{\"subdiv\":1,\"vals\":[1.0]},"
                           "{\"subdiv\":1,\"vals\":[-1.0],\"tie\":true},"
                           "{\"subdiv\":1,\"vals\":[-1.0],\"tie\":true},"
                           "{\"subdiv\":1,\"vals\":[-1.0],\"tie\":true}]}");
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        double inE = 0, outLE = 0, outRE = 0;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            for (int s = 0; s < kBlock; ++s) { inE += 0.5; outLE += juce::square((double)buf.getSample(0,s)); outRE += juce::square((double)buf.getSample(1,s)); }
        }
        const double dL = dB (outLE/inE), dR = dB (outRE/inE);
        // the tied cells ignore their own −1 and hold the leader's +1 → pan stays hard right
        ok &= check ("T22 glued run holds leader value (hard right)", dL < -6.0 && dR > 2.0 && dR < 4.0);
        std::printf ("    dL = %+.2f dB, dR = %+.2f dB\n", dL, dR);
    }

    // ── T23: serial↔parallel flip mid-stream is crossfaded — no pop, finite ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        for (int b = 1; b <= 2; ++b) {   // overlapping bands so the topologies really differ
            const juce::String pre = "b" + juce::String (b) + "_";
            setParam (p, pre + "on", 1.0f);
            setParam (p, pre + "freq", 1000.0f); setParam (p, pre + "width", 2.0f);
            setParam (p, pre + "lift", 100.0f);  setParam (p, pre + "depth", 0.0f);
            setParam (p, pre + "bias", b == 1 ? 100.0f : -100.0f);
        }
        setParam (p, "amount", 100.0f);
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        bool finite = true; double maxJump = 0.0; float prevL = 0.0f; bool havePrev = false;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
            if (blk == kWarmBlocks + 40)  setParam (p, "routing", 1.0f);   // flip mid-stream…
            if (blk == kWarmBlocks + 120) setParam (p, "routing", 0.0f);   // …and back
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) { havePrev = false; continue; }
            for (int s = 0; s < kBlock; ++s) {
                const float l = buf.getSample (0, s);
                finite = finite && std::isfinite (l) && std::isfinite (buf.getSample (1, s));
                if (havePrev) maxJump = juce::jmax (maxJump, (double) std::abs (l - prevL));
                prevL = l; havePrev = true;
            }
        }
        // 1 kHz sine per-sample delta ≈ 0.13·amp (amp ≤ √2 here) → intrinsic ≲ 0.19;
        // a hard topology swap added a step on top. Crossfaded must stay near intrinsic.
        ok &= check ("T23 routing flip crossfades (finite, no pop)", finite && maxJump < 0.35);
        std::printf ("    max |Δsample| across flips = %.3f\n", maxJump);
    }

    // ── T24: mod matrix — B1's modulator routed to global OUTPUT raises the level ──
    {
        auto runOut = [] (bool withRoute)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
            setParam (p, "b1_lift", 0.0f);        // allpass-flat: pan path out of the picture
            setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 0.02f);
            setParam (p, "b1_phase", 90.0f);   setParam (p, "b1_inertia", 0.0f);   // sine pinned ≈ +1
            if (withRoute)   // src band 0 → dst 63 (global output), +100%
                p.setRoutesJson ("{\"routes\":[{\"src\":0,\"dst\":63,\"depth\":100}]}");
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
            double inE = 0, outE = 0;
            for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                for (int s = 0; s < kBlock; ++s) { inE += 0.5; outE += juce::square((double)buf.getSample(0,s)); }
            }
            return dB (outE / inE);
        };
        const double base = runOut (false), modded = runOut (true);
        // output default 0 dB, norm 24/36; mod +1 → norm clamps to 1.0 → +12 dB
        ok &= check ("T24 mod route drives OUTPUT (+12 dB)", std::abs (base) < 0.2 && modded > 11.0 && modded < 12.6);
        std::printf ("    base = %+.2f dB, routed = %+.2f dB\n", base, modded);
    }

    // ── T25: route polarity follows the source band's BI/UNI toggle ──
    //   UNI → one-way from the knob (level never dips BELOW its unmodulated 0 dB)
    //   BI  → swings both ways (level must dip below as the modulator goes negative)
    {
        auto runPolarity = [] (float uni, double& lo, double& hi)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
            setParam (p, "b1_lift", 0.0f);          // allpass-flat: only the routed LEVEL moves
            setParam (p, "b1_uni", uni);
            setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 2.0f);
            setParam (p, "b1_inertia", 0.0f);
            p.setRoutesJson ("{\"routes\":[{\"src\":0,\"dst\":63,\"depth\":100}]}");   // → global OUTPUT
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
            lo = 1.0e9; hi = -1.0e9;
            for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                double e = 1.0e-12;
                for (int s = 0; s < kBlock; ++s) e += juce::square ((double) buf.getSample (0, s));
                const double d = dB (e / (0.5 * kBlock));    // vs the dry sine's own energy
                lo = juce::jmin (lo, d); hi = juce::jmax (hi, d);
            }
        };
        double uLo = 0, uHi = 0, bLo = 0, bHi = 0;
        runPolarity (1.0f, uLo, uHi);   // UNI
        runPolarity (0.0f, bLo, bHi);   // BI
        ok &= check ("T25 UNI one-way / BI bipolar (level floor)", uLo > -0.5 && uHi > 6.0 && bLo < -6.0);
        std::printf ("    UNI %.2f … %+.2f dB   BI %.2f … %+.2f dB\n", uLo, uHi, bLo, bHi);
    }

    // ── T26: Env pan slew floor — no audio-rate ripple AM (was: noise at high ENV GAIN) ──
    // The detector ripples at 2× the programme frequency; before the floor,
    // smooth=0 made the pan track that ripple per-sample, which reads here as
    // huge total variation of the short-window L/R balance. Measure the balance
    // trajectory's total variation on a steady 200 Hz sine with a hot detector.
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 200.0f); setParam (p, "b1_width", 3.0f);
        setParam (p, "b1_lift", 100.0f); setParam (p, "b1_depth", 100.0f);
        setParam (p, "b1_mode", 5.0f);                      // Env
        setParam (p, "b1_inertia", 0.0f);                   // worst case: zero smooth
        setParam (p, "env_atk", 1.0f); setParam (p, "env_rel", 5.0f);
        // Detector must sit MID-RANGE: a hot signal pins globalEnv at the 1.0
        // clamp (constant target, no ripple, test proves nothing). The user
        // hears the noise when GAIN pushes programme through this ripple zone;
        // a quiet sine parks the detector inside it deterministically.
        setParam (p, "env_gain", 0.0f);
        constexpr float kAmp = 0.15f;
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*200.0/kSampleRate;
        double tv = 0.0, prevBal = 0.0; bool have = false;
        constexpr int kWin = 32;
        for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
            for (int s = 0; s < kBlock; ++s) { const float v=kAmp*(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            for (int w = 0; w + kWin <= kBlock; w += kWin) {
                double el = 1.0e-12, er = 1.0e-12;
                for (int s = w; s < w + kWin; ++s) { el += juce::square ((double) buf.getSample (0, s));
                                                     er += juce::square ((double) buf.getSample (1, s)); }
                const double bal = (el - er) / (el + er);
                if (have) tv += std::abs (bal - prevBal);
                prevBal = bal; have = true;
            }
        }
        // measured: 108.1 without the floor (per-sample ripple tracking, audible
        // ~−28 dB sidebands), 7.6 with it (residual ~−54 dB, inaudible — a one-
        // pole slew attenuates, it can't null). 15 = 2× margin over fixed, 7×
        // below broken.
        ok &= check ("T26 Env slew floor kills ripple AM", tv < 15.0);
        std::printf ("    balance total variation: %.3f (floor active)\n", tv);
    }

    // ── T27: waveforms are independent sources — a SINE route swings while the
    //         band's own MODE is Steps with a flat pattern (pan engine idle) ──
    // Also proves the legacy path: the same route WITHOUT stype follows the
    // band's mode (flat steps) and must NOT move the level.
    {
        auto runStype = [] (const char* routesJson, double& lo, double& hi)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
            setParam (p, "b1_lift", 0.0f);          // allpass-flat: only the routed LEVEL moves
            setParam (p, "b1_mode", 4.0f);          // Steps…
            p.setStepsJson (0, "{\"count\":4,\"steps\":[{\"subdiv\":1,\"vals\":[0]},{\"subdiv\":1,\"vals\":[0]},{\"subdiv\":1,\"vals\":[0]},{\"subdiv\":1,\"vals\":[0]}]}");   // …all flat
            setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 2.0f);
            setParam (p, "b1_inertia", 0.0f);
            p.setRoutesJson (routesJson);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
            lo = 1.0e9; hi = -1.0e9;
            for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                double e = 1.0e-12;
                for (int s = 0; s < kBlock; ++s) e += juce::square ((double) buf.getSample (0, s));
                const double d = dB (e / (0.5 * kBlock));
                lo = juce::jmin (lo, d); hi = juce::jmax (hi, d);
            }
        };
        double sLo = 0, sHi = 0, fLo = 0, fHi = 0;
        // explicit SINE waveform → swings ±12 dB even though the band's MODE is flat Steps
        runStype ("{\"routes\":[{\"src\":0,\"stype\":0,\"dst\":63,\"depth\":100}]}", sLo, sHi);
        // no stype → follows the band's MODE (flat steps) → level must stay put
        runStype ("{\"routes\":[{\"src\":0,\"dst\":63,\"depth\":100}]}", fLo, fHi);
        ok &= check ("T27 stype independent of band mode", sHi > 6.0 && sLo < -6.0
                                                        && std::abs (fLo) < 0.5 && std::abs (fHi) < 0.5);
        std::printf ("    SINE route %.2f … %+.2f dB   follow-mode(flat) %.2f … %+.2f dB\n", sLo, sHi, fLo, fHi);
    }

    // ── T28: self-feedback routes are refused — band N → its own RATE is a
    //         no-op (identical output to no route), cross-band rate mod works ──
    {
        auto runRate = [] (const char* routesJson, std::vector<double>& blocks)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
            setParam (p, "b1_lift", 100.0f); setParam (p, "b1_depth", 100.0f);
            setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 2.0f);
            setParam (p, "b1_inertia", 0.0f);
            // band 2: enabled but allpass-flat (lift 0) — inaudible itself, its
            // modulator ticks, so a cross-band route has a live source
            setParam (p, "b2_on", 1.0f); setParam (p, "b2_lift", 0.0f);
            setParam (p, "b2_ratemode", 1.0f); setParam (p, "b2_rate", 0.7f);
            if (routesJson != nullptr) p.setRoutesJson (routesJson);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
            blocks.clear();
            for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                double e = 1.0e-12;
                for (int s = 0; s < kBlock; ++s) e += juce::square ((double) buf.getSample (0, s));
                blocks.push_back (dB (e / (0.5 * kBlock)));
            }
        };
        std::vector<double> base, self, cross;
        runRate (nullptr, base);
        runRate ("{\"routes\":[{\"src\":0,\"stype\":0,\"dst\":5,\"depth\":100}]}", self);    // b1 → OWN rate: refused
        runRate ("{\"routes\":[{\"src\":1,\"stype\":0,\"dst\":5,\"depth\":100}]}", cross);   // b2 → b1 rate: legal
        double dSelf = 0, dCross = 0;
        for (size_t k = 0; k < base.size(); ++k) {
            dSelf  = juce::jmax (dSelf,  std::abs (self[k]  - base[k]));
            dCross = juce::jmax (dCross, std::abs (cross[k] - base[k]));
        }
        // self must be a byte-level no-op (route dropped at parse); cross must
        // actually diverge (route accepted, live source) — proves the filter
        // rejects exactly the self case, not the whole matrix.
        ok &= check ("T28 self rate-mod refused, cross-band legal", dSelf < 1.0e-9 && dCross > 0.5);
        std::printf ("    max block diff: self %.3g dB, cross %.2f dB\n", dSelf, dCross);
    }

    // ── T29: FREEZE pauses the band's modulation — balance holds still while
    //         frozen, moves again on release ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 100.0f);
        setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 2.0f);   // 2 Hz sine sweep
        setParam (p, "b1_inertia", 0.0f);
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        auto runPhase = [&] (int blocks, double& swing)
        {
            double mn = 1.0e9, mx = -1.0e9;
            for (int blk = 0; blk < blocks; ++blk) {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                double el = 1.0e-12, er = 1.0e-12;
                for (int s = 0; s < kBlock; ++s) { el += juce::square ((double) buf.getSample (0, s));
                                                   er += juce::square ((double) buf.getSample (1, s)); }
                const double bal = (el - er) / (el + er);
                mn = juce::jmin (mn, bal); mx = juce::jmax (mx, bal);
            }
            swing = mx - mn;
        };
        double swimBefore = 0, swimFrozen = 0, swimAfter = 0;
        runPhase (kWarmBlocks + 100, swimBefore);          // free-running: full sweep
        setParam (p, "b1_freeze", 1.0f);
        runPhase (10, swimFrozen); runPhase (100, swimFrozen);   // settle a beat, then measure
        setParam (p, "b1_freeze", 0.0f);
        runPhase (100, swimAfter);
        // 2 Hz over ~1 s sweeps the full L↔R range; frozen must sit dead still
        ok &= check ("T29 freeze holds, release resumes", swimBefore > 1.0 && swimFrozen < 0.01 && swimAfter > 1.0);
        std::printf ("    balance swing: before %.2f, frozen %.4f, after %.2f\n", swimBefore, swimFrozen, swimAfter);
    }

    // ── T30: the slope MORPH reconstructs null-clean everywhere — detents
    //         exact, mid-blend positions within the small allpass-shape ripple ──
    {
        for (float slope : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_slope", slope);
            setParam (p, "b1_lift", 0.0f);
            double maxDiff = 0;
            const auto r0 = run (p, maxDiff);
            const double f0L = dB (r0.outL / r0.inL), f0R = dB (r0.outR / r0.inR);

            EntropanAudioProcessor q;
            q.prepareToPlay (kSampleRate, kBlock);
            setParam (q, "b1_on", 1.0f);
            setParam (q, "b1_slope", slope);
            setParam (q, "b1_lift", 100.0f); setParam (q, "b1_depth", 0.0f);
            const auto r1 = run (q, maxDiff);
            const double c0L = dB (r1.outL / r1.inL), c0R = dB (r1.outR / r1.inR);

            // detents (0/50/100) must be exact; mid-blends tolerate the measured
            // same-order allpass shape difference (< 0.5 dB)
            const bool detent = slope == 0.0f || slope == 50.0f || slope == 100.0f;
            const double tol = detent ? 0.05 : 0.5;
            const bool pass = std::abs (f0L) < tol && std::abs (f0R) < tol
                           && std::abs (c0L) < tol && std::abs (c0R) < tol;
            char name[64];
            std::snprintf (name, sizeof (name), "T30 slope %3.0f%% null-clean", slope);
            ok &= check (name, pass);
            std::printf ("    lift0 %+0.4f/%+0.4f dB   centre %+0.4f/%+0.4f dB\n", f0L, f0R, c0L, c0R);
        }
    }

    // ── T32: the analyzer renders LOW tones as needles — ≤ 8 of 256 columns
    //         (≈0.3 oct) above −50 dB for 40 / 80 / 237 Hz, via the SHARED
    //         resample math the editor compiles (AnalyzerResample.h) ──
    {
        using namespace EntropanAnalyzer;
        const double sr = 48000.0;
        std::vector<ColGeom> geom ((size_t) kBins);
        buildGeometry (sr, geom.data());
        juce::dsp::FFT fftS (kSmallOrder), fftB (kBigOrder);
        juce::dsp::WindowingFunction<float> winS ((size_t) kSmallSize, juce::dsp::WindowingFunction<float>::blackmanHarris);
        juce::dsp::WindowingFunction<float> winB ((size_t) kBigSize,  juce::dsp::WindowingFunction<float>::blackmanHarris);
        bool allNarrow = true;
        for (double hz : { 40.0, 80.0, 237.0 })
        {
            std::vector<float> big ((size_t) kBigSize * 2, 0.0f), small ((size_t) kSmallSize * 2, 0.0f);
            for (int n = 0; n < kBigSize; ++n)
                big[(size_t) n] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * n / sr);
            std::memcpy (small.data(), big.data() + (kBigSize - kSmallSize), sizeof (float) * kSmallSize);
            winS.multiplyWithWindowingTable (small.data(), kSmallSize);
            winB.multiplyWithWindowingTable (big.data(), kBigSize);
            fftS.performFrequencyOnlyForwardTransform (small.data());
            fftB.performFrequencyOnlyForwardTransform (big.data());
            float cols[kBins];
            computeColumns (geom.data(), small.data(), big.data(), cols);
            float pk = -120; for (int b = 0; b < kBins; ++b) pk = juce::jmax (pk, cols[b]);
            int wide = 0; for (int b = 0; b < kBins; ++b) if (cols[b] > pk - 50.0f) ++wide;
            std::printf ("    %4.0f Hz: %d columns above peak-50 dB\n", hz, wide);
            allNarrow = allNarrow && wide <= 8;
        }
        ok &= check ("T32 analyzer LF needles (multires)", allNarrow);
    }

    // ── T33: sweeping SLOPE during audio must not click — static-centre band,
    //         host sweeps the slope param 0→1 normalized; the output's max
    //         sample-to-sample step must stay near the clean sine's own ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
        setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", 0.0f);   // static centre: output = allpassed sine
        auto* slopeParam = p.apvts.getParameter ("b1_slope");
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        float prev = 0.0f; double maxStep = 0.0; bool have = false;
        const int sweepBlocks = 400;   // ~4.3 s, slope swept end to end and back
        for (int blk = 0; blk < kWarmBlocks + sweepBlocks; ++blk)
        {
            if (blk >= kWarmBlocks)
            {
                const double t = (double) (blk - kWarmBlocks) / (double) sweepBlocks;
                const double tri = t < 0.5 ? t * 2.0 : 2.0 - t * 2.0;   // 0→1→0
                slopeParam->setValueNotifyingHost ((float) tri);
            }
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) { prev = buf.getSample (0, kBlock - 1); have = true; continue; }
            for (int s = 0; s < kBlock; ++s)
            {
                const float v = buf.getSample (0, s);
                if (have) maxStep = juce::jmax (maxStep, (double) std::abs (v - prev));
                prev = v; have = true;
            }
        }
        // clean 1 kHz sine max step = 2π·1000/48000 ≈ 0.131; allow smoothing
        // headroom. A filter-reset click is an impulse — far above this bound.
        ok &= check ("T33 slope sweep is click-free", maxStep < 0.20);
        std::printf ("    max sample step during sweep: %.3f (clean sine ≈ 0.131)\n", maxStep);
    }

    // ── T34: a NARROW bell hard-pans its centre — the user's exact scenario
    //         (sine at fc, bell tight around it, lift/depth/amount 100) must
    //         reach ≥0.97 balance at every slope ──
    {
        bool allReach = true;
        for (float slope : { 0.0f, 50.0f, 100.0f })
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 299.0f);
            setParam (p, "b1_width", 0.2f);      // Q ≈ 7.2 — the screenshot patch
            setParam (p, "b1_lift", 100.0f); setParam (p, "b1_depth", 100.0f);
            setParam (p, "b1_slope", slope);
            setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 0.5f);
            setParam (p, "b1_inertia", 0.0f);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*299.0/kSampleRate;
            double mx = 0;
            for (int blk = 0; blk < kWarmBlocks + kTestBlocks; ++blk) {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                double el = 1e-12, er = 1e-12;
                for (int s = 0; s < kBlock; ++s) { el += juce::square ((double) buf.getSample (0, s));
                                                   er += juce::square ((double) buf.getSample (1, s)); }
                mx = juce::jmax (mx, std::abs ((el - er) / (el + er)));
            }
            std::printf ("    slope %3.0f%%: narrow-bell max |balance| %.3f\n", slope, mx);
            allReach = allReach && mx >= 0.97;
        }
        ok &= check ("T34 narrow bell hard-pans its centre", allReach);
    }

    // ── T35: sweeping WOW/FLUTTER during audio must not click — the delay-tap
    //         depth must move smoothly, not jump per block ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 0.0f);            // W&F is global — bands not needed
        auto* wow = p.apvts.getParameter ("wow");
        auto* flt = p.apvts.getParameter ("flutter");
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        float prev = 0.0f; double maxStep = 0.0; bool have = false; int maxBlk = -1, maxS = -1;
        const int sweepBlocks = 400;
        for (int blk = 0; blk < kWarmBlocks + sweepBlocks; ++blk)
        {
            if (blk >= kWarmBlocks)
            {
                const double t = (double) (blk - kWarmBlocks) / (double) sweepBlocks;
                const double tri = t < 0.5 ? t * 2.0 : 2.0 - t * 2.0;   // 0→1→0
                wow->setValueNotifyingHost ((float) tri);
                flt->setValueNotifyingHost ((float) (1.0 - tri));
            }
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) { prev = buf.getSample (0, kBlock - 1); have = true; continue; }
            for (int s = 0; s < kBlock; ++s)
            {
                const float v = buf.getSample (0, s);
                if (have && std::abs (v - prev) > maxStep) { maxStep = std::abs (v - prev); maxBlk = blk; maxS = s; }
                prev = v; have = true;
            }
        }
        // clean 1 kHz sine step ≈ 0.131; the wobble modulates pitch slightly —
        // allow headroom. A per-block tap jump is an impulse, far above this.
        std::printf ("    argmax: block %d sample %d (sweep t=%.3f)\n", maxBlk, maxS, (double) (maxBlk - kWarmBlocks) / 400.0);
        ok &= check ("T35 wow/flutter sweep is click-free", maxStep < 0.25);
        std::printf ("    max sample step during W&F sweep: %.3f (clean sine ≈ 0.131)\n", maxStep);
    }

    // ── T36: slope sweep is click-free even at MINIMUM width on a LOW bell —
    //         guards the B90 out-of-zone cascade gate (a cold resume of the
    //         steep 8-section cascade is the worst case; if the dormant-reset
    //         is wrong this thumps, exactly like the W&F engage bug T35 caught) ──
    {
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 120.0f);   // low → longest bell ring-up
        setParam (p, "b1_width", 0.1f);    // narrowest → steepest resonance
        setParam (p, "b1_lift", 100.0f); setParam (p, "b1_depth", 100.0f);
        auto* slopeParam = p.apvts.getParameter ("b1_slope");
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*120.0/kSampleRate;
        float prev = 0.0f; double maxStep = 0.0; bool have = false;
        const int sweepBlocks = 600;   // slow full sweep 0→100→0, crosses both zone edges
        for (int blk = 0; blk < kWarmBlocks + sweepBlocks; ++blk)
        {
            if (blk >= kWarmBlocks)
            {
                const double u = (double) (blk - kWarmBlocks) / (double) sweepBlocks;
                const double tri = u < 0.5 ? u * 2.0 : 2.0 - u * 2.0;   // 0→1→0 in normalized param
                slopeParam->setValueNotifyingHost ((float) tri);
            }
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) { prev = buf.getSample (0, kBlock - 1); have = true; continue; }
            for (int s = 0; s < kBlock; ++s)
            {
                const float v = buf.getSample (0, s);
                if (have) maxStep = juce::jmax (maxStep, (double) std::abs (v - prev));
                prev = v; have = true;
            }
        }
        // 120 Hz sine max step ≈ 2π·120/48000 ≈ 0.0157; the pan sweeps too, so
        // allow generous headroom. A cascade-resume click is an impulse, ≫ this.
        ok &= check ("T36 slope sweep click-free (min width, low bell)", maxStep < 0.045);
        std::printf ("    max sample step, narrow low sweep: %.4f\n", maxStep);
    }

    // ── T37: STEPS at a fast rate must not click. Every other waveform's slew
    //         has a floor (S&H 4 ms, Env 5 ms, Chaos 4 ms); Steps was the one
    //         branch that took slewT = 0 at SMOOTH = 0, i.e. coefficient 1 —
    //         an INSTANT pan jump at each step edge. One step boundary is a
    //         discontinuity in the pan gains, which is a click on sustained
    //         material, and fast rates deliver them by the hundred per second. ──
    {
        EntropanAudioProcessor p;
        // Hard alternating pattern: every boundary is a full L→R throw, the
        // worst case the sequencer can produce.
        juce::String json = "{\"count\":8,\"steps\":[";
        for (int k = 0; k < 8; ++k)
        {
            json << "{\"subdiv\":1,\"vals\":[" << ((k % 2) ? "1.0" : "-1.0") << "]}";
            if (k < 7) json << ",";
        }
        json << "]}";
        p.setStepsJson (0, json);
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 1.0f);
        setParam (p, "b1_freq", 1000.0f);
        setParam (p, "b1_width", 2.0f);      // wide → the band carries the tone
        setParam (p, "b1_lift", 100.0f);
        setParam (p, "b1_depth", 100.0f);    // full throw, so a jump is maximal
        setParam (p, "b1_mode", 4.0f);       // Steps
        setParam (p, "b1_ratemode", 1.0f);   // Free — not tempo-locked
        setParam (p, "b1_rate", 8.0f);       // 8 Hz x 8 steps = 64 edges/sec
        setParam (p, "b1_stepsmooth", 0.0f); // square: the branch under test

        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate;
        float prevL = 0.0f, prevR = 0.0f; double maxStep = 0.0; bool have = false;
        const int runBlocks = 400;
        for (int blk = 0; blk < kWarmBlocks + runBlocks; ++blk)
        {
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) { prevL = buf.getSample(0,kBlock-1); prevR = buf.getSample(1,kBlock-1); have = true; continue; }
            for (int s = 0; s < kBlock; ++s)
            {
                const float l = buf.getSample (0, s), r = buf.getSample (1, s);
                if (have) maxStep = juce::jmax (maxStep, (double) juce::jmax (std::abs (l - prevL), std::abs (r - prevR)));
                prevL = l; prevR = r; have = true;
            }
        }
        // A clean 1 kHz sine steps by at most 2π·1000/48000 ≈ 0.131 per sample.
        // The pan also moves, so allow headroom — but an instant full-throw
        // jump lands far above this, exactly like the T35 W&F engage thump.
        ok &= check ("T37 fast STEPS sequencer is click-free", maxStep < 0.20);
        std::printf ("    max sample step, 8 Hz x 8 steps, SMOOTH=0: %.4f (clean sine %.4f)\n",
                     maxStep, 2.0*juce::MathConstants<double>::pi*1000.0/kSampleRate);
    }

    // ── T38: wow & flutter depth is specced in PITCH, and the engaged latency
    //         is reported to the host. Pitch swing for a sine-modulated delay
    //         is 2*pi*rate*peakDelay; the constants derive the peak delay from
    //         the pitch target, so this pins the audible spec rather than the
    //         implementation detail. ──
    {
        using P = EntropanAudioProcessor;
        const double wowPitch  = 2.0*juce::MathConstants<double>::pi * P::kWowRate  * P::kWowPeakS;
        const double flutPitch = 2.0*juce::MathConstants<double>::pi * P::kFlutRate * P::kFlutPeakS;
        // Openly an effect now — about 2x a worn cassette (0.15-0.35%).
        ok &= check ("T38a wow peak pitch in 0.7-0.9%",     wowPitch  > 0.007 && wowPitch  < 0.009);
        ok &= check ("T38b flutter peak pitch in 0.5-0.7%", flutPitch > 0.005 && flutPitch < 0.007);
        // The read tap must never run off the front of the line.
        ok &= check ("T38c base delay clears the swing",    P::kBaseDelayS > (P::kWowPeakS + P::kFlutPeakS));
        std::printf ("    wow %.3f%% (%.1f cents), flutter %.3f%% (%.1f cents), base %.2f ms\n",
                     wowPitch * 100.0,  1200.0 * std::log2 (1.0 + wowPitch),
                     flutPitch * 100.0, 1200.0 * std::log2 (1.0 + flutPitch),
                     P::kBaseDelayS * 1000.0);

        // Host PDC. wfLatencySamples() is the value the processor publishes;
        // prepareToPlay pushes it synchronously, so that path is checked against
        // getLatencySamples() directly. (Mid-session changes take the same value
        // through an AsyncUpdater — the message hop is plumbing, not arithmetic.)
        const int expected = (int) std::lround (P::kBaseDelayS * kSampleRate);
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        ok &= check ("T38d latency 0 while W&F is down",
                     p.wfLatencySamples() == 0 && p.getLatencySamples() == 0);
        setParam (p, "wow", 50.0f);
        ok &= check ("T38e engaged latency == base delay", p.wfLatencySamples() == expected);
        p.prepareToPlay (kSampleRate, kBlock);        // re-prepare publishes it
        ok &= check ("T38f engaged latency reaches the host", p.getLatencySamples() == expected);
        std::printf ("    reported %d samples engaged (%.2f ms @ %.0f Hz)\n",
                     p.getLatencySamples(), P::kBaseDelayS * 1000.0, kSampleRate);
        setParam (p, "flutter", 0.0f);
        setParam (p, "wow", 0.0f);
        ok &= check ("T38g latency returns to 0", p.wfLatencySamples() == 0);
    }

    // ── T39: ENGAGING W&F must not comb. T35 already engages instantly and
    //         passes, but it measures max sample-step on a 1 kHz sine, which
    //         cannot see the real artifact: while the engage crossfade runs,
    //         the output is dry summed with a copy of itself delayed by the
    //         base — a comb whose first null sits at 1/(2*base). Feed exactly
    //         that frequency and the level collapses mid-fade. Measured as an
    //         RMS dip in short windows against the steady-state level. ──
    {
        const double fNull = 1.0 / (2.0 * EntropanAudioProcessor::kBaseDelayS);
        EntropanAudioProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        setParam (p, "b1_on", 0.0f);          // W&F is global
        juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
        double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*fNull/kSampleRate;

        const int win = 128;                  // ~2.7 ms — finer than the fade
        double refRms = 0.0, minRms = 1.0e9;
        double acc = 0.0; int accN = 0;
        const int preBlocks = 60, postBlocks = 120;
        for (int blk = 0; blk < kWarmBlocks + preBlocks + postBlocks; ++blk)
        {
            if (blk == kWarmBlocks + preBlocks)       // flip it ON in one block
                setParam (p, "wow", 100.0f);
            for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
            p.processBlock (buf, midi);
            if (blk < kWarmBlocks) continue;
            for (int s = 0; s < kBlock; ++s)
            {
                const double v = buf.getSample (0, s);
                acc += v * v;
                if (++accN == win)
                {
                    const double rms = std::sqrt (acc / (double) win);
                    // last window before the flip = the reference level
                    if (blk < kWarmBlocks + preBlocks) refRms = rms;
                    else minRms = juce::jmin (minRms, rms);
                    acc = 0.0; accN = 0;
                }
            }
        }
        const double dipDb = 20.0 * std::log10 (juce::jmax (1.0e-9, minRms / juce::jmax (1.0e-9, refRms)));
        // A gliding tap keeps ONE signal path, so the level holds. A crossfade
        // against a delayed copy nulls this frequency outright.
        ok &= check ("T39 W&F engage does not comb", dipDb > -3.0);
        std::printf ("    engage dip at %.1f Hz (the comb null): %.1f dB\n", fNull, dipDb);
    }

    // ── T40: per-band SOLO. Auditions the band's own bell, panned — so a
    //         soloed narrow band on a two-tone input must keep the tone inside
    //         the band and reject the one outside it, and must follow the pan
    //         rather than nulling at centre (which is what auditioning the
    //         DISPLACEMENT band*(g-1) would do). ──
    {
        auto runSolo = [] (bool solo, double toneHz, float bias, double& outL, double& outR)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 1000.0f);
            setParam (p, "b1_width", 0.5f);      // narrow — clear in/out of band
            setParam (p, "b1_lift", 100.0f);
            setParam (p, "b1_depth", 0.0f);      // static: pan set by bias alone
            setParam (p, "b1_bias", bias);
            setParam (p, "b1_solo", solo ? 1.0f : 0.0f);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*toneHz/kSampleRate;
            double sl = 0.0, sr = 0.0; int n = 0;
            for (int blk = 0; blk < kWarmBlocks + 80; ++blk)
            {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase)*0.5f; phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                for (int s = 0; s < kBlock; ++s)
                { sl += (double) buf.getSample(0,s) * buf.getSample(0,s);
                  sr += (double) buf.getSample(1,s) * buf.getSample(1,s); ++n; }
            }
            outL = std::sqrt (sl / (double) n); outR = std::sqrt (sr / (double) n);
        };

        double inbandL, inbandR, outbandL, outbandR, mixL, mixR;
        runSolo (true,  1000.0,  0.0f, inbandL,  inbandR);    // tone AT the band
        runSolo (true,  100.0,   0.0f, outbandL, outbandR);   // tone well below it
        runSolo (false, 1000.0,  0.0f, mixL,     mixR);       // same, unsoloed

        ok &= check ("T40a solo passes the band's own tone", inbandL > 0.2 * mixL);
        ok &= check ("T40b solo rejects out-of-band content", outbandL < 0.1 * inbandL);
        std::printf ("    solo RMS: in-band %.4f, out-of-band %.4f, unsoloed mix %.4f\n",
                     inbandL, outbandL, mixL);

        // Panned hard left: the whole point of soloing is hearing this.
        double hlL, hlR;
        runSolo (true, 1000.0, -100.0f, hlL, hlR);
        ok &= check ("T40c soloed band still pans", hlL > 4.0 * hlR);
        std::printf ("    solo bias L: L %.4f vs R %.4f\n", hlL, hlR);

        // No band armed => the mix is untouched, bit for bit.
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            for (int i = 1; i <= 6; ++i)          // band 1 defaults ON — same premise as T1
                setParam (p, "b" + juce::String (i) + "_on", 0.0f);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            juce::Random rng (7);
            double maxDiff = 0.0;
            for (int blk = 0; blk < kWarmBlocks + 40; ++blk)
            {
                std::array<float, kBlock> refL {}, refR {};
                for (int s = 0; s < kBlock; ++s)
                {
                    const float l = rng.nextFloat()*2.0f-1.0f, r = rng.nextFloat()*2.0f-1.0f;
                    refL[(size_t) s] = l; refR[(size_t) s] = r;
                    buf.setSample (0, s, l); buf.setSample (1, s, r);
                }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                for (int s = 0; s < kBlock; ++s)
                    maxDiff = juce::jmax (maxDiff,
                                (double) juce::jmax (std::abs (buf.getSample(0,s) - refL[(size_t) s]),
                                                     std::abs (buf.getSample(1,s) - refR[(size_t) s])));
            }
            // Same bar T1 sets for "exact wire".
            ok &= check ("T40d nothing soloed leaves the wire exact", maxDiff < 1.0e-6);
            std::printf ("    idle maxDiff with solo compiled in: %.3g\n", maxDiff);
        }
    }

    // ── T41: every APVTS parameter must appear in exactly one of the editor's
    //         relay ID lists. Those lists are hand-maintained, and a parameter
    //         missing from them gets no WebView relay — so Juce.getSliderState/
    //         getToggleState binds to nothing and the control silently does
    //         nothing. That is exactly how b*_solo shipped broken in B100: the
    //         DSP was right and tested, the knob just was not connected. ──
    {
        EntropanAudioProcessor p;
        juce::StringArray relayed;
        relayed.addArray (EntropanAudioProcessorEditor::sliderParamIds());
        relayed.addArray (EntropanAudioProcessorEditor::comboParamIds());
        relayed.addArray (EntropanAudioProcessorEditor::toggleParamIds());

        juce::StringArray missing, unknown;
        for (auto* param : p.getParameters())
            if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
                if (! relayed.contains (wid->paramID))
                    missing.add (wid->paramID);

        for (const auto& id : relayed)
            if (p.apvts.getParameter (id) == nullptr)
                unknown.add (id);

        // A duplicate would build two relays for one parameter and the second
        // would quietly win, so catch that too.
        juce::StringArray dupes;
        for (int i = 0; i < relayed.size(); ++i)
            for (int j = i + 1; j < relayed.size(); ++j)
                if (relayed[i] == relayed[j] && ! dupes.contains (relayed[i]))
                    dupes.add (relayed[i]);

        ok &= check ("T41a every parameter has a UI relay", missing.isEmpty());
        ok &= check ("T41b no relay for a parameter that does not exist", unknown.isEmpty());
        ok &= check ("T41c no duplicate relay ids", dupes.isEmpty());
        std::printf ("    %d parameters, %d relayed", p.getParameters().size(), relayed.size());
        if (! missing.isEmpty()) std::printf ("; MISSING: %s", missing.joinIntoString (", ").toRawUTF8());
        if (! unknown.isEmpty()) std::printf ("; UNKNOWN: %s", unknown.joinIntoString (", ").toRawUTF8());
        if (! dupes.isEmpty())   std::printf ("; DUPLICATE: %s", dupes.joinIntoString (", ").toRawUTF8());
        std::printf ("\n");
    }

    // ── T42: FLUX. Two properties have to hold. At 0 it must be EXACTLY the
    //         old pure-sine wow/flutter, so turning the knob up is the only way
    //         to change the sound. And because FLUX crossfades toward a bounded
    //         walk instead of adding a term, it must not widen the delay swing
    //         — if it did, the read tap could cross zero and the reported
    //         latency would be a lie. ──
    {
        auto capture = [] (float flux, std::vector<float>& out)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 0.0f);
            setParam (p, "wow", 100.0f);
            setParam (p, "flutter", 100.0f);
            setParam (p, "flux", flux);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*440.0/kSampleRate;
            out.clear();
            for (int blk = 0; blk < 260; ++blk)   // ~2.8 s: several wow revolutions
            {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase)*0.5f; phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk >= kWarmBlocks)
                    for (int s = 0; s < kBlock; ++s) out.push_back (buf.getSample (0, s));
            }
        };

        std::vector<float> a, b, c;
        capture (0.0f,   a);
        capture (0.0f,   b);
        capture (100.0f, c);

        double sameDiff = 0.0, fluxDiff = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            sameDiff = juce::jmax (sameDiff, (double) std::abs (a[i] - b[i]));
            fluxDiff = juce::jmax (fluxDiff, (double) std::abs (a[i] - c[i]));
        }
        ok &= check ("T42a flux 0 is deterministic", sameDiff == 0.0);
        ok &= check ("T42b flux 100 actually changes the sound", fluxDiff > 0.01);
        std::printf ("    flux 0 vs 0: %.3g;  flux 0 vs 100: %.3g\n", sameDiff, fluxDiff);

        // Peak excursion must not grow: same input, so any extra output swing
        // would mean the shape left +/-1. Compare peak levels.
        double peakA = 0.0, peakC = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            peakA = juce::jmax (peakA, (double) std::abs (a[i]));
            peakC = juce::jmax (peakC, (double) std::abs (c[i]));
        }
        ok &= check ("T42c flux does not widen the swing", peakC <= peakA * 1.02 + 1.0e-6);
        std::printf ("    peak flux 0 %.4f vs flux 100 %.4f\n", peakA, peakC);

        // And the latency it reports is unchanged by flux — the whole point of
        // crossfading rather than adding.
        EntropanAudioProcessor p0;
        p0.prepareToPlay (kSampleRate, kBlock);
        setParam (p0, "wow", 100.0f);
        const int latNoFlux = p0.wfLatencySamples();
        setParam (p0, "flux", 100.0f);
        ok &= check ("T42d flux costs no latency", p0.wfLatencySamples() == latNoFlux);
    }

    // ── T43: band SHAPES. Each shape is a different unity-gain extraction fed
    //         into the same out = in + B(in)*lift*(g-1), so two things must hold
    //         for every one of them: at pan 0 the displacement vanishes and the
    //         wire stays exact, and the shape actually selects the region it
    //         claims. Probed with two tones — 200 Hz and 5 kHz — around a band
    //         sitting at 1 kHz. ──
    {
        // energy at each tone, hard-panned left, for a given shape
        auto probe = [] (int shape, double toneHz, double& l, double& r)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 1000.0f);
            setParam (p, "b1_width", 1.0f);
            setParam (p, "b1_lift", 100.0f);
            setParam (p, "b1_depth", 0.0f);
            setParam (p, "b1_bias", -100.0f);      // static hard left
            setParam (p, "b1_shape", (float) shape);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double ph = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*toneHz/kSampleRate;
            double sl = 0.0, sr = 0.0; int n = 0;
            for (int blk = 0; blk < kWarmBlocks + 80; ++blk)
            {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(ph)*0.5f; ph+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                for (int s = 0; s < kBlock; ++s)
                { sl += (double) buf.getSample(0,s)*buf.getSample(0,s);
                  sr += (double) buf.getSample(1,s)*buf.getSample(1,s); ++n; }
            }
            l = std::sqrt (sl/(double) n); r = std::sqrt (sr/(double) n);
        };

        // "moved" = how far left/right this tone got pushed. 1.0 = untouched.
        auto ratio = [&] (int shape, double hz) { double l,r; probe (shape,hz,l,r); return r / juce::jmax (1.0e-9, l); };

        const double bellLow  = ratio (0, 200.0),  bellHigh = ratio (0, 5000.0), bellMid = ratio (0, 1000.0);
        // Probed DEEP in each passband. A minimum-phase LP/HP is only transparent
        // well away from its corner: near the cutoff it still passes the tone at
        // unity magnitude but with tens of degrees of phase, and in + B(g-1)
        // cancels by vector sum, so the far channel keeps a residue there. Band-
        // pass has no such problem because it is real-valued AT its centre,
        // which is what makes the bell reach the rail exactly.
        const double lowLow   = ratio (1, 50.0),   lowHigh  = ratio (1, 5000.0);
        const double highLow  = ratio (2, 200.0),  highHigh = ratio (2, 15000.0);
        const double notchMid = ratio (3, 1000.0), notchLow = ratio (3, 200.0);

        // Diagnostic: SOLO outputs band*lift*g, so with bias 0 (g == 1) its RMS
        // against the input's IS the extraction gain. The displacement only
        // cancels cleanly where that gain is 1.
        auto extractionGain = [] (int shape, double toneHz)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 1000.0f);
            setParam (p, "b1_width", 1.0f);
            setParam (p, "b1_lift", 100.0f);
            setParam (p, "b1_depth", 0.0f);
            setParam (p, "b1_bias", 0.0f);
            setParam (p, "b1_shape", (float) shape);
            setParam (p, "b1_solo", 1.0f);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double ph = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*toneHz/kSampleRate;
            double acc = 0.0; int n = 0;
            for (int blk = 0; blk < kWarmBlocks + 80; ++blk)
            {
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(ph)*0.5f; ph+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                for (int s = 0; s < kBlock; ++s) { acc += (double) buf.getSample(0,s)*buf.getSample(0,s); ++n; }
            }
            return std::sqrt (acc/(double) n) / (0.5 / std::sqrt (2.0));   // vs the 0.5-amplitude input
        };
        std::printf ("    extraction gain (1.0 = unity passband, what the null needs)\n");
        std::printf ("      low  @200Hz %.3f   high @5k %.3f   bell @1k %.3f\n",
                     extractionGain (1, 200.0), extractionGain (2, 5000.0), extractionGain (0, 1000.0));
        std::printf ("    R/L per shape (1.0 = untouched, <1 = pushed left)\n");
        std::printf ("      bell : 200Hz %.3f  1k %.3f  5k %.3f\n", bellLow, bellMid, bellHigh);
        std::printf ("      low  : 50Hz  %.3f          5k  %.3f\n", lowLow, lowHigh);
        std::printf ("      high : 200Hz %.3f          15k %.3f\n", highLow, highHigh);
        std::printf ("      notch: 200Hz %.3f  1k %.3f\n", notchLow, notchMid);

        ok &= check ("T43a bell moves its centre, not the flanks", bellMid < 0.2 && bellLow > 0.7 && bellHigh > 0.7);
        ok &= check ("T43b low moves the lows only",               lowLow  < 0.25 && lowHigh  > 0.7);
        ok &= check ("T43c high moves the highs only",             highHigh< 0.25 && highLow  > 0.7);
        ok &= check ("T43d notch moves everything BUT the centre", notchMid > 0.7 && notchLow < 0.2);

        // Tilt: lows and highs must go OPPOSITE ways from the same pan.
        {
            double tl_l, tl_r, th_l, th_r;
            probe (4, 200.0, tl_l, tl_r);
            probe (4, 5000.0, th_l, th_r);
            // HARD separation now: the LR4 split is phase-matched, so a full
            // tilt sends each half to its side outright. The old
            // displacement-based tilt managed 1.9:1 (a wobble); this demands
            // better than 4:1 a couple of octaves off the corner.
            const bool lowsLeft   = tl_l > 4.0 * tl_r;
            const bool highsRight = th_r > 4.0 * th_l;
            ok &= check ("T43e tilt separates lows and highs hard", lowsLeft && highsRight);
            std::printf ("      tilt : 200Hz L %.3f R %.3f | 5k L %.3f R %.3f\n", tl_l, tl_r, th_l, th_r);
        }

        // Every shape must still leave the wire exact when nothing is panned.
        for (int shape = 0; shape < 5; ++shape)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_shape", (float) shape);
            setParam (p, "b1_lift", 100.0f);
            setParam (p, "b1_depth", 0.0f);
            setParam (p, "b1_bias", 0.0f);        // centre ⇒ g == 1 ⇒ no displacement
            setParam (p, "b1_gain", 0.0f);
            double maxDiff = 0;
            const auto r = run (p, maxDiff);
            if (shape == 4)
            {
                // Tilt is the honest exception: engaged at centre it is an LR4
                // ALLPASS of the input — magnitude-identical, phase-rotated —
                // because the hard separation requires replacing the signal
                // with the phase-matched split rather than displacing from dry.
                // So the promise here is RMS-flat, not bit-exact. The band-OFF
                // wire stays bit-exact (T1).
                const double dL = dB (r.outL / r.inL), dR = dB (r.outR / r.inR);
                ok &= check ("T43j tilt at centre is level-exact (allpass)",
                             std::abs (dL) < 0.05 && std::abs (dR) < 0.05);
                std::printf ("      tilt centre: %+0.3f dB L, %+0.3f dB R (phase-only rotation)\n", dL, dR);
            }
            else
            {
                static const char* kNames[] = { "T43f bell nulls at centre",
                                                "T43g low nulls at centre",
                                                "T43h high nulls at centre",
                                                "T43i notch nulls at centre" };
                ok &= check (kNames[shape], maxDiff < 1.0e-5);
                if (maxDiff >= 1.0e-5) std::printf ("      shape %d maxDiff %.3g\n", shape, maxDiff);
            }
        }
    }

    std::printf ("\n%s\n", ok ? "ALL TESTS PASSED" : "TESTS FAILED");
    return ok ? 0 : 1;
}
