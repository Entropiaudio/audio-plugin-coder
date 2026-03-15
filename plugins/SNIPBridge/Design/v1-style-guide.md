# SNIP Bridge - Style Guide v1

## Color Palette

### Core Colors
| Role | Hex | Usage |
|:-----|:----|:------|
| Background | `#0D0D0D` | Main window background |
| Surface | `#1A1A1A` | Panel backgrounds, meter backgrounds |
| Surface Light | `#242424` | Hover states, elevated panels |
| Border | `#2A2A2A` | Subtle dividers, panel borders |

### Accent Colors
| Role | Hex | Usage |
|:-----|:----|:------|
| Primary (Pink) | `#E91E8C` | Send button, genre header, target lines, interactive highlights |
| Secondary (Cyan) | `#00D4FF` | Meter fills, data readouts, slider track |
| Cyan Dim | `#00D4FF80` | Meter fill at lower levels (50% opacity) |

### Semantic Colors
| Role | Hex | Usage |
|:-----|:----|:------|
| Success | `#00E676` | ✓ pass indicators, "on target" bars, ready LED |
| Warning | `#FFD600` | ⚠ warning indicators, "close" deviation bars |
| Error | `#FF1744` | ✗ fail indicators, "off target" bars, error LED, phase danger |
| Sending | `#FFD600` | Sending state LED |

### Text Colors
| Role | Hex | Usage |
|:-----|:----|:------|
| Primary Text | `#FFFFFF` | Headings, values, labels |
| Secondary Text | `#808080` | Section labels, sublabels |
| Muted Text | `#4A4A4A` | Disabled states, placeholder |

## Typography

### Font Stack
```css
--font-display: 'Inter', 'SF Pro Display', -apple-system, sans-serif;
--font-mono: 'JetBrains Mono', 'SF Mono', 'Fira Code', monospace;
```

### Scale
| Element | Font | Size | Weight | Spacing | Transform |
|:--------|:-----|:-----|:-------|:--------|:----------|
| Plugin Title | Display | 16px | 700 | 3px | Uppercase |
| Section Label | Display | 10px | 600 | 2px | Uppercase |
| Value Readout | Mono | 18px | 500 | 0 | None |
| Value Label | Display | 10px | 400 | 1px | Uppercase |
| Feedback Header | Mono | 14px | 600 | 0 | None |
| Feedback Body | Mono | 13px | 400 | 0 | None |
| Button Text | Display | 14px | 700 | 1px | Uppercase |
| Dropdown | Display | 13px | 400 | 0 | None |
| Slider Value | Mono | 12px | 400 | 0 | None |

## Spacing System

### Base Unit: 8px
| Token | Value | Usage |
|:------|:------|:------|
| `--sp-xs` | 4px | Tight gaps (label-value pairs) |
| `--sp-sm` | 8px | Between related elements |
| `--sp-md` | 16px | Section padding, column gaps |
| `--sp-lg` | 24px | Major section margins |
| `--sp-xl` | 32px | Outer window padding (not used — tight layout) |

### Window Padding
- Top: 0 (header bar flush)
- Sides: 0 (columns flush to edges)
- Bottom: 0 (footer bar flush)
- Column internal padding: 16px

## Component Styles

### Meters (Vertical Bars)
```css
.meter {
  width: 30px;
  height: 220px;
  background: #1A1A1A;
  border-radius: 4px;
  border: 1px solid #2A2A2A;
  overflow: hidden;
}
.meter-fill {
  background: linear-gradient(to top, #00D4FF, #E91E8C);
  transition: height 60ms ease-out;
}
```

### Spectral Bars
```css
.spectral-bar {
  width: 24px;
  background: #1A1A1A;
  border-radius: 3px;
  position: relative;
}
.spectral-fill {
  /* Color shifts based on deviation from target */
  /* On target: #00E676 (green) */
  /* Close: #FFD600 (yellow) */
  /* Off: #FF1744 (red) */
  transition: height 80ms ease-out;
}
.spectral-target {
  position: absolute;
  width: 100%;
  height: 2px;
  border-top: 2px dashed #E91E8C;
}
```

### Send Button
```css
.send-button {
  background: #E91E8C;
  color: #FFFFFF;
  font-weight: 700;
  font-size: 14px;
  letter-spacing: 1px;
  text-transform: uppercase;
  border: none;
  border-radius: 6px;
  padding: 12px 32px;
  cursor: pointer;
  transition: filter 0.15s, box-shadow 0.15s;
}
.send-button:hover {
  filter: brightness(1.2);
  box-shadow: 0 0 20px rgba(233, 30, 140, 0.4);
}
.send-button.sending {
  animation: pulse-glow 1s infinite;
}
.send-button.sent {
  background: #00E676;
}
.send-button.error {
  background: #FF1744;
}
```

### Genre Dropdown
```css
.genre-select {
  background: #1A1A1A;
  color: #FFFFFF;
  border: 1px solid #2A2A2A;
  border-radius: 4px;
  padding: 8px 32px 8px 12px;
  font-size: 13px;
  appearance: none;
  cursor: pointer;
}
.genre-select:focus {
  border-color: #E91E8C;
  outline: none;
  box-shadow: 0 0 0 2px rgba(233, 30, 140, 0.3);
}
```

### Correlation Arc
- Semicircle from -1.0 to +1.0
- Background arc: #1A1A1A with #2A2A2A stroke
- Filled arc segments:
  - -1.0 to 0.0: #FF1744 (red zone)
  - 0.0 to 0.5: #FFD600 (yellow zone)
  - 0.5 to 1.0: #00E676 (green zone)
- Needle indicator: #00D4FF dot, 8px diameter

### Status LED
```css
.status-led {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}
.status-led.ready { background: #00E676; box-shadow: 0 0 6px #00E676; }
.status-led.sending { background: #FFD600; box-shadow: 0 0 6px #FFD600; }
.status-led.error { background: #FF1744; box-shadow: 0 0 6px #FF1744; }
```

## Animations

### Meter Smoothing
- CSS transition: `height 60ms ease-out` on meter fills
- JS update rate: 30Hz (33ms intervals)

### Button Pulse (Sending State)
```css
@keyframes pulse-glow {
  0%, 100% { box-shadow: 0 0 10px rgba(233, 30, 140, 0.3); }
  50% { box-shadow: 0 0 25px rgba(233, 30, 140, 0.6); }
}
```

### Sent Confirmation
- Flash green for 3 seconds, then transition back to pink over 0.3s

## Dark Theme Notes
- No light theme variant — this is a single-theme design
- All interactive elements have visible focus states (pink border/glow)
- Minimum contrast ratio: 4.5:1 for all text
- Meter colors chosen for readability against #1A1A1A backgrounds
