# SNIP Bridge - Implementation Plan

## Complexity Score: 4/5 (Expert)
## Framework: WebView
## Strategy: Phased Implementation (3 phases)

---

## Phase 4.1.1: Core Infrastructure & Measurement Engine

**Goal:** Audio pass-through working, all three analyzers producing real data, WebView shell connected.

- [ ] **PluginProcessor scaffold**
  - Pass-through `processBlock()` (copy input → output)
  - Declare parameters: `target_genre` (Choice), `analysis_window` (Float)
  - State save/restore via `getStateInformation` / `setStateInformation`

- [ ] **LUFS Meter**
  - K-weighting biquad filter (two-stage: high-shelf at 1681 Hz, high-pass at 38 Hz)
  - Mean-square accumulator per channel
  - Short-term LUFS (3-second sliding window)
  - Integrated LUFS with absolute + relative gating
  - Simple RMS (no weighting)

- [ ] **Spectral Analyzer**
  - 4096-point FFT using `juce::dsp::FFT`
  - Hann windowing
  - Lock-free ring buffer (audio thread → analysis)
  - Band averaging into 6 frequency bands
  - Spectral tilt calculation (linear regression)

- [ ] **Stereo Analyzer**
  - L/R Pearson correlation coefficient
  - Mid/Side width calculation
  - Running average over analysis window

- [ ] **PluginEditor scaffold**
  - WebView2 component setup
  - 30Hz timer callback for metric updates
  - `emitEventIfBrowserIsVisible("analysisUpdate", ...)` with JSON payload
  - Placeholder HTML shell (dark background, "SNIP Bridge" title)

- [ ] **Atomic data bridge**
  - Atomic floats for: LUFS integrated, LUFS short, RMS, correlation, width
  - Atomic float array for 6 spectral bands
  - Audio thread writes, timer thread reads

**Deliverable:** Plugin loads in DAW, passes audio, analyzers produce real numbers visible in WebView console log.

---

## Phase 4.1.2: Genre Engine, Feedback, & Full UI

**Goal:** Genre comparison working, feedback text generating, full WebView UI rendered.

- [ ] **Genre Profile Data**
  - Static struct array with 7 genre profiles
  - Each profile: LUFS range, spectral band targets (6 values), width range, correlation range

- [ ] **Comparison Engine**
  - Per-dimension deviation calculation (Dynamics, Tonality, Width)
  - Percentage match scoring
  - Overall match percentage

- [ ] **Feedback Text Generator**
  - Rule-based text builder
  - Per-dimension status: pass (✓), warning (⚠), fail (✗)
  - Actionable suggestions per dimension
  - Output as formatted multi-line string

- [ ] **Full WebView UI**
  - Dark background (#1a1a1a) with pink (#E91E8C) and cyan (#00D4FF) accents
  - Genre dropdown selector (synced with JUCE parameter)
  - Reaction Time knob/slider
  - LUFS/RMS meter bars (animated)
  - Spectral balance display (6-band bar chart)
  - Stereo correlation/width meter
  - Feedback text display area (scrollable, monospace)
  - "Send to Meetsnip.com" button (large, prominent)
  - Connection status indicator

- [ ] **WebView ↔ C++ bidirectional events**
  - C++ → JS: `analysisUpdate` (metrics JSON at 30Hz)
  - C++ → JS: `feedbackUpdate` (text, at lower rate ~2Hz)
  - JS → C++: `parameterChange` (genre selection, reaction time)
  - JS → C++: `sendToSnip` (button trigger)

**Deliverable:** Full UI rendering with real-time meters, genre comparison, and feedback text updating live.

---

## Phase 4.1.3: Web API Integration & Polish

**Goal:** Snip API bridge working, edge cases handled, production-ready.

- [ ] **HTTP API Client**
  - Build JSON payload: all metrics + genre + feedback + timestamp
  - Async HTTP POST via `juce::URL::createInputStream()` on background thread
  - Connection state machine: Ready → Sending → Sent → (auto-reset after 3s)
  - Error handling with status display (timeout, network error, API error)

- [ ] **API Response Handling**
  - Parse response (success/failure)
  - Display confirmation or error in WebView
  - Optional: deep link to meetsnip.com session

- [ ] **Edge Cases & Polish**
  - Silence detection (don't analyze when input is silent)
  - Reset integrated LUFS (button or auto-reset)
  - Analysis window parameter smoothing (crossfade when changed)
  - WebView resize handling
  - DAW state recall (genre selection, window size persist)

- [ ] **Performance Verification**
  - Confirm zero-latency pass-through
  - Profile CPU usage (target: <2% on modern CPU)
  - Verify lock-free operation (no audio thread blocking)
  - Test at 44.1/48/96 kHz sample rates

**Deliverable:** Complete plugin with API integration, tested and polished.

---

## Dependencies

### Required JUCE Modules
- `juce_audio_basics` — sample buffers, audio types
- `juce_audio_processors` — plugin framework
- `juce_dsp` — FFT, biquad filters
- `juce_gui_extra` — WebBrowserComponent
- `juce_core` — URL, JSON, threading

### External Dependencies
- Meetsnip.com API endpoint (TBD — may need API key / documentation)
- WebView2 runtime (Windows, bundled via JUCE)

---

## Risk Assessment

### High Risk
- **Meetsnip.com API:** No API documentation reviewed yet. May need API key, OAuth, or specific payload format. Mitigation: implement with configurable endpoint URL; build the JSON payload structure first, wire up actual API later.
- **LUFS Accuracy:** ITU-R BS.1770-4 is a specific standard with exact filter coefficients. Incorrect implementation = wrong readings. Mitigation: use published K-weighting coefficients; validate against reference LUFS meter.

### Medium Risk
- **Thread Safety:** Ring buffer between audio and analysis threads must be lock-free. Mitigation: use proven single-producer/single-consumer ring buffer pattern with atomic indices.
- **WebView Performance:** 30Hz metric updates to WebView could cause jank with complex DOM. Mitigation: Canvas-based rendering for meters, minimal DOM updates.

### Low Risk
- **Pass-through Audio:** Trivially correct (buffer copy).
- **Genre Profiles:** Static data, no runtime complexity.
- **Feedback Text:** Simple string concatenation with conditionals.
