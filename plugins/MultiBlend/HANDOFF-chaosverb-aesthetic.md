# MultiBlend — Chaosverb Aesthetic + Chaos System Handoff

**Date:** 2026-05-17
**Plugin path:** `/Users/nbs/Developer/audio-plugin-coder/plugins/MultiBlend/`
**Build status:** ✅ VST3 builds clean, ad-hoc signed
**Build artifact:** `/Users/nbs/Developer/audio-plugin-coder/build/plugins/MultiBlend/MultiBlend_artefacts/Release/VST3/MultiBlend.vst3`

---

## Context

MultiBlend is a multi-band processor (3 bands × 5 slots × per-band controls). I extended its WebView UI with the **Chaosverb Glow Dark** aesthetic system (saved as a reusable template in the plugin-freedom-system repo) and added a **Make Chaos** mutation system inspired by Chaosverb's mutation timer.

Important: **all changes are in `Source/ui/public/index.html` only**. No C++ changes were made. No new parameters. No APVTS modifications. The chaos system runs entirely in JavaScript and writes to existing JUCE parameters via the WebView relay (`getSliderState(id).setNormalisedValue(v)`).

---

## What's already done

### 1. CVKnob component installed
Files copied + wired:
- `Source/ui/public/cv-knob.css`
- `Source/ui/public/cv-knob.js`
- `CMakeLists.txt` → both files added to `juce_add_binary_data` SOURCES
- `Source/PluginEditor.cpp` → `/cv-knob.css` and `/cv-knob.js` URL handlers added to `getResource()`
- `Source/ui/public/index.html` → `<link>` to head + `<script>` after `<body>`

The CVKnob component is **available but not currently used** — MultiBlend has its own custom inline `drawKnob()` renderer that already implements the same Chaosverb 3-pass glow algorithm. CVKnob is wired for future use if you want to refactor to the standalone component.

Backup files preserved: `CMakeLists.txt.bak`, `Source/PluginEditor.cpp.bak`, `Source/ui/public/index.html.bak`.

### 2. Aesthetic polish (design token additions)
Added to `:root` block in `index.html`:

```css
/* Full section accent palette — vocab for future expansion beyond 3 bands */
--section-space:    #3ab8d4;   /* cyan       */
--section-spectral: #8aaa7a;   /* sage olive */
--section-tone:     #5fc88a;   /* mint green */
--section-motion:   #b06ed4;   /* violet     */
--section-duck:     #d6884a;   /* warm amber */
--section-output:   #c8c8d0;   /* pearl      */

/* Universal cyan for save/create actions (distinct from brand purple) */
--action-create:      #00d4ff;
--action-create-glow: rgba(0, 212, 255, 0.2);
```

Plus two new utility classes ready to use anywhere in markup:

- **`.btn-pill-glow`** — transparent pill button, cyan-on-hover, white-flash on `.firing` state. Drop-in for any save/create/trigger button.
- **`.rainbow-frame`** — wrap an `<input>` to get animated rainbow border (cycles through all section accent colors over 12s). Ready for save-preset dialog.

### 3. Make Chaos mutation system

**Header cluster** inserted into `.header-center` above the existing Linear Phase pill:

```
[ ⚡ MAKE CHAOS ]   00:30   [▶]   │ EVERY ▬▬◉▬▬ 30s
```

| Control | Behavior |
|---------|----------|
| `⚡ Make Chaos` button | Click → randomizes all unlocked chaos params now. Resets countdown if timer running. Flashes white on fire. |
| Countdown `00:30` | 22px JetBrains Mono cyan. Dim opacity 0.55 idle, full 1.0 running, pulses white→cyan on each fire. |
| `▶`/`■` toggle | Start/stop auto-mutation timer. Cyan glow when running. |
| Interval slider | Log-scale 5s → 600s (10min). Live label format: `5s` / `30s` / `1m30s` / `5m` / `10m`. Restarts countdown if changed mid-run. |

**What randomizes** (5 params × 3 bands = 15 params):
- `band_low_dry_wet`, `band_low_input_gain`, `band_low_output_gain`, `band_low_slope`, `band_low_ms_morph`
- `band_mid_dry_wet`, `band_mid_input_gain`, `band_mid_output_gain`, `band_mid_slope`, `band_mid_ms_morph`
- `band_high_dry_wet`, `band_high_input_gain`, `band_high_output_gain`, `band_high_slope`, `band_high_ms_morph`

