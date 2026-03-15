# SNIP Bridge - Creative Brief

## Hook
**"Your mix, measured. Your feedback, instant."**
SNIP Bridge is a real-time mix analysis plugin that sits on your master bus, measures your mix across three critical dimensions, compares it against genre-specific profiles, and bridges directly to meetsnip.com for professional feedback — all without touching your audio.

## Description

### What It Does
SNIP Bridge is a **pure pass-through analyzer** — it never colors, processes, or alters the audio signal. It lives on the master bus and performs real-time analysis across three dimensions:

1. **Dynamics (RMS/LUFS):** Measures loudness over time, tracking integrated LUFS, short-term LUFS, and RMS levels. Compares against genre-appropriate loudness targets.

2. **Tonality (Spectral Balance):** Analyzes the frequency spectrum and compares the mix's tonal curve against the expected spectral profile for the selected genre. Identifies areas that are too bright, too muddy, or spectrally unbalanced.

3. **Stereo Width (Correlation):** Monitors stereo correlation and width across frequency bands. Flags mono-compatibility issues and detects over-widened or collapsed stereo images.

### Genre Profiles
Predefined reference profiles for genres like:
- Hip-Hop / Trap
- Pop
- EDM / Dance
- Rock / Alternative
- R&B / Soul
- Lo-Fi / Ambient
- Classical / Jazz

Each profile contains target ranges for LUFS, spectral tilt, low-end ratio, high-frequency energy, and stereo width expectations.

### The Bridge
The "Send to Meetsnip.com" button packages the current analysis snapshot (metrics, genre comparison, mix notes) and sends it to the Snip platform via web API — enabling producers to get professional feedback with data-backed context.

### What It Doesn't Do
- No audio processing or effects
- No gain staging or volume adjustment
- No EQ, compression, or any signal alteration
- Pure measurement and feedback only

## Target User
Producers and mix engineers who want objective, data-driven feedback on their mixes before sending to mastering or seeking professional critique on meetsnip.com.

## Sonic Character
**Transparent.** Zero latency, zero coloration. The plugin is acoustically invisible.

## Visual Identity
- **Dark, modern, flat** — inspired by meetsnip.com's design language
- Charcoal/black background with high-contrast neon accents
- Pink/magenta primary accent color (matching Snip brand)
- Cyan/teal secondary accent for meters and data visualization
- Clean sans-serif typography with generous spacing
- Precision metering tool aesthetic — no vintage, no skeuomorphism
- Data-driven layout: meters, graphs, and text feedback dominate the interface
