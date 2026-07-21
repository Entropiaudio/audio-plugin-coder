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

    // ── diagnostic harness (ENTROPAN_DIAG=fft): replicate the editor's exact
    //    analyzer pipeline on a pure sine and print the display columns ──
    if (std::getenv ("ENTROPAN_DIAG") != nullptr
        && juce::String (std::getenv ("ENTROPAN_DIAG")) == "fft")
    {
        constexpr int kOrd = 14, kN = 1 << kOrd, kBins = 256;
        for (double sr : { 48000.0, 44100.0 })
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (sr, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 237.0f); setParam (p, "b1_width", 1.41f);
            setParam (p, "b1_lift", 0.0f);   setParam (p, "b1_depth", 100.0f);
            setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 0.5f);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            juce::dsp::FFT fft (kOrd);
            juce::dsp::WindowingFunction<float> win ((size_t) kN, juce::dsp::WindowingFunction<float>::blackmanHarris);
            std::vector<float> drain ((size_t) kN), accum ((size_t) kN, 0.0f), work ((size_t) kN * 2, 0.0f);
            int fill = 0;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*237.0/sr;
            // editor cadence: process ~1 block then drain (60 Hz ≈ 800 samples ≈ 1.5 blocks)
            std::vector<float> cols ((size_t) kBins, -120.0f);
            for (int blk = 0; blk < 300; ++blk)
            {
                for (int s = 0; s < kBlock; ++s) {
                    if (std::getenv ("ENTROPAN_SPLICE") != nullptr && ((blk * kBlock + s) % (int) (sr / 2)) == 0)
                        phase = 0.0;   // loop-point discontinuity every 0.5 s
                    const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                const int got = p.popAnalyzer (drain.data(), kN);
                if (got > 0)
                {
                    if (got >= kN) std::memcpy (accum.data(), drain.data() + (got - kN), sizeof (float) * kN);
                    else { const int keep = kN - got;
                           std::memmove (accum.data(), accum.data() + got, sizeof (float) * (size_t) keep);
                           std::memcpy (accum.data() + keep, drain.data(), sizeof (float) * (size_t) got);
                    }
                    fill = juce::jmin (kN, fill + got);
                }
                if (blk < 299 || fill < kN) continue;
                std::memcpy (work.data(), accum.data(), sizeof (float) * kN);
                win.multiplyWithWindowingTable (work.data(), kN);
                fft.performFrequencyOnlyForwardTransform (work.data());
                const double binHz = sr / (double) kN;
                for (int b = 0; b < kBins; ++b)
                {
                    const double f0 = 20.0 * std::pow (1000.0, (double) b / kBins);
                    const double f1 = 20.0 * std::pow (1000.0, (double) (b + 1) / kBins);
                    const double fc = std::sqrt (f0 * f1);
                    const int b0 = juce::jlimit (1, kN/2 - 2, (int) (fc / binHz));
                    const float fr = (float) juce::jlimit (0.0, 1.0, fc / binHz - b0);
                    const int i0 = juce::jlimit (1, kN/2 - 1, (int) (f0 / binHz));
                    const int i1 = juce::jlimit (i0 + 1, kN/2, (int) std::ceil (f1 / binHz));
                    float mag;
                    if (i1 - i0 >= 2) { mag = 0; for (int k = i0; k < i1; ++k) mag = juce::jmax (mag, work[(size_t) k]); }
                    else mag = work[(size_t) b0] * (1.0f - fr) + work[(size_t) (b0 + 1)] * fr;
                    cols[(size_t) b] = (float) juce::Decibels::gainToDecibels ((double) mag / (double) (kN / 4), -80.0);
                }
            }
            // report: peak col + every col above -55 dB with its frequency
            int pk = 0; for (int b = 1; b < kBins; ++b) if (cols[(size_t) b] > cols[(size_t) pk]) pk = b;
            std::printf ("sr %.0f  peak col %d (%.0f Hz, %.1f dB)  cols > -55 dB:\n", sr,
                         pk, 20.0 * std::pow (1000.0, (pk + 0.5) / (double) kBins), cols[(size_t) pk]);
            for (int b = 0; b < kBins; ++b)
                if (cols[(size_t) b] > -55.0f)
                    std::printf ("  col %3d  %6.0f Hz  %6.1f dB\n", b,
                                 20.0 * std::pow (1000.0, (b + 0.5) / (double) kBins), cols[(size_t) b]);
        }
        return 0;
    }

    // ── diagnostic harness (ENTROPAN_DIAG=1): pan reach + volume-vs-pan sweep ──
    // Renders a mono sine through one band and reports, per scenario, the
    // balance extremes and the output power binned by |balance| — the two
    // numbers behind "doesn't reach hard L/R" and "volume changes at centre".
    if (std::getenv ("ENTROPAN_DIAG") != nullptr)
    {
        struct Scen { const char* name; float sineHz, depth; const char* routes; float biasFree, gainRouteDepth; };
        const Scen scens[] = {
            { "S1 depth100 no-routes            ", 1000.0f, 100.0f, nullptr, 0.0f, 0.0f },
            { "S2 depth34  no-routes            ", 1000.0f,  34.0f, nullptr, 0.0f, 0.0f },
            { "S3 depth34  sine->BIAS 100       ", 1000.0f,  34.0f, "{\"routes\":[{\"src\":0,\"stype\":0,\"dst\":8,\"depth\":100}]}", 0.0f, 0.0f },
            { "S4 depth34  sine->BIAS 100 +OVR  ", 1000.0f,  34.0f, "{\"routes\":[{\"src\":0,\"stype\":0,\"dst\":8,\"depth\":100}]}", 1.0f, 0.0f },
            { "S5 depth0   sine->BIAS 100       ", 1000.0f,   0.0f, "{\"routes\":[{\"src\":0,\"stype\":0,\"dst\":8,\"depth\":100}]}", 0.0f, 0.0f },
            { "S6 depth100 sine at band edge    ",  500.0f, 100.0f, nullptr, 0.0f, 0.0f },
            { "S7 depth100 + sine->GAIN 100     ", 1000.0f, 100.0f, "{\"routes\":[{\"src\":0,\"stype\":0,\"dst\":4,\"depth\":100}]}", 0.0f, 0.0f },
        };
        for (const auto& sc : scens)
        {
            EntropanAudioProcessor p;
            p.prepareToPlay (kSampleRate, kBlock);
            setParam (p, "b1_on", 1.0f);
            setParam (p, "b1_freq", 1000.0f); setParam (p, "b1_width", 2.0f);
            setParam (p, "b1_lift", 100.0f);  setParam (p, "b1_depth", sc.depth);
            setParam (p, "amount", 100.0f);
            setParam (p, "b1_ratemode", 1.0f); setParam (p, "b1_rate", 0.5f);
            setParam (p, "b1_inertia", 0.0f);
            setParam (p, "b1_override", sc.biasFree);
            if (sc.routes != nullptr) p.setRoutesJson (sc.routes);
            juce::AudioBuffer<float> buf (2, kBlock); juce::MidiBuffer midi;
            double phase = 0.0; const double inc = 2.0*juce::MathConstants<double>::pi*(double) sc.sineHz/kSampleRate;
            double balMin = 1e9, balMax = -1e9;
            double pwrCentre = 0, nCentre = 0, pwrSide = 0, nSide = 0, pwrAll = 0, nAll = 0;
            constexpr int kWin = 96;   // ~2 ms windows
            for (int blk = 0; blk < kWarmBlocks + 400; ++blk) {   // ~4.3 s = 2+ mod cycles
                for (int s = 0; s < kBlock; ++s) { const float v=(float)std::sin(phase); phase+=inc; buf.setSample(0,s,v); buf.setSample(1,s,v); }
                p.processBlock (buf, midi);
                if (blk < kWarmBlocks) continue;
                for (int w = 0; w + kWin <= kBlock; w += kWin) {
                    double el = 1e-12, er = 1e-12;
                    for (int s = w; s < w + kWin; ++s) { el += juce::square ((double) buf.getSample (0, s));
                                                         er += juce::square ((double) buf.getSample (1, s)); }
                    const double bal = (el - er) / (el + er);
                    const double pwr = (el + er) / (double) kWin;   // vs mono sine pwr 0.5·2ch
                    balMin = juce::jmin (balMin, bal); balMax = juce::jmax (balMax, bal);
                    pwrAll += pwr; nAll++;
                    if (std::abs (bal) < 0.15) { pwrCentre += pwr; nCentre++; }
                    if (std::abs (bal) > 0.55) { pwrSide   += pwr; nSide++;   }
                }
            }
            const double pc = nCentre ? dB (pwrCentre / nCentre) : -999;
            const double ps = nSide   ? dB (pwrSide   / nSide)   : -999;
            std::printf ("%s bal %+.3f..%+.3f  pwr centre %+.2f dB (n=%.0f)  side %+.2f dB (n=%.0f)  Δ %+.2f dB\n",
                         sc.name, balMin, balMax, pc, nCentre, ps, nSide,
                         (nCentre && nSide) ? pc - ps : 0.0);
        }
        return 0;
    }

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

    // ── T30: every slope reconstructs null-clean — lift-0 allpass-flat and
    //         centre-pan unity must hold at 12 and 48 dB/oct like they do at 24 ──
    {
        for (float slope : { 0.0f, 2.0f })
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

            const bool pass = std::abs (f0L) < 0.05 && std::abs (f0R) < 0.05
                           && std::abs (c0L) < 0.05 && std::abs (c0R) < 0.05;
            char name[64];
            std::snprintf (name, sizeof (name), "T30 slope %d null-clean", slope == 0.0f ? 12 : 48);
            ok &= check (name, pass);
            std::printf ("    lift0 %+0.4f/%+0.4f dB   centre %+0.4f/%+0.4f dB\n", f0L, f0R, c0L, c0R);
        }
    }

    std::printf ("\n%s\n", ok ? "ALL TESTS PASSED" : "TESTS FAILED");
    return ok ? 0 : 1;
}
