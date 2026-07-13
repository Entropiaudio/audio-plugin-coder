# Entropan — Parameter Specification

Definitive control list. IDs are APVTS parameter IDs.
Architecture: 6 band slots, each with dedicated modulator (see creative-brief).

## Global parameters

| ID | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `seed` | Seed | Int | 1 – 128 | 1 | — |
| `mix` | Mix | Float | 0.0 – 100.0 | 100.0 | % |
| `output` | Level | Float | -24.0 – +12.0 | 0.0 | dB |
| `bypass` | Bypass | Bool | off / on | off | — |

- `mix` — **master depth**: scales every band's `depth` at once. 100% = depths as set; 0% = all motion frozen at center. NOT a dry/wet blend — unlifted spectrum passes untouched by design.
- `seed` — reproducible random streams for S&H/Drift/Chaos modes.
- **Re-roll (CHAOS) button** — UI-only native event (not a parameter, not automatable): re-deals random state of all random-type modulators.
- There is no separate ENTROPY macro (dropped — `mix` covers the master-depth role).

## Per-band slot parameters — ×6 slots, prefix `b1_` … `b6_`

| ID suffix | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `_on` | Enable | Bool | off / on | b1: on, b2–b6: off | — |
| `_freq` | Center | Float | 20.0 – 20000.0 (log) | 200 / 500 / 1k / 2k / 5k / 10k | Hz |
| `_width` | Width | Float | 0.1 – 4.0 | 1.0 | oct |
| `_lift` | Lift | Float | 0.0 – 100.0 | 100.0 | % |
| `_depth` | Depth | Float | 0.0 – 100.0 | 50.0 | % |
| `_mode` | Mod Type | Choice | Sine, Triangle, S&H, Drift, Chaos, Steps | Sine | — |
| `_rate` | Rate | Float | 0.02 – 8.0 (log) | 0.5 | Hz |
| `_sync` | Sync | Bool | off / on | on | — |
| `_div` | Interval | Choice | 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bar, 4 bar | 1/2 | note |
| `_inertia` | Inertia | Float | 0.0 – 100.0 | 60.0 | % |
| `_phase` | Phase | Float | 0.0 – 360.0 | 0.0 | ° |

- `_lift` — **the bell height** from the spectrum-display gesture: extraction amount — how much of the band region is routed into the pan path. 0% = band passes dry (null), 100% = fully routed. Pan-only: no gain change (lifted + unlifted portions always sum to unity when centered).
- `_depth` — pan modulation amplitude for the band (how far the lifted portion swings). Scaled globally by `mix`.
- `_rate` used when `_sync` off; `_div` when on.
- `_inertia` — slew between pan targets: 100% = liquid glide, 0% = instant snap.
- `_phase` — offset for periodic modes (Sine/Triangle/Steps); ignored by S&H/Drift/Chaos.

**Total: 4 global + 6 × 11 = 70 APVTS parameters.**

## Step sequencer data (non-parameter state)

Per band, Steps mode: **2–16 steps, each step's value = pan position** (-100…+100).

- Stored in the plugin `ValueTree` state blob, **NOT** as APVTS parameters (16 steps × 6 bands = 96 extra params would explode the layout and pollute host automation lists).
- Step count + step values edited in the UI step-sequencer panel, saved with session and presets via `getStateInformation`/`setStateInformation`.
- Not host-automatable by design; `_rate`/`_div` still clock the sequencer and remain automatable.

## UI section mapping (Chaosverb-clone layout)

| UI region | Contents |
| :--- | :--- |
| Main area | Spectrum analyzer + lift-gesture bell curves (6 band handles; bell height = `_lift`). Disabled bands stay visible as dimmed ghosts ("off vibe") — selectable, draggable, no glow/trace |
| Left columns | Selected band inspector: freq, width, lift, depth, mode |
| Right panel | Selected band modulator engine: rate/interval + sync, inertia, phase (Chaosverb-slider style); step-sequencer editor when Steps mode active |
| Top bar | CHAOS re-roll button, preset select, seed display |
| Bottom/output row | mix (master depth), output, bypass |
