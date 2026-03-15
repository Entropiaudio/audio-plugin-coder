# SNIP Bridge - Style Guide v2 (Premium Overhaul)

## Design Philosophy
Premium audio plugin aesthetic inspired by iZotope and FabFilter. Every element has depth, glow, and physicality. No flat surfaces — everything is either embedded (meters) or raised (buttons, panels).

## Color Palette

### Core Colors
| Role | Hex | Usage |
|:-----|:----|:------|
| Background | `#050505` | Deep black — pure void |
| Surface | `#0E0E12` | Panel backgrounds (with gradient overlay) |
| Surface Raised | `#141418` | Elevated elements |
| Surface Light | `#1C1C22` | Hover states |
| Border | `rgba(255,255,255,0.06)` | Soft, barely-there borders |
| Border Soft | `rgba(255,255,255,0.03)` | Panel dividers |

### Accent Colors (with glow variants)
| Role | Hex | Glow | Usage |
|:-----|:----|:-----|:------|
| Pink | `#E91E8C` | `rgba(233,30,140,0.35)` | Brand accent, targets, button |
| Pink Strong | — | `rgba(233,30,140,0.55)` | Hover states, active glows |
| Cyan | `#00D4FF` | `rgba(0,212,255,0.35)` | Meter fills, data values |
| Cyan Strong | — | `rgba(0,212,255,0.55)` | LED readouts, active glows |

### Semantic Colors (with glow)
| Role | Hex | Glow | Usage |
|:-----|:----|:-----|:------|
| Success | `#00E676` | `rgba(0,230,118,0.4)` | Pass indicators |
| Warning | `#FFD600` | `rgba(255,214,0,0.35)` | Warning indicators |
| Error | `#FF1744` | `rgba(255,23,68,0.4)` | Fail indicators |

### Text Colors
| Role | Hex | Usage |
|:-----|:----|:------|
| Bright | `#FFFFFF` | Button text only |
| Primary | `#E8E8F0` | Headings, labels |
| Secondary | `#6B6B80` | Section labels, sublabels |
| Muted | `#3A3A4A` | Disabled, tick marks |

## Depth System

### Panel Depth (Glassmorphism)
Every column panel uses layered gradients for depth:
```css
background:
  linear-gradient(175deg,
    rgba(255,255,255,0.025) 0%,   /* Top highlight */
    transparent 40%,
    rgba(0,0,0,0.1) 100%);       /* Bottom shadow */
background-color: var(--surface);
```
Plus a 1px top highlight pseudo-element:
```css
::before {
  background: linear-gradient(90deg, transparent, rgba(255,255,255,0.06), transparent);
}
```

### Meter Embedding
Meters look physically recessed into the panel:
```css
box-shadow:
  inset 0 2px 6px rgba(0,0,0,0.8),   /* Deep inset top */
  inset 0 0 12px rgba(0,0,0,0.4),     /* Inner ambient shadow */
  0 1px 0 rgba(255,255,255,0.03);     /* Bottom edge highlight */
```

### Neon Fill Glow
Meter fills emit light outward:
```css
box-shadow:
  0 0 8px var(--cyan-glow-strong),     /* Near glow */
  0 0 20px var(--cyan-glow),           /* Far glow */
  inset 0 0 6px rgba(255,255,255,0.15); /* Internal brightness */
```
Plus a bright white cap line at the fill top via `::after`.

### Button 3D Effect
Send button has a physical raised appearance:
```css
background:
  linear-gradient(180deg,
    rgba(255,255,255,0.12) 0%,   /* Top shine */
    transparent 40%,
    rgba(0,0,0,0.15) 100%),      /* Bottom shadow */
  linear-gradient(180deg, #F02A96, #C4167A);  /* Base gradient */
border-top: 1px solid rgba(255,255,255,0.2);   /* Top edge light */
border-bottom: 1px solid rgba(0,0,0,0.3);      /* Bottom edge shadow */
box-shadow:
  0 2px 8px rgba(233,30,140,0.3),  /* Near shadow */
  0 4px 16px rgba(233,30,140,0.15), /* Far shadow */
  inset 0 1px 0 rgba(255,255,255,0.1); /* Inner top shine */
```

## Typography

### LED Digital Readouts
Numeric values glow like real LED displays:
```css
font-family: var(--mono);
font-size: 20px;
font-weight: 600;
color: var(--cyan);
text-shadow:
  0 0 6px var(--cyan-glow-strong),   /* Tight glow */
  0 0 16px var(--cyan-glow),          /* Medium spread */
  0 0 30px rgba(0,212,255,0.15);     /* Ambient halo */
```

### Feedback Status Indicators
Pass/warn/fail text glows in its semantic color:
```css
.fb-pass { text-shadow: 0 0 8px var(--success-glow); font-weight: 600; }
.fb-warn { text-shadow: 0 0 8px var(--warning-glow); font-weight: 600; }
.fb-fail { text-shadow: 0 0 8px var(--error-glow); font-weight: 600; }
```

## Stereo Arc (SVG)
- **Background track:** 2px stroke, `rgba(255,255,255,0.06)` — barely visible
- **Colored arc:** 3px stroke, smooth gradient from red→yellow→green→cyan (via `linearGradient`)
- **Tick marks:** 1px subtle markers at -1.0, 0, +1.0
- **Needle:** 4px cyan circle with triple-pass gaussian blur glow filter
- **Needle core:** 1.5px white circle at 90% opacity — intense bright center

## Animations
- Meter fills: `height 80ms ease-out`
- Button hover: `translateY(-1px)` lift effect
- Button active: `translateY(1px)` press effect
- Pulse glow: 1.2s ease-in-out infinite (sending state)
- Color transitions: 200ms ease

## Key Differences from v1
1. Background deepened from `#0D0D0D` to `#050505`
2. All meters have inset shadow embedding
3. Meter fills have neon glow + white cap lines
4. Spectral bars glow in their deviation color
5. Stereo arc redesigned: thin elegant gradient, intense glowing needle
6. All numeric readouts have LED text-shadow glow
7. Send button has 3D raised gradient with hover/active press states
8. Panels have glassmorphic gradient overlays + top highlight pseudo-elements
9. Borders softened to near-invisible `rgba(255,255,255,0.06)`
10. Body has subtle radial vignette effect
