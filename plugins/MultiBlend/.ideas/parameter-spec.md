# MultiBlend - Parameter Specification

## Global Parameters (Automatable)

| ID | Name | Type | Range | Default | Unit | Notes |
|:---|:-----|:-----|:------|:--------|:-----|:------|
| `global_dry_wet` | Global Dry/Wet | Float | 0.0 - 100.0 | 100.0 | % | Blends full multiband output vs original dry input |
| `global_input_gain` | Input Gain | Float | -24.0 - 24.0 | 0.0 | dB | Pre-crossover input level |
| `global_output_gain` | Output Gain | Float | -24.0 - 24.0 | 0.0 | dB | Post-mix output level |
| `linear_phase` | Linear Phase | Bool | On/Off | Off | — | Enables linear phase crossover (higher latency) |

## Crossover Parameters (Automatable)

| ID | Name | Type | Range | Default | Unit | Notes |
|:---|:-----|:-----|:------|:--------|:-----|:------|
| `crossover_low_mid` | Low-Mid Frequency | Float | 20.0 - 20000.0 | 200.0 | Hz | Dynamic range: expands when adjacent band disabled |
| `crossover_mid_high` | Mid-High Frequency | Float | 20.0 - 20000.0 | 3000.0 | Hz | Dynamic range: expands when adjacent band disabled |

### Dynamic Crossover Behavior

| Scenario | Low-Mid Range | Mid-High Range |
|:---------|:-------------|:---------------|
| All bands active | 20 - 20000 Hz (clamped below mid-high) | 20 - 20000 Hz (clamped above low-mid) |
| Low band off | Mid stretches to 20 Hz (low-mid crossover hidden) | Normal |
| High band off | Normal | Mid stretches to 20 kHz (mid-high crossover hidden) |
| Low + High off | Mid = full range, both crossovers hidden | Mid = full range, both crossovers hidden |
| Mid band off | Low stretches up | High stretches down (single shared crossover) |

## Per-Band Parameters (x3: Low, Mid, High)

Replace `X` with `low`, `mid`, or `high`.

| ID | Name | Type | Range | Default | Unit | Notes |
|:---|:-----|:-----|:------|:--------|:-----|:------|
| `band_X_dry_wet` | Band Dry/Wet | Float | 0.0 - 100.0 | 100.0 | % | Blends processed band vs unprocessed band signal |
| `band_X_input_gain` | Band Input | Float | -24.0 - 24.0 | 0.0 | dB | Gain before plugin chain |
| `band_X_output_gain` | Band Output | Float | -24.0 - 24.0 | 0.0 | dB | Gain after plugin chain |
| `band_X_solo` | Solo | Bool | On/Off | Off | — | Solo this band (mutes others) |
| `band_X_mute` | Mute | Bool | On/Off | Off | — | Mutes this band's output |
| `band_X_bypass` | Bypass | Bool | On/Off | Off | — | Bypasses band entirely; releases crossover range to neighbors |
| `band_X_slope` | Crossover Slope | Float | 6.0 - 96.0 | 24.0 | dB/oct | Continuous per-band crossover slope |
| `band_X_ms_morph` | M/S Morph | Float | 0.0 - 1.0 | 0.5 | — | 0.0=100% Mid, 0.5=Stereo, 1.0=100% Side. Affects all slots in this band |

## Per-Slot Parameters (x5 per band = x15 total)

Replace `X` with `low`, `mid`, or `high`. Replace `Y` with `1`-`5`.

| ID | Name | Type | Range | Default | Unit | Notes |
|:---|:-----|:-----|:------|:--------|:-----|:------|
| `band_X_slot_Y_dry_wet` | Slot Dry/Wet | Float | 0.0 - 100.0 | 100.0 | % | Blends slot output vs slot input (parallel mix). Shows percentage in knob |
| `band_X_slot_Y_bypass` | Slot Bypass | Bool | On/Off | Off | — | Bypasses this slot (signal passes through) |

## Non-Automatable / UI-Only Elements

| ID | Name | Type | Notes |
|:---|:-----|:-----|:------|
| `band_X_slot_Y_plugin` | Loaded Plugin | Reference | VST3 plugin identifier; click to open plugin browser or hosted GUI |
| `spectrum_display` | Spectrum Analyzer | Visualization | Real-time frequency display showing band splits and levels |
| `crossover_handles` | Crossover Drag Handles | UI Control | Draggable frequency dividers on spectrum display |

## Parameter Count Summary

| Scope | Count | Formula |
|:------|:------|:--------|
| Global | 4 | dry/wet + in + out + linear phase |
| Crossover | 2 | low-mid freq + mid-high freq |
| Per-band | 27 | 9 params x 3 bands (added slope + ms_morph, removed routing) |
| Per-slot | 30 | 2 params x 5 slots x 3 bands (removed routing) |
| **Total automatable** | **63** | |
