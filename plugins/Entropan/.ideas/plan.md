# Entropan — Implementation Plan

## Complexity Score: 4/5

## Implementation Strategy: Phased

### Phase 4.1: Core Processing (null-test first)
- [x] `PluginProcessor` skeleton + full 77-param APVTS layout (from parameter-spec).
- [x] Band splitter cascade: 6 × 3-way LR4 split (`juce::dsp::LinkwitzRileyFilter`) with allpass compensation, serial residual chain.
- [x] Lift split per band: `lifted = band·lift` → pan path (+ per-band ±6 dB gain, smoothed), `unlifted = band·(1−lift)` → dry sum.
- [x] Static equal-power pan per band (fixed pan target from `depth`·`amount`, no modulators yet).
- [x] Sum + output/bypass stages.
- [x] **Gate:** null test — all bands off → output ≈ input; 1 band on, lift 0 (or depth 0, centered) → output ≈ input (allpass-flat magnitude).

### Phase 4.2: Modulator Engines
- [x] Control-rate tick infrastructure (~1 ms) + per-band one-pole inertia slew + smoothed pan gains.
- [x] Sine, Triangle (phase accumulator + phase offset).
- [x] S&H, Drift (seeded RNG streams, `seed + bandIndex`).
- [x] Chaos (Lorenz, control-rate integration, normalized x).
- [x] Rate modes: tempo sync via `AudioPlayHead` PPQ (loop-jump safety), free Hz incl. audio-rate per-sample path (>~50 Hz), MIDI note tracking (`acceptsMidi`, last-note priority, inertia = glide).
- [x] Global speed multiplier into all band rates.
- [x] Re-roll: atomic flag UI→audio, re-deal at next tick.
- [x] Amount master-depth scaling.

### Phase 4.3: Steps Mode + State + Analyzer
- [x] Step sequencer engine (2–16 slot-stable slots, subdiv 1/2/4 ratchets, PPQ/free clocked, phase = start offset).
- [x] Step data in ValueTree; `getStateInformation`/`setStateInformation` round-trip incl. APVTS + steps.
- [x] Analyzer FIFO tap → UI-thread FFT 2048 → spectrum frames to WebView event channel.

### Phase 4.4: Polish
- [x] Click-free band enable crossfade (~30 ms); block-rate cutoff glide (~40 ms) kills freq/Q drag zipper.
- [x] Denormal protection (`ScopedNoDenormals`); CPU gate T9: 0.8% realtime, 6 bands + audio-rate.
- [x] Transport edge cases: sync phase is a pure function of PPQ while playing (loop/relocate safe); free-run fallback when stopped.
- [x] Parameter smoothing audit: freq/width glide, depth per-sample smoothed, lift/gain/enable/amount/output smoothed. Inertia decoupled from reach (rate-relative slew + analytic compensation, T7).

## Dependencies

**Required JUCE modules:**
- `juce_audio_basics`, `juce_audio_processors`
- `juce_dsp` (LinkwitzRileyFilter, FFT, SmoothedValue)
- `juce_gui_extra` (WebBrowserComponent) + `JUCE_WEB_BROWSER=1`

**External:** none. Fonts + `js/juce` interop copied from Chaosverb UI assets.

## Risk Assessment

**High:**
- Cascade phase coherence — null test can fail subtly if allpass compensation topology wrong. Mitigation: build 1-band split first, verify magnitude flatness (pink noise + spectrum diff), then scale to 6.
- Overlapping bands semantics — lower-index band claims overlap (serial cascade). Must be documented in UI/manual; risk of "band 2 sounds thin" reports.

**Medium:**
- Tempo-sync boundary logic under looping/relocate (double triggers or missed S&H deals).
- WebView gesture ↔ 3 params (freq/width/depth) mapping fidelity + relayout during drag.
- Step-sequencer state sync UI↔processor (non-APVTS path needs own change-notify).
- Filter re-tune while audio runs (freq drag) — needs coefficient smoothing or small crossfade to avoid zipper.

**Low:**
- Global gain stages, seed handling, bypass crossfade, analyzer FIFO.

## UI Framework Decision

**Decision: webview**

**Rationale:** User mandate — clone Chaosverb v1.0.18 GUI (WebView, single-file HTML). Plugin additionally needs an interactive spectrum display with lift-gesture bell editing — canvas-based WebView is the proven path (Chaosverb PluginEditor glue reusable ~770 lines); stock Visage widgets can't cover this without building a custom spectrum editor from scratch.

**Implementation strategy: phased** (complexity 4).
