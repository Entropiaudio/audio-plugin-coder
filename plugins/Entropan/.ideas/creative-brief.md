# Entropan — Creative Brief

**Brand:** Entropia Audio
**Concept:** Lift a band. Let entropy take it.

## Hook

> **Entropan — lift a band, let entropy take it.**
> Grab any region of the spectrum — Spectre-style — and hand it to its own chaos engine. One band gently breathing left and right, or six bands scattering to six different clocks. You choose how much order survives.

## Description

Entropan is a band-targeted spectral panner. The user **lifts a band** on a spectrum display (a bell-curve gesture, like Wavesfactory Spectre's EQ interaction) — but instead of boosting gain, the lift hands that frequency region to a panning modulator. Everything outside lifted bands passes through untouched and stays dead-center.

**Lift is pan-only:** lift height = modulation depth for that band. No gain change.

### DSP architecture (Option B — parallel time-domain filter bank)

- Per active band: an allpass-matched crossover pair (Linkwitz-Riley around the band's edges) extracts the band from the signal, leaving a complementary residual.
- The extracted band is panned with an equal-power law, then summed back with the residual.
- Panning and slew run at audio rate — no hop-grid quantization, near-zero latency, no spectral smearing.
- STFT/FFT exists **only** for the analyzer display in the UI, never in the audio path.
- Candidate topology: serial extraction cascade — band N is extracted from the residual of band N-1, so overlapping bands stay complementary and the full sum nulls. Final call in `/plan`.

### Per-band modulator (the "in control ↔ going crazy" axis)

Every band slot (max **6**) has its own dedicated modulation engine:

| Mode | Character |
| :--- | :--- |
| Sine | Classic smooth auto-pan |
| Triangle | Linear sweep auto-pan |
| S&H | Stepped random jumps each interval |
| Drift | Smoothed random walk — organic wander, never repeats |
| Chaos | Lorenz/logistic chaotic oscillator — semi-periodic, brand-defining |
| Steps | Step sequencer — each step's value IS the pan position |

Per band: rate (free Hz or tempo-synced note division), depth (= lift height), inertia (slew: liquid ↔ snap), phase offset. Global **ENTROPY** macro scales all band depths at once; **CHAOS re-roll button** re-deals the random state of every random-type modulator; **Seed** makes runs reproducible.

## UI Direction (predetermined by user)

**Clone the Chaosverb v1.0.18 GUI** (source: `/Users/nbs/Developer/plugin-freedom-system/plugins/Chaosverb/Source/ui/public/index.html`, single-file WebView UI, ~3.7k lines, inline CSS/JS + Fraunces/Montserrat/JetBrains Mono fonts + `js/juce` interop lib).

- Same dark warm background, color-coded sections with colored uppercase headers, arc-style knobs with per-section accent colors, top bar with logo/brand/version, action button (CHAOS-style re-roll), preset selector, right-hand feature panel.
- Adapt: main area = spectrum display with lift-gesture bells; left columns = selected band's inspector (freq/width/depth/mode); right panel = selected band's modulator engine (Chaosverb-slider style) + step-sequencer editor when Steps mode is active; top bar = CHAOS re-roll, preset select, ENTROPY master.
- UI framework consequence: **WebView** (JUCE 8 `WebBrowserComponent`), same architecture as Chaosverb's `PluginEditor` glue (~770 lines) — formal selection recorded in `/plan`.

## Success criteria

- **Null test:** no bands lifted → output ≈ input (time-domain complementary split only engages per active band).
- One band, Sine mode, high inertia → smooth, musical auto-pan of that region only; rest of the spectrum image untouched.
- 6 bands with mixed Chaos/S&H/Steps modes → wild motion, still mix-safe: residual spectrum stays centered, no level pumping.
- Automation-safe: toggling bands on/off produces no clicks (crossfaded engage).
