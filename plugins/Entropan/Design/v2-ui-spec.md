# Entropan — UI Specification v2

**v2 change:** bell height = `lift` (extraction amount); per-band DEPTH knob = pan amplitude; global MIX = master depth (ENTROPY dropped).

Cloned from **Chaosverb v1.0.18** design system (`/Users/nbs/Developer/plugin-freedom-system/plugins/Chaosverb/Source/ui/public/index.html`). Layout adapted: spectrum-top / controls-bottom.

## Window

- Default: **900 × 560 px** (Chaosverb pattern: saved size in ValueTree, resizable).
- Resize limits: 720×448 → 1620×1008 (16:10-ish, keep aspect via shell zoom like Chaosverb `currentScale`).
- Settings popover (gear, bottom-right): size presets SM 720×448 / MD 900×560 / LG 1200×747 / XL 1500×933, brightness, tooltips toggle.

## Layout regions

```
┌────────────────────────────────────────────────────────────────┐
│ HEADER (56px): title+eyebrow | ↶ CHAOS ↷  preset ▾ | seed ‹ › │
├────────────────────────────────────────────────────────────────┤
│ SPECTRUM (flex ~62%): analyzer + grid + 6 lift bells            │
│   band chips (B1…B6) top-left · dB scale right · Hz scale bottom│
├────────────────────────────────────────────────────────────────┤
│ BOTTOM STRIP (~180px): 3 sections                               │
│  BAND [freq width lift depth + ON]  MOD [mode chips; rate/     │
│  (selected band color)          phase h-sliders + SYNC | step   │
│                                 editor when Steps]  OUTPUT      │
│                                 [width mix level + BYPASS]      │
└────────────────────────────────────────────────────────────────┘
```

## Spectrum display (main interaction)

- Canvas, log-freq X (20 Hz–20 kHz), dB Y (-60…0 analyzer, lift overlay normalized).
- Analyzer: filled magnitude spectrum, `--bg-elevated`→transparent gradient fill, subtle line.
- **Bells:** each enabled band = bell curve; height = `lift` (extraction amount), center X = `freq`, width = `width` oct. Fill = band color @ 18% alpha, stroke = band color, handle dot at apex. **Disabled (bypassed) band = ghost:** bell stays visible, dimmed (~30% stroke, faint fill, no glow, no pan trace) — off vibe; still click-selectable and draggable; double-click re-enables.
- Live pan trace: thin horizontal indicator inside each bell showing current mod pan position (animates; preview fakes it).
- Gestures:
  - Drag handle: X = freq, Y = lift.
  - Alt-drag / vertical scroll on handle: width.
  - Double-click empty area: enable next free band slot there.
  - Double-click handle: disable band.
  - Click bell/chip: select band (bottom strip follows).
- Band chips row (top-left overlay): `B1…B6`, filled = enabled, ring = selected; click select, ⌥click toggle enable.

## Bottom strip sections

### BAND (selected band accent color)
| Control | Type | Param |
| :--- | :--- | :--- |
| Center | arc knob 56px | `bN_freq` |
| Width | arc knob 56px | `bN_width` |
| Lift | arc knob 56px | `bN_lift` |
| Depth | arc knob 56px | `bN_depth` |
| ON | Chaosverb `flutter-toggle` style | `bN_on` |

### MOD (selected band)
- Mode selector: 6 text chips — SINE TRI S&H DRIFT CHAOS STEPS (`bN_mode`), selected = band color.
- H-sliders (Chaosverb `hslider-row` style):
  - RATE + SYNC toggle (`bN_rate`/`bN_div`/`bN_sync`) — synced shows note div, free shows Hz.
  - INERTIA (`bN_inertia`).
  - PHASE (`bN_phase`) — dimmed/disabled for S&H/Drift/Chaos.
- **Steps mode:** slider stack swaps to step editor — N vertical bars (2–16), bar value = pan (-100…+100, center line), drag to paint; `-`/`+` step count; RATE/SYNC row stays.

### OUTPUT (Chaosverb output styling, `--section-output`)
| Control | Type | Param |
| :--- | :--- | :--- |
| Mix (master depth) | arc knob | `mix` |
| Level | arc knob | `output` |
| Bypass | `bypass-btn` | `bypass` |

## Header

- Left: `.plugin-title` "Entropan" (Fraunces 300), `.plugin-eyebrow` "ENTROPIA AUDIO · V0.1.0".
- Center: `.chaos-row` — undo ↶, **CHAOS** re-roll (`mutate-btn` style; fires re-roll event + flash overlay), redo ↷; preset dropdown below.
- Right: **SEED** numeric stepper (mono text, ‹ › arrows).

## Controls inventory (visible at once)

- 6 arc knobs (4 band + 2 output)
- 3 h-sliders (or step editor) + sync/mode toggles
- 6 mode chips, 6 band chips, ON/BYPASS/CHAOS/undo/redo/preset/seed/gear
- 1 spectrum canvas

## Parameter ↔ element ID convention

`knob_<paramID>`, `hslider_<paramID>`, `chip_mode_<idx>`, `band_chip_<n>`, `spectrum` canvas. Selected-band controls rebind to `b<sel>_*` params on selection change (JS-side indirection — same widgets, swapped param handles).

## Style notes

- All tokens inherited from Chaosverb (see v2-style-guide.md). No new surface colors.
- Section accent usage: BAND/MOD sections tint to **selected band hue**; OUTPUT stays `--section-output` grey; header accent = copper.
- Knob rendering: canvas arc -135°→+135°, 3-pass glow stroke in accent, warm disc face `#1f1c19`, white indicator dot, JetBrains Mono value below (Chaosverb `buildKnob` clone).
- Motion: `--t-fast/base/slow` + `--ease-editorial`; CHAOS click = `mutation-flash` overlay reuse.
