# MultiBlend - Implementation Plan

## Complexity Score: 5/5 (Research-Level)
## Framework: WebView
## Strategy: Phased Implementation (6 phases)

---

## Phase 4.1.1: Plugin Hosting Infrastructure

**Goal:** Establish the core ability to load, process, and unload VST3 plugins within MultiBlend.

- [ ] **PluginHostManager class**
  - Register `juce::VST3PluginFormat` with `AudioPluginFormatManager`
  - `KnownPluginList` integration — scan or import from host DAW
  - Async plugin creation via `createPluginInstanceAsync()`
  - Plugin description lookup and validation

- [ ] **PluginSlot class**
  - `std::atomic<AudioPluginInstance*>` for lock-free audio thread access
  - `loadPlugin(PluginDescription)` — async load on message thread, atomic swap
  - `unloadPlugin()` — remove from audio path, destroy on message thread
  - `processBlock()` — delegates to hosted plugin if loaded, pass-through if empty
  - Bypass flag (`std::atomic<bool>`)
  - Dry/wet mix (`juce::SmoothedValue<float>`)
  - State save/restore via `getStateInformation()` / `setStateInformation()`

- [ ] **PluginEditorWindow class**
  - Standalone window wrapping `pluginInstance->createEditor()`
  - Open/close management per slot
  - Auto-close when plugin unloaded

- [ ] **Basic MultiBlendProcessor skeleton**
  - 73 `juce::AudioParameterFloat/Bool/Choice` registered
  - `prepareToPlay()` / `releaseResources()` / `processBlock()` stubs
  - Global input/output gain with `SmoothedValue`
  - State serialization (own params + all 15 hosted plugin states)

**Deliverable:** Can load a VST3 plugin into a slot, process audio through it, show its GUI, save/restore state.

---

## Phase 4.1.2: Crossover & Band Splitting

**Goal:** Split audio into 3 frequency bands with configurable crossovers.

- [ ] **CrossoverProcessor class**
  - 4th-order Linkwitz-Riley (24 dB/oct) as default using `juce::dsp::LinkwitzRileyFilter`
  - Two crossover points: `crossover_low_mid` and `crossover_mid_high`
  - Allpass compensation for bands not split at a given point
  - Slope switching (12/24/48 dB/oct): 12 = 2nd-order LR, 24 = 4th-order LR, 48 = cascade two 4th-order LR
  - Output: 3 separate `AudioBuffer<float>` (low, mid, high)

- [ ] **Dynamic crossover ranges**
  - When `band_X_bypass` is true, reconfigure crossover frequencies
  - Low bypassed: remove low-mid crossover, mid receives full low+mid spectrum
  - High bypassed: remove mid-high crossover, mid receives full mid+high spectrum
  - Mid bypassed: single crossover point between low and high
  - Smooth frequency transitions to avoid clicks

- [ ] **Band summing**
  - Sum 3 band buffers back to stereo output
  - Respect solo/mute flags (solo = mute all others)
  - Bypassed bands pass their signal through unprocessed

**Deliverable:** Audio correctly splits into 3 bands and sums flat. Dynamic crossover ranges work. Solo/mute/bypass functional.

---

## Phase 4.1.3: Band Processing & Plugin Chains

**Goal:** Wire 5 serial PluginSlots per band with full gain staging and dry/wet.

- [ ] **BandProcessor class**
  - 5 PluginSlot instances in series
  - Band input/output gain (`SmoothedValue`)
  - Band dry/wet (`SmoothedValue`) — captures pre-gain dry signal, blends after chain
  - Band bypass — skips entire processing chain
  - Propagates `prepareToPlay` / sample rate / buffer size to all slots

- [ ] **Serial slot processing**
  - Output of slot N feeds input of slot N+1
  - Empty/bypassed slots pass through with zero overhead
  - Per-slot dry/wet blending at each step

- [ ] **Integration with CrossoverProcessor**
  - CrossoverProcessor outputs 3 band buffers
  - Each BandProcessor processes its buffer
  - Results summed in main processBlock

**Deliverable:** Full signal chain working: input → crossover → 3 bands × 5 slots each → sum → output. Dry/wet at all 3 levels.

---

## Phase 4.1.4: M/S Routing

**Goal:** Implement L/R/M/S/Stereo routing at band and slot levels.

- [ ] **MSProcessor utility class**
  - `encodeMidSide(AudioBuffer&)` — in-place L/R → M/S
  - `decodeMidSide(AudioBuffer&)` — in-place M/S → L/R
  - `extractLeft(AudioBuffer&, AudioBuffer& out)` — copy left, zero right
  - `extractRight(AudioBuffer&, AudioBuffer& out)` — copy right, zero left
  - `recombine(AudioBuffer& processed, AudioBuffer& original, RoutingMode)` — merge processed component back

