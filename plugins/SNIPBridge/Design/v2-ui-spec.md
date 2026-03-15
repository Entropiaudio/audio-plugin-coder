# SNIP Bridge - UI Specification v2 (Premium Overhaul)

## Summary of Changes from v1
CSS-only overhaul targeting premium $150 VST plugin aesthetic. No layout or structural changes — same 1000x500 wide 4-column layout.

## Visual Upgrades

### 1. Depth & Glassmorphism
- Column panels: subtle top-to-bottom gradient overlay + 1px top highlight line
- Background: pure deep black `#050505` with radial vignette
- Borders softened to near-invisible `rgba(255,255,255,0.06)`
- Panel corners remain flush (no rounding between columns — they tile seamlessly)

### 2. Advanced Meters
- Meter wells: deep `inset box-shadow` creates physically embedded look
- Meter fills: neon glow via multi-layer `box-shadow` (cyan → pink gradient)
- White cap line at fill top via `::after` pseudo-element
- Spectral bars: individual glow per bar matching deviation color (green/yellow/red)

### 3. Stereo Arc
- Thin 3px arc with smooth `linearGradient` (red→yellow→green→cyan)
- Barely-visible 2px background track
- Needle: 4px cyan circle with triple-pass gaussian blur = intense glow
- Inner needle core: 1.5px white dot for bright center
- Subtle tick marks at -1.0, 0, +1.0

### 4. LED Typography
- All numeric readouts (LUFS, RMS, Corr, Width): triple-layer `text-shadow`
- Feedback status icons: colored `text-shadow` glow (green/yellow/red)
- Logo "BRIDGE" text has pink glow
- React slider value has cyan glow

### 5. Overall Polish
- Send button: 3D layered gradient with top shine, bottom shadow, hover lift, press sink
- Genre dropdown: inset shadow, subtle hover border transition
- Slider thumb: radial gradient with pink glow halo
- Status LED: double-layer glow (tight + ambient)
- Header/footer: subtle directional gradient backgrounds
- Match bar: `inset box-shadow` for embedded track + `currentColor` glow on fill

## Files
- **v2-test.html** — Working interactive preview (open in Chrome/Edge)
- **v2-style-guide.md** — Complete CSS specification with code examples
- **v2-ui-spec.md** — This file

## Layout (unchanged from v1)
- Window: 1000 x 500 px
- Header: 48px
- Footer: 56px
- Column 1 (Dynamics): 200px
- Column 2 (Tonality): 220px
- Column 3 (Stereo): 180px
- Column 4 (Feedback): flex remaining
