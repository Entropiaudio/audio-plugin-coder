# SNIP Bridge - DSP Architecture Specification

## Overview
SNIP Bridge is a **pure pass-through analyzer**. The audio thread copies input to output unchanged. All analysis runs in parallel on the audio data without modifying it.

## Core Components

### 1. Pass-Through Stage
- Copy input buffer directly to output buffer (zero processing)
- Zero latency, zero coloration

### 2. LUFS Meter (ITU-R BS.1770-4)
- **K-weighting filter:** Two-stage biquad (high-shelf + high-pass) per channel
- **Mean-square accumulator:** Per-channel energy over gated blocks
- **Gating:** Absolute gate at -70 LUFS, then relative gate at -10 LU below ungated
- **Short-term LUFS:** 3-second sliding window (overlap 2.9s)
- **Integrated LUFS:** Running gated measurement from reset
- **RMS:** Simple per-block RMS (no K-weighting, no gating)

### 3. Spectral Analyzer (FFT-based)
- **FFT Engine:** 4096-point FFT (juce::dsp::FFT)
- **Windowing:** Hann window applied before transform
- **Band Averaging:** Group FFT bins into analysis bands:
  - Sub (20-60 Hz)
  - Low (60-250 Hz)
  - Low-Mid (250-1000 Hz)
  - Mid (1000-4000 Hz)
  - High-Mid (4000-8000 Hz)
  - High (8000-20000 Hz)
- **Output:** dB magnitude per band, averaged over analysis window
- **Spectral Tilt:** Linear regression slope across band magnitudes

### 4. Stereo Analyzer
- **Correlation:** Pearson correlation coefficient between L and R channels
  - `r = Σ(L·R) / sqrt(Σ(L²) · Σ(R²))`
  - +1.0 = mono, 0.0 = uncorrelated, -1.0 = out of phase
- **Width Estimate:** Derived from mid/side ratio
  - `width = (side_energy / (mid_energy + side_energy)) * 200%`
- **Accumulation:** Running average over analysis window period

### 5. Genre Profile Engine
- **Data Structure:** Static lookup table of 7 genre profiles
- **Each Profile Contains:**
  - Target LUFS range (min, max)
  - Expected spectral band ratios (6 bands, normalized)
  - Expected stereo width range
  - Expected correlation range
- **Comparison:** Calculate deviation of current metrics from target profile
- **Scoring:** Per-dimension score (Dynamics, Tonality, Width) as percentage match

### 6. Feedback Generator
- **Input:** Genre comparison scores + raw metrics
- **Logic:** Rule-based text generation (not AI/ML)
- **Output:** Multi-line text string with actionable mix notes
- **Example output:**
  ```
  Genre: Hip-Hop | Match: 72%
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  ✓ Dynamics: LUFS at -9.2 — within target (-8 to -11)
  ⚠ Tonality: Low-end at 28% — below target (35-45%). Consider boosting sub/bass.
  ✗ Width: Stereo at 78% — too wide for genre. Narrow the low-end stereo image.
  ```

### 7. Web API Bridge
- **Protocol:** HTTP POST to meetsnip.com API endpoint
- **Payload:** JSON snapshot of current analysis state
- **Threading:** Async HTTP request on background thread (NOT audio thread)
- **Status:** Connection state machine (Ready → Sending → Sent/Error)
- **Implementation:** JUCE URL class + WebView JavaScript fetch() as fallback

## Processing Chain

```
                    ┌──────────────────────────────────────────┐
                    │            ANALYSIS BRANCH               │
                    │                                          │
                    │  ┌─────────┐  ┌──────────┐  ┌────────┐  │
                    ├─→│ LUFS    │  │ Spectral │  │ Stereo │  │
                    │  │ Meter   │  │ Analyzer │  │ Corr.  │  │
                    │  └────┬────┘  └────┬─────┘  └───┬────┘  │
                    │       │            │            │        │
                    │       └────────┬───┘────────────┘        │
                    │                ▼                          │
                    │       ┌────────────────┐                 │
                    │       │ Genre Compare  │                 │
                    │       └───────┬────────┘                 │
                    │               ▼                          │
                    │       ┌────────────────┐                 │
                    │       │ Feedback Gen   │──→ WebView UI   │
                    │       └────────────────┘                 │
                    └──────────────────────────────────────────┘

Input ──────────────────────────────────────────────────────→ Output
                     (PURE PASS-THROUGH)
```

## Parameter Mapping

| Parameter | Component | Function | Range |
|:----------|:----------|:---------|:------|
| `target_genre` | Genre Profile Engine | Selects reference profile for comparison | 0-6 (enum) |
| `analysis_window` | All Analyzers | Controls smoothing/averaging window length | 0.5-10.0 seconds |

## Thread Safety Model

| Thread | Operations | Synchronization |
|:-------|:-----------|:----------------|
| Audio Thread | Pass-through copy, feed samples to analyzers | Lock-free ring buffers for FFT input |
| Timer Thread (30Hz) | Read analyzer results, update WebView | Atomic reads of analysis output |
| Background Thread | HTTP POST to Snip API | Async, triggered by button click from WebView JS |

### Audio Thread Contract
- **MUST:** Copy input to output
- **MUST:** Push samples into analysis ring buffers
- **MUST NOT:** Allocate memory, perform HTTP calls, or block
- **MUST NOT:** Read from or write to WebView state

### Ring Buffer Design
- Single-producer (audio thread), single-consumer (analysis timer)
- Size: 4096 * 4 = 16384 samples (enough for FFT + overlap)
- Lock-free using atomic read/write indices

## Data Flow to WebView

Analysis results flow to WebView via `juce::WebBrowserComponent::emitEventIfBrowserIsVisible()`:
- Event: `"analysisUpdate"` fired at 30Hz from timer callback
- Payload: JSON object with all current metrics
- WebView JS receives via `window.__JUCE__.backend.addEventListener("analysisUpdate", ...)`

Button press from WebView flows back via:
- `window.__JUCE__.backend.emitEvent("sendToSnip", data)`
- C++ receives and dispatches async HTTP POST

## Complexity Assessment

**Score: 4/5 (Expert)**

**Rationale:**
- LUFS measurement requires ITU-R BS.1770-4 compliant K-weighting and gating — non-trivial DSP
- FFT spectral analysis with proper windowing and band averaging
- Real-time thread safety with lock-free ring buffers between audio and analysis threads
- WebView ↔ C++ bidirectional communication for metrics + API trigger
- Async HTTP integration from plugin context
- Genre comparison engine with 7 multi-dimensional profiles
- Rule-based feedback text generation

**What keeps it from 5:** No synthesis, no ML/AI inference, no complex feedback topologies. The DSP is measurement-only (no signal modification), which simplifies the audio thread significantly.