- [ ] **Band-level routing**
  - Before band chain: encode to selected routing domain
  - After band chain: decode back to stereo
  - Unprocessed component preserved and recombined

- [ ] **Slot-level routing**
  - If slot routing differs from band routing, apply additional encode/decode around the slot
  - Nested routing: band=Mid, slot=Left means "left channel of the mid component"

**Deliverable:** All 5 routing modes work at both band and slot levels. Nested routing correct.

---

## Phase 4.1.5: Linear Phase Crossover

**Goal:** Add optional FFT-based linear phase crossover mode.

- [ ] **LinearPhaseCrossover class**
  - Windowed-sinc FIR filter design at crossover frequencies
  - FFT-based convolution (overlap-add or overlap-save)
  - FFT size: 4096 default (configurable)
  - Produces same 3-band split as minimum-phase crossover

- [ ] **Mode switching**
  - `linear_phase` parameter toggles between LR and FIR crossover
  - Crossfade between modes over ~50ms to avoid clicks
  - `setLatencySamples()` updated dynamically (0 for minimum phase, FFT_SIZE/2 for linear phase)

- [ ] **Latency compensation**
  - Report latency to host for PDC (plugin delay compensation)
  - Hosted plugins' internal latency also tracked and reported

**Deliverable:** Linear phase mode produces phase-aligned crossover. Switching between modes is click-free. Host compensates latency.

---

## Phase 4.1.6: WebView UI

**Goal:** Complete FabFilter-inspired web UI for all controls.

- [ ] **Spectrum display**
  - Real-time FFT visualization of input signal
  - Color-coded band regions (Low, Mid, High with distinct pastels)
  - Draggable crossover handles on the spectrum
  - Slope visualization (steepness shown graphically)

- [ ] **Band strips (x3)**
  - Vertical strip per band with: input/output gain knobs, dry/wet knob, solo/mute/bypass buttons, routing selector
  - 5 plugin slot cards per band

- [ ] **Plugin slot cards (x15)**
  - Slot number, plugin name (or "Empty"), bypass toggle, dry/wet knob, routing selector
  - Click empty slot → plugin browser (list from KnownPluginList)
  - Click loaded slot → open hosted plugin GUI window
  - Drag-to-reorder within a band (stretch goal)

- [ ] **Global controls**
  - Global input/output gain, global dry/wet
  - Linear phase toggle with latency indicator
  - Crossover slope selector

- [ ] **WebView ↔ C++ bridge**
  - `window.__JUCE__` for parameter changes
  - Spectrum data sent from C++ to WebView via JSON events (~30 Hz)
  - Plugin list sent on request for plugin browser
  - Hosted plugin state changes (load/unload) communicated via events

**Deliverable:** Complete, functional UI. All parameters controllable. Spectrum and crossover visualization working.

---

## Dependencies

### Required JUCE Modules
- `juce_audio_basics` — audio buffer, MIDI
- `juce_audio_processors` — AudioProcessor, AudioPluginFormatManager, VST3PluginFormat, KnownPluginList
- `juce_dsp` — LinkwitzRileyFilter, ProcessSpec, SmoothedValue
- `juce_gui_extra` — WebBrowserComponent
- `juce_gui_basics` — Component, DocumentWindow (for hosted editor windows)
- `juce_core` — JSON, threading, var
- `juce_data_structures` — ValueTree (state persistence)

### External Dependencies
- **VST3 SDK** — bundled with JUCE, required for hosting VST3 plugins
- **WebView2** (Windows) / **WKWebView** (macOS) — platform WebView backends

---

## Risk Assessment

### High Risk
- **VST3 hosting thread safety:** Loading/unloading plugins while audio processes. **Mitigation:** Atomic pointer swap pattern; prepare new instance fully before swapping in; destroy old instance on message thread only.
- **Hosted plugin latency:** Each hosted plugin may report its own latency. The total chain latency per band may differ. **Mitigation:** Track per-slot latency, compute max band latency, compensate shorter bands with delay buffers if needed (or accept per-band latency differences).
- **State persistence:** Saving/restoring 15 plugin states plus own params. **Mitigation:** Serialize each hosted plugin state as a base64-encoded blob inside MultiBlend's own XML/binary state.

### Medium Risk
- **Linear phase crossover latency:** FFT-based crossover adds significant latency. **Mitigation:** Clearly report to host; make it optional; default off.
- **Dynamic crossover reconfiguration:** Changing filter topology when bands are bypassed. **Mitigation:** Always keep all filters instantiated; bypass bands by summing their output into neighbors rather than removing filters.
- **CPU load:** 15 hosted plugins + crossover + M/S processing. **Mitigation:** Empty/bypassed slots are zero-cost; encourage users to bypass unused slots.

### Low Risk
- **Parameter count (73):** Large but manageable with JUCE's parameter system.
- **WebView performance:** Spectrum display at 30 Hz is lightweight.
- **M/S processing:** Simple, well-understood math.
