# Entropan — Implementation Plan

## Complexity Score: 4/5

## Implementation Strategy: Phased

### Phase 4.1: Core Processing (null-test first)
- [ ] `PluginProcessor` skeleton + full 71-param APVTS layout (from parameter-spec).
- [ ] Band splitter cascade: 6 × 3-way LR4 split (`juce::dsp::LinkwitzRileyFilter`) with allpass compensation, serial residual chain.
- [ ] Lift split per band: `lifted = band·lift` → pan path, `unlifted = band·(1−lift)` → dry sum.
- [ ] Static equal-power pan per band (fixed pan target from `depth`·`mix`, no modulators yet).
- [ ] Sum + width/output/bypass stages.
- [ ] **Gate:** null test — all bands off → output ≈ input; 1 band on, lift 0 (or depth 0, centered) → output ≈ input (allpass-flat magnitude).

### Phase 4.2: Modulator Engines
- [ ] Control-rate tick infrastructure (~1 ms) + per-band one-pole inertia slew + smoothed pan gains.
- [ ] Sine, Triangle (phase accumulator + phase offset).
- [ ] S&H, Drift (seeded RNG streams, `seed + bandIndex`).
- [ ] Chaos (Lorenz, control-rate integration, normalized x).
- [ ] Tempo sync via `AudioPlayHead` PPQ (interval boundaries, loop-jump safety, free-run fallback).
- [ ] Re-roll: atomic flag UI→audio, re-deal at next tick.
- [ ] Mix master-depth scaling.

### Phase 4.3: Steps Mode + State + Analyzer
- [ ] Step sequencer engine (2–16 steps, value = pan target, PPQ/free clocked, phase = start offset).
- [ ] Step data in ValueTree; `getStateInformation`/`setStateInformation` round-trip incl. APVTS + steps.
- [ ] Analyzer FIFO tap → UI-thread FFT 2048 → spectrum frames to WebView event channel.

### Phase 4.4: Polish
- [ ] Click-free band enable crossfade (~30 ms), filter re-tune smoothing (freq/width drag).
- [ ] Denormal protection (`ScopedNoDenormals`), CPU check with 6 bands active.
- [ ] Transport edge cases: stop/start, loop wrap, tempo change mid-note.
- [ ] Parameter smoothing audit (no zipper on freq/width/depth automation).

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
