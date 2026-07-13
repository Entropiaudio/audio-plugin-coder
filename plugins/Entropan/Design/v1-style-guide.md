# Entropan — Style Guide v1

Source of truth: **Chaosverb v1.0.18 design tokens** (Entropia Audio website palette). Copied verbatim; additions marked ★.

## Color palette

### Surfaces
| Token | Hex | Use |
| :--- | :--- | :--- |
| `--bg-shell` | `#14110F` | window background |
| `--bg-card` | `#1C1916` | section cards |
| `--bg-elevated` | `#24201D` | elevated surfaces, popovers |

### Borders
| Token | Hex |
| :--- | :--- |
| `--border-subtle` | `#2e2a25` |
| `--border-bright` | `#3e3a35` |
| `--border-divider` | `#26221F` |
| `--border-soft` | `rgba(255,245,230,0.08)` |

### Text
| Token | Hex |
| :--- | :--- |
| `--text-primary` | `#E8E2D6` (warm cream) |
| `--text-secondary` | `#A8A29A` |
| `--text-dim` | `#6A6258` |
| `--text-faint` | `#403A32` |

### Brand accent (Entropia copper)
| Token | Value |
| :--- | :--- |
| `--accent` | `#D4A574` |
| `--accent-bright` | `#E2BD96` |
| `--accent-glow` | `rgba(212,165,116,0.35)` |
| `--accent-tint` | `rgba(212,165,116,0.08)` |
| `--accent-muted` | `rgba(212,165,116,0.15)` |

### ★ Band accents (6 hues, mapped from Chaosverb section palette)
| Band | Token | Hex | Origin |
| :--- | :--- | :--- | :--- |
| B1 | `--band-1` | `#3ab8d4` | section-space (cyan) |
| B2 | `--band-2` | `#8aaa7a` | section-spectral (sage) |
| B3 | `--band-3` | `#5fc88a` | section-tone (green) |
| B4 | `--band-4` | `#b06ed4` | section-motion (purple) |
| B5 | `--band-5` | `#d6884a` | section-duck (orange) |
| B6 | `--band-6` | `#D4A574` | accent (copper) |

Output section: `--section-output: #c8c8d0`.

### States
`--state-success #6bff8e` · `--state-warning #ffb86b` · `--state-error #ff6b6b`

## Typography

| Font | Weight/Style | Use |
| :--- | :--- | :--- |
| **Fraunces** (variable) | 300, optical | `.plugin-title` only |
| **Montserrat** (variable) | 300–500 | editorial accents |
| **JetBrains Mono** (variable) | 400–600 | everything else: labels, values, chips, buttons |

- Body base: JetBrains Mono 11px.
- Section labels: uppercase, letter-spacing ~2px, dimmed section hue.
- Values: 10px mono `#666666` (locked: `#444`).
- Fonts shipped as woff2 in `ui/public/fonts/` (copy from Chaosverb: `Fraunces-Variable.woff2`, `Montserrat-Variable.woff2`, `JetBrainsMono-Variable.woff2`).

## Spacing / layout

- Shell padding 12px; section gap 10px; card radius 8px; card border `--border-subtle`.
- Header height 56px; bottom strip 180px; spectrum takes remaining flex.
- Knob: 56×56 canvas + face; header ENTROPY knob 44×44.
- H-slider row: track 8px tall, radius 4, fill = section/band hue gradient, mono value right-aligned, SYNC toggle 44px.

## Component styles (clone from Chaosverb CSS)

- **Arc knob:** canvas arc -135°→+135°, track `#2a2622`, value arc = accent w/ 3-pass glow (blur widths ~6/3/1.5), disc face `#1f1c19` border `--border-bright`, white 4px indicator dot rotated `-135+norm*270`°, `translateY(-12px)`.
- **Buttons:** `mutate-btn` (CHAOS): outlined pill, copper border/glow on hover; `bypass-btn`, `flutter-toggle` (ON/OFF states), `mode-toggle` (SYNC), `settings-btn` gear.
- **Preset dropdown:** `preset-dropdown-btn` + menu, mono, `--bg-elevated`.
- **Overlays:** `mutation-flash` (CHAOS click flash), `bypass-dim` (dim shell when bypassed), tooltips = custom floating div (native `title` broken in WKWebView — Chaosverb lesson).

## ★ Spectrum canvas styling

- Background: transparent over shell; grid lines `--border-divider`, octave marks + dB rows; labels `--text-faint` 9px mono.
- Analyzer fill: vertical gradient `rgba(232,226,214,0.10)`→transparent, 1px line `rgba(232,226,214,0.25)`.
- Bell: fill band-hue @18%, stroke band-hue @85% 1.5px, glow on selected (shadow blur 8 band-hue); apex handle 8px dot, white core.
- Pan trace: 2px band-hue line sliding L/R within bell width at bell mid-height.
- Step editor: bars fill band-hue @35%, active step bright, center line `--border-bright`.

## Motion

`--t-fast 120ms` · `--t-base 200ms` · `--t-slow 350ms` · `--ease-editorial cubic-bezier(0.22,1,0.36,1)`. Reduced-motion media query honored (Chaosverb block reused).
