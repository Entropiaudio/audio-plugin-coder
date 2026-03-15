# SNIP Bridge - UI Specification v1

## Layout
- **Window:** 1000 x 500 px
- **Layout Type:** 4-column wide with header bar and footer action bar
- **Framework:** WebView (HTML/CSS/JS)

## Grid Structure

```
┌──────────────────────────────────────────────────────────────┐
│  HEADER BAR (48px)                                           │
│  Logo/Title    |    Genre Dropdown    |    Reaction Knob     │
├──────────┬──────────┬───────────┬────────────────────────────┤
│  COL 1   │  COL 2   │  COL 3    │  COL 4                    │
│ DYNAMICS │ TONALITY │ STEREO    │  MIX FEEDBACK              │
│  200px   │  220px   │  180px    │  ~360px (flex)             │
│          │          │           │                            │
│  LUFS    │  6-band  │  Corr.    │  Genre: Hip-Hop            │
│  meter   │  bars    │  meter    │  Match: 72%                │
│  (vert)  │  + target│  + width  │  ━━━━━━━━━━━━━━            │
│          │  lines   │  arc      │  ✓ Dynamics: ...           │
│  RMS     │          │           │  ⚠ Tonality: ...          │
│  meter   │          │           │  ✗ Width: ...              │
│  (vert)  │          │           │                            │
├──────────┴──────────┴───────────┴────────────────────────────┤
│  FOOTER BAR (56px)                                           │
│  [Status LED]  Connection Status  │  [ SEND TO MEETSNIP.COM ]│
└──────────────────────────────────────────────────────────────┘
```

## Sections

### Header Bar (48px height)
- **Left:** "SNIP BRIDGE" logo text (bold, 16px, white, letter-spacing: 3px)
- **Center:** Genre dropdown selector (styled dark select with pink accent border)
- **Right:** Reaction Time slider (horizontal, labeled "REACT" with value display)

### Column 1: Dynamics (200px wide)
- **Section Label:** "DYNAMICS" (10px, uppercase, letter-spacing 2px, secondary text)
- **LUFS Meter:** Vertical bar, 30px wide, 220px tall
  - Fill color: gradient from cyan (#00D4FF) at bottom to pink (#E91E8C) at top
  - Background: #1A1A1A
  - Genre target zone shown as semi-transparent overlay band
- **RMS Meter:** Second vertical bar, 30px wide, 220px tall, next to LUFS
  - Same gradient style
- **Readouts below meters:**
  - "LUFS" label + value (e.g., "-9.2") in cyan monospace
  - "RMS" label + value (e.g., "-11.4") in cyan monospace
  - "Integrated" label + value in smaller secondary text

### Column 2: Tonality (220px wide)
- **Section Label:** "TONALITY" (same style as Dynamics)
- **6 Vertical Bars:** One per frequency band
  - Sub (20-60), Low (60-250), L-Mid (250-1k), Mid (1k-4k), H-Mid (4k-8k), High (8k-20k)
  - Bar width: 24px each, gap: 8px
  - Fill: cyan (#00D4FF) with opacity based on energy level
  - **Target Line:** Horizontal marker on each bar showing genre target level
    - Line color: pink (#E91E8C), 2px thick, dashed
    - Gap above/below target = deviation
  - **Color coding:** Bar color shifts green→yellow→red based on deviation from target
- **Band labels:** Below each bar (abbreviated: "Sub", "Low", "LM", "Mid", "HM", "Hi")

### Column 3: Stereo Width (180px wide)
- **Section Label:** "STEREO" (same style)
- **Correlation Meter:** Horizontal arc or semicircle
  - Range: -1.0 (left) to +1.0 (right)
  - Needle/indicator dot in cyan
  - Danger zone (< 0) highlighted in red
  - Safe zone (0.5 - 1.0) in green
- **Width Display:** Circular arc or percentage bar
  - 0% (mono) to 200% (hyper-wide)
  - Current value shown as filled arc
- **Readouts:**
  - "CORR" + value (e.g., "0.72")
  - "WIDTH" + value (e.g., "65%")

### Column 4: Mix Feedback (~360px, flex)
- **Section Label:** "MIX FEEDBACK" (same style)
- **Header line:** "Genre: [Name] | Match: [XX]%" in pink accent
- **Separator:** Thin horizontal line (#2A2A2A)
- **Feedback lines:** Monospace text, line-by-line
  - ✓ prefix = green (#00E676)
  - ⚠ prefix = yellow (#FFD600)
  - ✗ prefix = red (#FF1744)
- **Scrollable:** If feedback exceeds area, scroll with custom dark scrollbar
- **Font:** 13px monospace (JetBrains Mono or system monospace)

### Footer Bar (56px height)
- **Left:** Status LED (8px circle: green=ready, yellow=sending, red=error) + status text
- **Right:** "SEND TO MEETSNIP.COM" button
  - Width: 280px, Height: 40px
  - Background: #E91E8C (pink)
  - Text: white, bold, 14px, uppercase, letter-spacing 1px
  - Hover: brightness 1.2
  - Active/Sending: pulsing animation
  - Disabled: opacity 0.5 (when no analysis data)

## Controls

| Parameter | Type | Position | Visual Style |
|:----------|:-----|:---------|:-------------|
| `target_genre` | Dropdown | Header center | Dark select, pink border on focus, white text |
| `analysis_window` | Slider | Header right | Horizontal, cyan track, pink thumb, value display |

## Interaction States

### Send Button States
- **Ready:** Solid pink, "SEND TO MEETSNIP.COM"
- **Sending:** Pulsing pink glow, "SENDING..."
- **Sent:** Brief green flash, "SENT ✓" (reverts after 3s)
- **Error:** Red background, "ERROR — RETRY" (reverts after 5s)

### Genre Dropdown
- Custom styled `<select>` matching dark theme
- Options: Hip-Hop / Trap, Pop, EDM / Dance, Rock / Alt, R&B / Soul, Lo-Fi / Ambient, Classical / Jazz
- On change: immediately updates genre target overlays on all meters + regenerates feedback

### Reaction Time Slider
- Label: "REACT"
- Range: 0.5s - 10.0s
- Value display: "3.0s" next to slider
- Faster = more responsive meters, slower = more stable readings
