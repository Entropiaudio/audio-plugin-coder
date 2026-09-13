# MultiBlend - DSP Architecture Specification

## Overview
MultiBlend is a multiband VST3 host plugin. It splits audio into 3 frequency bands via Linkwitz-Riley crossovers, routes each band through a chain of up to 5 hosted VST3 plugin instances, and sums the results. Every level (slot, band, global) has independent dry/wet, gain staging, and L/R/M/S routing.

## Core Components

### 1. Global I/O Stage
- **Input Gain:** Applies `global_input_gain` to incoming stereo signal
- **Output Gain:** Applies `global_output_gain` after band summing
- **Global Dry/Wet:** Blends final processed output against the original dry input (captured pre-input-gain)
- **Dry Buffer:** Stores original input for global dry/wet blend

### 2. Crossover Splitter (Minimum Phase)
- **Type:** Linkwitz-Riley (LR) filters — `juce::dsp::LinkwitzRileyFilter`
- **Orders:** 2nd (12 dB/oct), 4th (24 dB/oct), 8th (48 dB/oct)
- **Topology:** Two crossover points produce 3 bands:
  - Low: LR lowpass at `crossover_low_mid`
  - Mid: LR highpass at `crossover_low_mid` → LR lowpass at `crossover_mid_high`
  - High: LR highpass at `crossover_mid_high`
- **Allpass Compensation:** Each filter pair sums flat. The band not split at a given crossover point must pass through the matching allpass to maintain phase alignment.
- **Dynamic Ranges:** When a band is bypassed, its crossover point is removed from the signal path. Adjacent bands absorb the freed spectrum. Implemented by reconfiguring filter frequencies, not by adding/removing filters at runtime.

### 3. Linear Phase Crossover (Optional)
- **Type:** FFT-based FIR crossover
- **Implementation:** Windowed-sinc FIR filters at the same crossover frequencies
- **FFT Size:** 4096 samples (configurable, trades latency for accuracy)
- **Latency:** Reports via `setLatencySamples()` when active — host compensates
- **Switching:** Crossfade between minimum-phase and linear-phase modes to avoid clicks
- **Note:** This is the most complex component. Can be deferred to a later phase.

### 4. Band Processor (x3)
Each band is an identical processing chain:
- **Band Input Gain:** Pre-chain level adjustment
- **M/S Encoder (conditional):** If `band_X_routing` != Stereo, encode to the selected domain
- **Plugin Chain:** 5 serial PluginSlot instances (see below)
- **M/S Decoder (conditional):** Decode back to stereo, recombining with the unprocessed component
- **Band Output Gain:** Post-chain level adjustment
- **Band Dry/Wet:** Blends processed band signal against the unprocessed band signal (captured before input gain)
- **Solo/Mute/Bypass:** Solo mutes other bands; Mute silences this band; Bypass skips the entire band and releases crossover range

### 5. Plugin Slot (x5 per band = x15 total)
Each slot wraps a single hosted VST3 plugin instance:
- **Plugin Instance:** `std::unique_ptr<juce::AudioPluginInstance>` — loaded asynchronously via `AudioPluginFormatManager::createPluginInstanceAsync()`
- **M/S Encoder (conditional):** If `slot_routing` != Stereo and differs from band routing, encode to the slot's domain
- **Process:** Call `pluginInstance->processBlock()` with the slot's buffer
- **M/S Decoder (conditional):** Decode back to the band's routing domain
- **Dry/Wet:** Blend slot output against slot input
- **Bypass:** When bypassed or empty, signal passes through unchanged (zero-cost)

### 6. Plugin Host Manager
Manages the lifecycle of hosted VST3 plugins:
- **Format Manager:** `juce::AudioPluginFormatManager` with VST3 format registered
- **Known Plugin List:** `juce::KnownPluginList` — uses the host DAW's scan results where possible, or performs its own scan
- **Plugin Loading:** Async creation on message thread via callback; audio thread sees the new instance only after an atomic pointer swap
- **Plugin Unloading:** Old instance destroyed on message thread after audio thread releases it
- **Editor Windows:** Each slot can open the hosted plugin's native editor in a standalone window via `pluginInstance->createEditor()`
- **State Persistence:** Hosted plugin states saved/restored via `getStateInformation()` / `setStateInformation()` as part of MultiBlend's own state

