# Entropan — DSP Architecture Specification

## Core Components

1. **Band Splitter Cascade** (6 slots, serial)
   - Per active band: 3-way Linkwitz-Riley split around band edges `[f_lo, f_hi]` derived from `freq` (center) and `width` (octaves): `f_lo = freq / 2^(width/2)`, `f_hi = freq * 2^(width/2)`.
   - Implementation: `juce::dsp::LinkwitzRileyFilter` (LR4). Split: input → LR(f_lo) → {low, rest}; rest → LR(f_hi) → {band, high}. Low branch gets allpass at `f_hi` (LR allpass mode) so `low_ap + band + high` sums allpass-flat.
   - **Serial cascade:** band N extracts from residual of band N-1. Band and residual share upstream phase rotation, so the final sum stays flat. Overlapping user bands: overlapped region is claimed by the lower-index band (band 2 extracts from what's left). Documented behavior, not a bug.
   - Disabled band = splitter fully bypassed. Enable/disable crossfaded (~30 ms) to avoid clicks.

2. **Per-Band Lift + Pan Stage** (×6)
   - **Lift split:** extracted band splits into `lifted = band · lift` (→ pan path) and `unlifted = band · (1 − lift)` (→ summed straight back, dry). `lift` = bell height (0–1). At `lift = 0` band recombines untouched → null preserved.
   - Equal-power balance law on the lifted stereo portion: θ = (pan+1)·π/4, `gainL = cos θ`, `gainR = sin θ` (scaled ×√2 so center = unity).
   - Balance-style (no mono fold) — preserves intra-band stereo detail.
   - Lift + pan gains applied via per-sample smoothing (`SmoothedValue`, ~5 ms) — no zipper.

3. **Modulator Engine** (×6, one per band, control-rate ticks ~1 ms + per-sample slew)
   | Mode | Algorithm |
   | :--- | :--- |
   | Sine | Phase accumulator, `sin(2π·φ + phase)` |
   | Triangle | Same accumulator, triangle shaping |
   | S&H | New uniform(-1,1) target at each interval boundary |
   | Drift | Smoothed random walk (interpolated value-noise clocked by rate) |
   | Chaos | Lorenz system integrated at control rate; rate scales dt; x-component normalized to ±1 |
   | Steps | Sequencer 2–16 steps; step = {value, length}; length ∈ {1, ½, ¼} of base interval (ratchet); weighted-duration clock; phase offsets start position |
   - **Inertia** = one-pole slew on modulator output → pan target. 0% = instant snap, 100% ≈ 2 s glide (exp map).
   - **Depth**: `pan = mod · depth · mix_master` (`mix` = global master depth).
   - **Tempo sync**: `AudioPlayHead` PPQ → interval boundaries for S&H/Steps, cycle length for Sine/Tri/Drift. Free-run Hz fallback when no playhead/`sync` off.
   - **Seed/streams**: RNG stream per band = `seed + bandIndex`. Reproducible.
   - **Re-roll**: UI event sets atomic flag → audio thread re-deals S&H/Drift/Chaos states at next control tick.

4. **Global Output Stage**
   - Σ (panned lifted portions + unlifted portions + residual) → **Output** gain → **Bypass** (crossfaded). (No global width/M-S stage — dropped.)
   - **Mix = master depth**: smoothed multiplier into all band depths (NO dry/wet stage — unlifted spectrum is inherently dry).

5. **Mod telemetry** (display only)
   - Control thread publishes each band's current mod value (post-inertia) at ~30 Hz to the UI event channel → scope ring buffer + pan-trace animation. No audio-thread allocation; reuse Chaosverb feedback-event pattern.

6. **Analyzer Tap** (display only, NOT in audio path)
   - Lock-free FIFO (input or post-output tap) → message thread → `juce::dsp::FFT` 2048 → magnitude spectrum → WebView via JUCE event (same pattern as SNIPBridge/Chaosverb feedback events).
   - Drives spectrum display + lift-gesture UI.

## Processing Chain

```
Input ─ Splitter1 ─ band1 ─┬─ ×lift1 ─ pan1(mod1) ─┐
          │                └─ ×(1−lift1) ──────────┤
          └ residual → Splitter2 ─ band2 ─ (same) ─┤(Σ) → OUTPUT ─► Out
                └ … Splitter6 ─ band6 ─ (same) ────┤    (gain)
                      └ final residual ────────────┘
pan_i = mod_i · depth_i · MIX(master)
Analyzer FIFO tap ──► (UI thread FFT → WebView spectrum)
```

## Parameter Mapping

| Parameter | Component | Function | Range |
| :--- | :--- | :--- | :--- |
| `seed` | RNG streams | Reproducible randomness | 1–128 |
| `mix` | All modulators | **Master depth** scale on all band depths | 0–100% |
| `output` | Output | Makeup gain | -24…+12 dB |
| `bypass` | Output | Crossfaded bypass | off/on |
| `bN_on` | Splitter N | Engage (crossfaded) | off/on |
| `bN_freq` | Splitter N | Band center → f_lo/f_hi | 20–20k Hz log |
| `bN_width` | Splitter N | Band width → f_lo/f_hi | 0.1–4 oct |
| `bN_lift` | Lift split N | Extraction amount (bell height) | 0–100% |
| `bN_depth` | Pan N | Pan modulation amplitude | 0–100% |
| `bN_mode` | Modulator N | Algorithm select | 6 choices |
| `bN_rate` | Modulator N | Free-run speed | 0.02–8 Hz log |
| `bN_sync` | Modulator N | Tempo sync switch | off/on |
| `bN_div` | Modulator N | Synced interval | 1/16–4 bar |
| `bN_inertia` | Modulator N | Slew time | 0–100% |
| `bN_phase` | Modulator N | Phase offset (periodic modes) | 0–360° |

Step-sequencer data (2–16 steps × 6 bands, pan value per step): **ValueTree state blob**, not APVTS (see parameter-spec).

## Complexity Assessment

**Score: 4/5 (Expert)**

**Rationale:**
- Multi-band complementary filter cascade with phase-coherence requirement (null test) — Level 3 baseline.
- ×6 independent modulator engines, 6 algorithms incl. chaotic oscillator + step sequencer with non-APVTS state — pushes to 4.
- Tempo-sync boundary logic (loop jumps, transport stop/start), click-free band engage, re-roll thread handoff.
- Real-time analyzer feed + WebView gesture UI (bells over spectrum) with bidirectional param sync.
- Not Level 5: no synthesis/resynthesis, no ML, all building blocks exist in JUCE (`LinkwitzRileyFilter`, `FFT`, `SmoothedValue`).
