/*
  Entropan Phase 4.1 gate: null / energy tests for the splitter cascade.

  T1  all bands off                     → output == input (exact wire)
  T2  band 1 on, lift 0                 → RMS preserved (allpass-flat)
  T3  band 1 on, lift 100, depth 0      → RMS preserved (centre pan unity)
  T4  band 1 on, lift 100, depth 100    → energy moves right (pan works)

  Exit code 0 = all pass.
*/

#include "../PluginProcessor.h"
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
        setParam (p, "b1_rate", 0.02f);
        setParam (p, "b1_phase", 90.0f);
        setParam (p, "b1_inertia", 100.0f);   // ← the whole point

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
                inE += 0.5;
                outLE += juce::square ((double) buf.getSample (0, s));
                outRE += juce::square ((double) buf.getSample (1, s));
            }
        }
        const double dL = dB (outLE / inE), dR = dB (outRE / inE);
        ok &= check ("T7 full reach at inertia 100%", dL < -6.0 && dR > 2.0 && dR < 4.0);
        std::printf ("    dL = %+.2f dB, dR = %+.2f dB\n", dL, dR);
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

    std::printf ("\n%s\n", ok ? "ALL TESTS PASSED" : "TESTS FAILED");
    return ok ? 0 : 1;
}