### 7. M/S Processor (Utility)
Stateless encoding/decoding used by bands and slots:
- **Encode:** `mid = (L + R) * 0.5`, `side = (L - R) * 0.5`
- **Decode:** `L = mid + side`, `R = mid - side`
- **Left-only:** Process left channel, pass right through unchanged
- **Right-only:** Process right channel, pass left through unchanged
- **Mid-only:** Encode to M/S, process mid, decode back (side unchanged)
- **Side-only:** Encode to M/S, process side, decode back (mid unchanged)

## Processing Chain

```
Input (Stereo)
  │
  ├─── Dry Buffer (capture for global dry/wet)
  │
  ▼
Global Input Gain
  │
  ▼
┌─────────────────────────────────────────────────┐
│           CROSSOVER SPLITTER                    │
│  LR Lowpass ──► LOW band buffer                 │
│  LR HP + LP ──► MID band buffer                 │
│  LR Highpass ──► HIGH band buffer               │
│  (+ allpass compensation per band)              │
└─────────────────────────────────────────────────┘
  │              │              │
  ▼              ▼              ▼
┌──────┐    ┌──────┐    ┌──────┐
│ BAND │    │ BAND │    │ BAND │
│ LOW  │    │ MID  │    │ HIGH │
│      │    │      │    │      │
│ In ──┤    │ In ──┤    │ In ──┤
│ [MS] │    │ [MS] │    │ [MS] │
│ Slot1│    │ Slot1│    │ Slot1│
│ Slot2│    │ Slot2│    │ Slot2│
│ Slot3│    │ Slot3│    │ Slot3│
│ Slot4│    │ Slot4│    │ Slot4│
│ Slot5│    │ Slot5│    │ Slot5│
│ [MS] │    │ [MS] │    │ [MS] │
│ Out──┤    │ Out──┤    │ Out──┤
│ D/W  │    │ D/W  │    │ D/W  │
└──┬───┘    └──┬───┘    └──┬───┘
   │           │           │
   ▼           ▼           ▼
┌─────────────────────────────────────────────────┐
│              BAND SUMMING                       │
│  Low + Mid + High (respecting solo/mute)        │
└─────────────────────────────────────────────────┘
  │
  ▼
Global Dry/Wet (blend with Dry Buffer)
  │
  ▼
Global Output Gain
  │
  ▼
Output (Stereo)
```

### Per-Slot Detail

```
Slot Input
  │
  ├─── Slot Dry Buffer (capture for slot dry/wet)
  │
  ▼
[M/S Encode if slot routing != band routing]
  │
  ▼
VST3 Plugin Instance processBlock()
  │
  ▼
[M/S Decode]
  │
  ▼
Slot Dry/Wet (blend with Slot Dry Buffer)
  │
  ▼
Slot Output → next slot input
```

## Parameter Mapping

| Parameter | Component | Function | Range |
|:----------|:----------|:---------|:------|
| `global_input_gain` | Global I/O | Pre-crossover gain | -24 to +24 dB |
| `global_output_gain` | Global I/O | Post-sum gain | -24 to +24 dB |
| `global_dry_wet` | Global I/O | Final wet/dry blend | 0-100% |
| `linear_phase` | Crossover | Switches LR ↔ FIR crossover | On/Off |
| `crossover_low_mid` | Crossover Splitter | Low-Mid split frequency | 20-20k Hz |
| `crossover_mid_high` | Crossover Splitter | Mid-High split frequency | 20-20k Hz |
| `crossover_slope` | Crossover Splitter | Filter order (12/24/48 dB/oct) | Choice 0-2 |
| `band_X_input_gain` | Band Processor | Pre-chain gain | -24 to +24 dB |
| `band_X_output_gain` | Band Processor | Post-chain gain | -24 to +24 dB |
| `band_X_dry_wet` | Band Processor | Band wet/dry blend | 0-100% |
| `band_X_routing` | Band Processor / M/S | Routing domain | L/R/M/S/Stereo |
| `band_X_solo` | Band Summing | Solo this band | On/Off |
| `band_X_mute` | Band Summing | Mute this band | On/Off |
| `band_X_bypass` | Band Processor + Crossover | Skip band, release crossover | On/Off |
| `band_X_slot_Y_dry_wet` | Plugin Slot | Slot wet/dry blend | 0-100% |
| `band_X_slot_Y_bypass` | Plugin Slot | Skip this slot | On/Off |
| `band_X_slot_Y_routing` | Plugin Slot / M/S | Slot routing domain | L/R/M/S/Stereo |

