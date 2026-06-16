# MultiBlend User Manual

**Version 1.0.0**

## What Is MultiBlend?

MultiBlend is a multiband VST3 host plugin. It splits your audio into three frequency bands (Low, Mid, High) and lets you load any third-party VST3 or AU plugin on each band. Five serial plugin slots per band give you surgical multiband processing with any plugins you already own.

## Quick Start

1. Insert MultiBlend on a channel in your DAW
2. Click "Scan" in the plugin browser to find your installed VST3/AU plugins
3. Click an empty slot to load a plugin
4. Click a loaded slot to open that plugin's GUI
5. Adjust crossover frequencies by dragging the handles on the spectrum display
6. Use per-slot and per-band Dry/Wet to blend processing

## Controls

### Global (Header)
- **Linear Phase** - Toggle FFT-based crossover (adds latency, preserves phase)
- **In** - Global input gain (-24 to +24 dB)
- **Out** - Global output gain (-24 to +24 dB)
- **D/W** - Global dry/wet (0-100%)

### Spectrum Analyzer
- Draggable crossover handles set Low-Mid and Mid-High frequencies
- Click the collapse arrow to hide/show the spectrum

### Per Band (Low, Mid, High)
- **In / Out** - Band input/output gain
- **D/W** - Band dry/wet blend
- **Slope** - Crossover slope (6-96 dB/oct, continuous)
- **M/S** - Mid/Side morph (Mid-only to Stereo to Side-only)
- **S** - Solo this band
- **M** - Mute this band
- **B** - Bypass this band (releases crossover range to neighbors)

### Per Slot (5 per band)
- **D/W** - Slot dry/wet (0-100%)
- **B** - Bypass this slot

## Signal Flow

```
Input -> Global In Gain -> Crossover Split ->
  Low:  Band In -> [M/S] -> Slot 1-5 -> [M/S decode] -> Band Out -> Band D/W
  Mid:  Band In -> [M/S] -> Slot 1-5 -> [M/S decode] -> Band Out -> Band D/W
  High: Band In -> [M/S] -> Slot 1-5 -> [M/S decode] -> Band Out -> Band D/W
-> Band Sum -> Global D/W -> Global Out Gain -> Output
```

## System Requirements

- macOS 10.13+ (VST3, AU, Standalone)
- 64-bit DAW with VST3 or AU support
- Third-party VST3/AU plugins to load into slots
