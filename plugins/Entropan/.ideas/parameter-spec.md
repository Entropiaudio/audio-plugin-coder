# Entropan — Parameter Specification

Definitive control list. IDs are APVTS parameter IDs.
Architecture: 6 band slots, each with dedicated modulator (see creative-brief).

## Global parameters

| ID | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `seed` | Seed | Int | 1 – 128 | 1 | — |
| `amount` | Amount | Float | 0.0 – 100.0 | 100.0 | % |
| `speed` | Global Speed | Choice | ÷4, ÷2, ×1, ×2, ×3, ×4 | ×1 | × |
| `output` | Level | Float | -24.0 – +12.0 | 0.0 | dB |
| `bypass` | Bypass | Bool | off / on | off | — |

- `amount` (formerly "mix") — **master depth**: scales every band's `depth` at once. 100% = depths as set; 0% = all motion frozen at center. NOT a dry/wet blend — unlifted spectrum passes untouched by design.
- `speed` — **global rate multiplier** (header, bright): multiplies every band's effective rate — synced divisions and free Hz alike. Whole plugin slower/faster with one control.
- `seed` — reproducible random streams for S&H/Drift/Chaos modes. UI: settings popover (removed from header in favor of Global Speed).
- **Re-roll (CHAOS) button** — UI-only native event (not a parameter, not automatable): re-deals random state of all random-type modulators.
- There is no separate ENTROPY macro (dropped — `amount` covers the master-depth role).

## Per-band slot parameters — ×6 slots, prefix `b1_` … `b6_`

| ID suffix | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `_on` | Enable | Bool | off / on | b1: on, b2–b6: off | — |
| `_freq` | Center | Float | 20.0 – 20000.0 (log) | 200 / 500 / 1k / 2k / 5k / 10k | Hz |
| `_width` | Width | Float | 0.1 – 4.0 | 1.0 | oct |
| `_lift` | Lift | Float | 0.0 – 100.0 | 100.0 | % |
| `_depth` | Depth | Float | 0.0 – 100.0 | 50.0 | % |
| `_mode` | Mod Type | Choice | Sine, Triangle, S&H, Drift, Chaos, Steps | Sine | — |
| `_rate` | Rate | Float | 0.02 – 1000.0 (log) | 0.5 | Hz |
| `_ratemode` | Rate Mode | Choice | Sync, Free, MIDI | Sync | — |
| `_div` | Interval | Choice | 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bar, 4 bar | 1/2 | note |
| `_inertia` | Inertia | Float | 0.0 – 100.0 | 60.0 | % |
| `_phase` | Phase | Float | 0.0 – 360.0 | 0.0 | ° |

- `_lift` — **the bell height** from the spectrum-display gesture: extraction amount — how much of the band region is routed into the pan path. 0% = band passes dry (null), 100% = fully routed. Pan-only: no gain change (lifted + unlifted portions always sum to unity when centered).
- `_depth` — pan modulation amplitude for the band (how far the lifted portion swings). Scaled globally by `mix`.
- `_ratemode` — **Sync**: `_div` note divisions from host tempo. **Free**: `_rate` in Hz, reaching **audio rate** (up to 1 kHz — tremolo/rotary/AM territory). **MIDI**: rate follows the last played MIDI note's frequency (mono, last-note priority; inertia doubles as glide). All modes multiplied by global `speed`.
- `_inertia` — slew between pan targets: 100% = liquid glide, 0% = instant snap.
- `_phase` — offset for periodic modes (Sine/Triangle/Steps); ignored by S&H/Drift/Chaos.

**Total: 5 global + 6 × 11 = 71 APVTS parameters.**

## Step sequencer data (non-parameter state)

Per band, Steps mode: **2–16 slots; each slot = { subdiv, vals[] }** — SLOT-STABLE ratcheting.

- Every slot occupies exactly one base interval (`_div` when synced / `_rate` cycle fraction when free) — the grid never shifts.
- `subdiv` ∈ **1, 2, 4**: divides that slot in place into equal sub-steps (base 1/8 → sub-steps at 1/16 or 1/32); **neighboring slots stay in place**.
- `vals[subdiv]` = pan position (-100…+100) per sub-step, each independently paintable.
- Stored in the plugin `ValueTree` state blob, **NOT** as APVTS parameters (would explode the layout and pollute host automation lists).
- Slot count, values and subdivisions edited in the UI step-sequencer panel (alt-click = cycle subdiv 1→2→4), saved with session and presets via `getStateInformation`/`setStateInformation`.
- **Step pattern presets:** factory patterns (Ramp Up/Down, Pyramid, Ping-Pong, Ratchet LR) + user-saved named patterns (persisted to a small user file, e.g. `~/Library/Application Support/Entropia Audio/Entropan/step-patterns.json`); selectable per band from the step editor.
- Not host-automatable by design; `_rate`/`_div` still clock the sequencer and remain automatable.

## UI-only features (no parameters)

- **Modulation scope** — oscilloscope of selected band's modulator output; fed by processor→UI telemetry event (~30 Hz per-band mod value), display only.
- **Floating band quick-menu** — RATE + DEPTH mini-sliders floating next to the selected band's pan trace; binds to the same `bN_rate`/`bN_div`/`bN_depth` params (additional access point, not new params). Draggable by its grip bar — user-parked position sticks.
- **Apex on/off** — clean click on a bell's tip toggles `bN_on`; no dedicated button.
- **MIDI input** — plugin accepts MIDI (`acceptsMidi = true`) to drive MIDI rate mode; notes are consumed, not passed through.

## UI section mapping (Chaosverb-clone layout)

| UI region | Contents |
| :--- | :--- |
| Main area | Spectrum analyzer + lift-gesture bell curves (6 band handles; bell height = `_lift`). Disabled bands stay visible as dimmed ghosts ("off vibe") — selectable, draggable, no glow/trace |
| Left columns | Selected band inspector: freq, width, lift, depth, mode |
| Right panel | Selected band modulator engine: rate/interval + sync, inertia, phase (Chaosverb-slider style); step-sequencer editor when Steps mode active |
| Top bar | CHAOS re-roll button, preset select, seed display |
| Bottom/output row | mix (master depth), output, bypass |