## Thread Safety Model

| Thread | Operations | Synchronization |
|:-------|:-----------|:----------------|
| **Audio Thread** | `processBlock()` — crossover splitting, band processing, slot processing via hosted plugins, gain staging, dry/wet, M/S encode/decode, band summing | Lock-free. All parameters via `std::atomic<float>`. Plugin instance access via atomic pointer reads. |
| **Message Thread** | Plugin loading/unloading, editor window management, UI updates, state save/restore | Async plugin creation with callback. Atomic pointer swap to hand instance to audio thread. Old instances destroyed here after audio thread releases. |
| **UI Thread (WebView)** | Spectrum display, crossover handle dragging, slot interaction, parameter changes | Communicates with C++ via `window.__JUCE__`. Parameter changes sent as messages, processed on message thread, forwarded to audio thread via atomics. |

### Critical Thread Safety Patterns
- **Plugin Swap:** New `AudioPluginInstance` prepared on message thread (correct sample rate, block size), then swapped in via `std::atomic<AudioPluginInstance*>`. Audio thread checks for new instance each block.
- **No Locks in Audio:** Zero mutexes or allocations on the audio thread. All plugin loading is asynchronous.
- **Parameter Smoothing:** Gain and dry/wet parameters smoothed with `juce::SmoothedValue` to avoid zipper noise.

## Class Structure

```
MultiBlendProcessor : juce::AudioProcessor
├── PluginHostManager
│   ├── juce::AudioPluginFormatManager
│   └── juce::KnownPluginList
├── CrossoverProcessor
│   ├── juce::dsp::LinkwitzRileyFilter (x4: LP+HP per crossover point)
│   ├── juce::dsp::LinkwitzRileyFilter (allpass compensation)
│   └── LinearPhaseCrossover (FIR-based, optional)
├── BandProcessor (x3: low, mid, high)
│   ├── MSProcessor (encode/decode)
│   ├── PluginSlot (x5)
│   │   ├── std::atomic<AudioPluginInstance*>
│   │   ├── MSProcessor (encode/decode)
│   │   ├── juce::SmoothedValue<float> (dry/wet)
│   │   └── std::atomic<bool> (bypass)
│   ├── juce::SmoothedValue<float> (input/output gain, dry/wet)
│   └── std::atomic<bool> (solo, mute, bypass)
├── juce::AudioBuffer<float> (dry buffer, band buffers)
└── juce::SmoothedValue<float> (global gains, global dry/wet)

MultiBlendEditor : juce::AudioProcessorEditor
├── juce::WebBrowserComponent (main UI)
└── PluginEditorWindow (x15, on-demand popup windows for hosted plugin GUIs)
```

## Complexity Assessment

**Score: 5/5 (Research-Level)**

**Rationale:**
- VST3 plugin hosting inside a plugin is inherently complex — thread-safe async loading, state management, editor lifecycle
- Multiband crossover with dynamic ranges and linear phase option
- 15 independent plugin processing slots with per-slot routing
- M/S encoding at two levels (band and slot) with proper decode
- 73 automatable parameters requiring efficient management
- Complex UI: spectrum analyzer, draggable crossovers, plugin browser, 15 slot cards with hosted editor popups
- State persistence must serialize/restore 15 hosted plugin states alongside own parameters

**What keeps it complex:**
- The plugin-hosting-inside-a-plugin pattern is rare and has many edge cases (plugin latency compensation, sample rate changes, buffer size changes propagated to hosted plugins)
- Linear phase crossover via FIR is a separate DSP domain from the IIR crossover
- Dynamic crossover range reconfiguration must be click-free
