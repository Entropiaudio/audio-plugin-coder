# WebView Plugin Template (Entropia Audio)

A complete, clone-and-rename JUCE 8 WebView plugin scaffold — the reusable
foundation distilled from **Entropan**. Ships a working demo (Gain → Lowpass +
mode + bypass) so you can build, load, and hear it immediately, then swap the
DSP and parameters for your own.

## What you get

- **C++ processor** — APVTS layout with a `logRange()` helper that stays
  byte-compatible with the WebView param layer, snapshot **undo/redo**
  (window size excluded so resizing never pollutes history; host session load
  resets it), crossfaded bypass, clean `processBlock`.
- **C++ editor** — relays/attachments held in **vectors built from ID lists**
  (add a param = add one string, no per-param members), correct
  relay→webview→attachment destruction order, embedded resource provider
  (query/fragment-safe), native functions (`commitUndo`/`undo`/`redo`/
  `setEditorSize`), aspect-locked resize, per-instance WebView2 data folder.
- **WebView UI** (`Source/ui/public/index.html`) — one self-contained file:
  - **Parameter layer** with embedded-vs-browser detection and a
    **standalone fallback**, so the same file renders at `http://localhost`
    for design work. Skewed params use the JUCE lib's own skew math — the UI
    and host agree on every knob position.
  - The **Entropia knob** (Chaosverb-matched: 72px canvas, copper→accent
    gradient arc, tight 3-pass glow, full 38px disc), plus mode chips and a
    toggle.
  - **Undo/redo** (header buttons + ⌘Z / ⌘⇧Z), **settings popover**
    (tooltips, window-size presets, brightness), **corner resize grip** (so AU
    hosts that don't give window-edge resize still work), custom floating
    tooltips (WKWebView ignores native `title`), and crisp CSS-`zoom` scaling.
  - Design tokens (`:root`) — reskin the whole UI from one place.
- **CMakeLists** — cross-platform (Win VST3 / mac VST3+AU / Linux VST3+LV2),
  web assets embedded via `juce_add_binary_data`.
- **Fonts + JUCE interop lib** already in place.

## Use it

```sh
./rename.sh TapeWidth              # → ../../plugins/TapeWidth (auto 4-char code)
./rename.sh TapeWidth Tpwd ~/x     # explicit code + destination
```

Then build from the **repo root** (the root `CMakeLists.txt` auto-discovers
`plugins/*/CMakeLists.txt`):

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target TapeWidth_VST3   # or _AU
```

Artefacts land in `build/plugins/TapeWidth/TapeWidth_artefacts/`.
Preview the UI in a browser while iterating:
`python3 -m http.server 8080 --directory plugins/TapeWidth/Source/ui/public`.

## Adding a parameter

1. **Processor** — add it in `createParameterLayout()` (use `logRange(lo,hi)`
   for anything frequency-like so the UI skew matches).
2. **Editor** — add its id to the matching list in `sliderParamIds()` /
   `comboParamIds()` / `toggleParamIds()`. Relay + attachment are created
   automatically.
3. **UI** — add it to the `P = { … }` map (`sliderParam`/`comboParam`/
   `toggleParam` with the id), then bind a `makeKnob(...)` / chip / toggle.

## Gotchas baked in (keep them)

- **Skew parity** — never build param ranges from custom lambdas; they
  serialize `skew=1` to the JS lib and desync the UI. Use `setSkewForCentre`.
- **Member order** — relays declared before the WebView, attachments after
  (they destroy in reverse). Reordering crashes the DAW on unload.
- **WKWebView** — no `ctx.filter` (canvas CSS blur) and no native tooltips;
  use `shadowBlur` and the custom `.tip`. Scale with CSS `zoom`, not
  `transform`, so canvases stay DPI-sharp.
- **AU resize** — Logic/Ableton give AU no window-edge resize; the corner grip
  drives `setEditorSize`.
- **Windows multi-instance** — each editor gets a unique WebView2 user-data
  folder.