**What stays fixed** (sane defaults — chaos shouldn't kill the audio):
- Crossover freqs (would change band identity)
- Solo / mute / bypass (would silence things)
- Slot routing (would scramble effect chains)
- Linear phase (system-level mode)
- Global In / Out / Dry-Wet (master controls)

**Lock system** (right-click any knob to exclude from chaos):
- Locked state: cyan inset ring on face + cyan indicator dot + `🔒` corner badge
- Per-session only (NOT persisted to APVTS — see "next steps" below)
- Uses delegated `contextmenu` listener — works on dynamically-generated slot knobs too
- Browser context menu suppressed via `e.preventDefault()`

**Implementation location:** All chaos logic lives in a single IIFE `installChaos()` near the bottom of `index.html`'s `<script>` block (just before the closing `</script>` tag, right after the spectrum animation loop).

**Debug hooks** exposed for testing / future settings panel:
```js
window.__chaos.fire()             // trigger mutation now
window.__chaos.start()            // start auto-timer
window.__chaos.stop()             // stop auto-timer
window.__chaos.setInterval(45)    // programmatically set interval in seconds
window.__chaos.lockedIds          // Set<string> of currently-locked param IDs
```

---

## Architecture notes

### Param writes
Chaos `fire()` uses MultiBlend's existing cached slider helper:
```js
const st = getSlider(id);           // cached lookup of WebSliderRelay state
st.setNormalisedValue(Math.random());
```
JUCE's `WebSliderParameterAttachment` listener writes the value to the AudioParameterFloat. The audio thread sees the new value on its next `processBlock` via `getRawParameterValue("id")->load()`.

### UI sync
After writing the param, `fire()` immediately updates the UI knob visually using the existing local triplet:
```js
knob.dataset.value = v;
updateKnobDisplay(knob);
updateIndicator(knob);
drawKnob(canvas, v, getKnobColor(knob));
```
This avoids the ~1-frame round-trip lag that would happen if we waited for the JUCE→JS listener to fire.

### Timing
- Mutation fires via `setInterval(fire, intervalMs)`
- Countdown ticker runs at 250ms cadence (smooth enough for 1Hz display)
- All cleaned up on `stop()` — no zombie timers

### Drawing knobs
The custom `drawKnob()` function (line ~1016 in `index.html`) implements the Chaosverb 3-pass glow algorithm:
- Pass 1: shadowBlur 8× lineW, globalAlpha 0.35 (outer halo)
- Pass 2: shadowBlur 4× lineW, globalAlpha 0.6 (mid glow)
- Pass 3: shadowBlur 1.5× lineW, globalAlpha 1.0 (crisp inner arc)
- Segment-by-segment cyan→accent gradient lerp via `lerpHex(KNOB_CYAN, accentColor, t)`

---

## Suggested next steps (in priority order)

### 1. Persist lock state across DAW sessions
Currently locks are JS-only and lost on plugin reload. To persist:

**Option A — APVTS booleans (proper):**
Add 15 `AudioParameterBool` lock params to `PluginProcessor::createParameterLayout()` matching the chaos param IDs (e.g. `chaos_lock_band_low_dry_wet`). Bind each via `WebToggleButtonRelay` + `WebToggleButtonParameterAttachment`. Wire `getToggleState(id)` calls in the chaos JS for read/write.

**Option B — JSON in state XML (simpler):**
Add a `juce::ValueTree` child node to APVTS state via `getStateInformation()` / `setStateInformation()` containing a JSON array of locked IDs. Expose via a `getNativeFunction("setChaosLocks", ...)` / `getNativeFunction("getChaosLocks", ...)` bridge.

Option A integrates with DAW automation (lock state is automatable). Option B is simpler and probably sufficient — locks aren't usually automated.

### 2. Visual countdown polish
Chaosverb has a few states the countdown can show that are currently missing here:
- `WAIT` when DAW transport is stopped (only relevant if you add tempo-sync mode)
- Last-second pulse acceleration
- `--:--` styling differentiation between "stopped" and "idle waiting"

### 3. Tempo-sync mode for interval
Chaosverb has a SYNC/MS toggle on its interval — sync mode snaps to note divisions (1/16, 1/8, ..., 8 bars) using host BPM via `juce::AudioPlayHead::getPosition()->getBpm()`. Would need:
- Toggle button in chaos cluster (`SYNC` / `MS`)
- BPM query bridge from C++ via `getNativeFunction("getHostBpm", ...)`
- Note division lookup table in JS
- Interval slider becomes either ms or division-index based on mode

### 4. Crossfade between mutations
Chaosverb dual-FDN crossfades between current and new state over 0-500ms to avoid jarring jumps. MultiBlend doesn't have parallel processors per band, so the simplest version would be parameter smoothing — wrap `setNormalisedValue` with a `requestAnimationFrame`-driven lerp over N ms. Already exists in JUCE as `LinearSmoothedValue` on the audio side, but UI-driven smoothing would feel different from automation-driven.

### 5. Optional: Make Chaos as a saveable behavior
Add an option to save the current chaos config (interval, locks, randomization param set) as part of preset state. Lets users have "subtle chaos" vs "destruction chaos" presets.

### 6. Use the CVKnob component
Currently MultiBlend has its own custom knob renderer (~150 lines) that duplicates the CVKnob algorithm. Could refactor to use the bundled component for cleaner code + automatic propagation of any future CVKnob improvements. Tradeoff: refactor risk vs. code dedup.

---

## File diff summary

```
plugins/MultiBlend/CMakeLists.txt
  +2 lines  (cv-knob.css, cv-knob.js in juce_add_binary_data SOURCES)

plugins/MultiBlend/Source/PluginEditor.cpp
  +18 lines (URL handlers for /cv-knob.css and /cv-knob.js in getResource)

plugins/MultiBlend/Source/ui/public/cv-knob.css      [NEW, ~120 lines]
plugins/MultiBlend/Source/ui/public/cv-knob.js       [NEW, ~320 lines]

plugins/MultiBlend/Source/ui/public/index.html
  ~210 new lines total:
    - +9   :root design tokens (section accents + action-create)
    - +60  CSS for .btn-pill-glow, .rainbow-frame, @keyframes rainbowFlow
    - +85  CSS for .chaos-cluster, .chaos-countdown, .chaos-timer-toggle,
                  .chaos-interval, .chaos-interval-slider, .chaos-interval-label,
                  .chaos-interval-caption, .knob.chaos-locked
    - +8   HTML for chaos cluster in header-center
    - +135 JS installChaos() IIFE at end of script
    - 2 backups created: index.html.bak, .pre-aesthetic.bak
```

---

## How to verify it's working

1. Build: `cd /Users/nbs/Developer/audio-plugin-coder && cmake --build build --target MultiBlend_VST3`
2. Load MultiBlend in any VST3 host (Reaper, Ableton, Logic via AU rescan, etc.)
3. Check header — should show `⚡ Make Chaos | 00:30 | ▶ | EVERY ___◉___ 30s` cluster
4. Click `⚡ Make Chaos` — all 15 band knobs (dry_wet, in_gain, out_gain, slope, ms_morph for each of low/mid/high) should jump to random values, button flashes white
5. Drag interval slider down → label shows `5s` → countdown ticks faster
6. Click `▶` → countdown counts down → mutation auto-fires at zero → countdown resets
7. Right-click any band knob → 🔒 badge appears + cyan ring → fires now skip that param
8. Right-click locked knob again → unlock

If `▶` doesn't appear or chaos doesn't fire, check browser console (Reaper has built-in WebView devtools via Ctrl+Shift+I in some hosts) for JS errors. Most likely cause is a typo in a param ID or `getSlider()` returning null.

---

## Related: Chaosverb Glow Dark template

The aesthetic system this is based on is saved as a reusable template at:
```
~/Developer/plugin-freedom-system/.claude/aesthetics/chaosverb-glow-dark-001/
├── aesthetic.md       (427 lines — full prose spec)
├── metadata.json      (tags, design tokens, dependencies)
└── preview.html       (live Chaosverb UI reference)
```

Apply to other plugins via the `ui-template-library` skill in plugin-freedom-system.

The CVKnob component is at:
```
~/Downloads/chaosverb-knob-component/
├── cv-knob.css
├── cv-knob.js
├── example.html
├── README.md
└── install-knob-component.sh    (one-command installer)
```

To install into any other plugin folder:
```bash
~/Downloads/chaosverb-knob-component/install-knob-component.sh /path/to/target-plugin
```

---

## Git status

These changes are NOT committed to the audio-plugin-coder repo. Run `git diff` from `/Users/nbs/Developer/audio-plugin-coder/` to review, then commit when ready.
