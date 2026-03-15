# SNIP Bridge - Parameter Specification

## Parameters

| ID | Name | Type | Range | Default | Unit | Notes |
|:---|:-----|:-----|:------|:--------|:-----|:------|
| `target_genre` | Target Genre | Choice | 0-6 | 0 | — | Dropdown: Hip-Hop, Pop, EDM, Rock, R&B, Lo-Fi, Classical |
| `analysis_window` | Reaction Time | Float | 0.5 - 10.0 | 3.0 | seconds | How fast metrics update; shorter = more reactive, longer = more stable |

## Read-Only Displays (Non-Automatable)

| ID | Name | Type | Display | Notes |
|:---|:-----|:-----|:--------|:------|
| `lufs_integrated` | Integrated LUFS | Float | -60 to 0 dBFS | Running integrated loudness |
| `lufs_short` | Short-Term LUFS | Float | -60 to 0 dBFS | 3-second window loudness |
| `rms_level` | RMS Level | Float | -60 to 0 dBFS | Current RMS |
| `spectral_balance` | Spectral Balance | Text | Low/Mid/High ratio | Tonal profile vs genre target |
| `stereo_correlation` | Stereo Correlation | Float | -1.0 to 1.0 | Pearson correlation of L/R channels |
| `stereo_width` | Stereo Width | Float | 0 to 200 | Percentage width estimate |
| `analysis_feedback` | Mix Feedback | Text | Multi-line | AI-generated text notes comparing mix to genre profile |

## Genre Profiles

Each genre profile defines target ranges:

| Genre | Target LUFS | Spectral Tilt | Low-End Ratio | HF Energy | Stereo Width |
|:------|:------------|:--------------|:--------------|:----------|:-------------|
| Hip-Hop / Trap | -8 to -11 | Heavy low | 35-45% | Low-Mid | Moderate |
| Pop | -9 to -12 | Balanced | 25-35% | High | Wide |
| EDM / Dance | -7 to -10 | Sub-heavy | 40-50% | High | Wide |
| Rock / Alternative | -10 to -14 | Mid-forward | 20-30% | Mid-High | Moderate |
| R&B / Soul | -10 to -13 | Warm | 30-40% | Low-Mid | Moderate-Wide |
| Lo-Fi / Ambient | -14 to -20 | Rolled-off HF | 25-35% | Low | Narrow-Moderate |
| Classical / Jazz | -16 to -24 | Flat/Natural | 15-25% | Mid | Natural |

## UI Elements (Non-Parameter)

| ID | Name | Type | Notes |
|:---|:-----|:-----|:------|
| `btn_send_snip` | Send to Meetsnip.com | Button | Trigger: packages analysis snapshot and sends via HTTP POST to Snip API |
| `meter_dynamics` | Dynamics Meter | Visualization | Real-time LUFS/RMS bar meter |
| `meter_spectrum` | Spectral Display | Visualization | Frequency balance bars or curve |
| `meter_stereo` | Stereo Meter | Visualization | Correlation/width indicator |
| `status_indicator` | Connection Status | LED/Text | Shows API connection state (Ready / Sending / Sent / Error) |
