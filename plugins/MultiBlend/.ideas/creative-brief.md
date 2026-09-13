# MultiBlend - Creative Brief

## Hook
**"Split. Load. Mix. Any plugin, any band, total control."**

MultiBlend is a multiband VST3 host that splits your audio into three frequency bands and lets you load any third-party VST3 plugin on each one. Five serial plugin slots per band, three tiers of dry/wet control, and per-slot routing options give you surgical multiband processing with any plugins you already own.

## Description

### What It Does
MultiBlend sits on your mix bus (or any channel) and splits the incoming audio into three frequency bands: Low, Mid, and High. Each band has five serial VST3 plugin slots where you can load any third-party VST3 plugin. The plugins process in series (slot 1 feeds slot 2 feeds slot 3, etc.), and each slot has its own dry/wet, bypass, and L/R/M/S routing. Clicking any slot opens the hosted plugin's native GUI.

### Multiband Crossover
The crossover is the heart of the plugin. Two crossover points (Low-Mid and Mid-High) divide the spectrum. The primary control is the crossover slope (12, 24, or 48 dB/oct), with an optional linear phase mode for transparent splitting. A standard Linkwitz-Riley topology ensures flat summing when all bands are at unity.

### Dynamic Crossover Ranges
When a band is bypassed or disabled, its frequency range becomes available to the neighboring band(s):
- **Low off:** Mid stretches down to 20 Hz
- **High off:** Mid stretches up to 20 kHz
- **Low + High off:** Mid becomes full-range (20 Hz - 20 kHz)
- **Mid off:** Low stretches up and High stretches down, sharing a single crossover point

This allows MultiBlend to function as a dual-band or even single-band plugin rack when needed.

### Three-Tier Dry/Wet
- **Per-slot:** Blend each hosted plugin independently
- **Per-band:** Blend the entire processed band chain against the unprocessed band signal
- **Global:** Blend the full multiband output against the original dry input

### Routing Flexibility
Every band and every slot can independently be set to process Left only, Right only, Mid, Side, or full Stereo. This enables scenarios like compressing only the mid-side content of the low band while saturating only the left channel of the highs.

### Volume Staging
Input and output gain controls on every band plus the global level ensure clean gain staging through complex plugin chains.

### What It Doesn't Do
- Does not include any built-in effects (it hosts YOUR plugins)
- Does not support VST2, AU, or AAX plugin formats in slots (VST3 only)
- Does not perform automatic gain compensation between slots
- Does not scan or manage your plugin library (uses the host DAW's plugin list)

## Target User
Mixing and mastering engineers who want multiband processing flexibility without being locked into a single plugin's built-in effects. Power users who already own a large VST3 collection and want to deploy them with surgical frequency precision.

## Sonic Character
Transparent. The crossover and routing are designed to be sonically invisible - all character comes from whatever plugins you load. Linear phase mode available for zero phase distortion at crossover points.

## Visual Identity
- **Clean, light, professional** - inspired by FabFilter's design language
- Light background with subtle gradients
- Clear frequency spectrum visualization showing the three bands
- Color-coded bands (distinct but harmonious pastels)
- Plugin slots as clean rectangular cards with plugin name, bypass, and dry/wet
- Smooth animations for crossover dragging
- High-contrast text for readability
