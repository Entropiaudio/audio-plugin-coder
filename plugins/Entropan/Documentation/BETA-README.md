# Entropan — macOS beta

Spectral panner. Lift a band on the spectrum, give it a modulator, let it move.

## Install

Run `Entropan-0.7.0.pkg`. It's signed and notarised by Apple, so it should open
without a Gatekeeper warning. The installer lets you pick VST3, AU, or both:

| Format | Installs to |
|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3` |
| Audio Unit | `/Library/Audio/Plug-Ins/Components` |

Requires **macOS 10.15** or later. Universal — Apple Silicon and Intel.

Logic and GarageBand only see the AU after a rescan; if it doesn't appear,
quit the DAW, then run `killall -9 AudioComponentRegistrar` in Terminal and
reopen.

## Quick start

1. **Double-click the spectrum** to add a band. Drag it — horizontal sets
   frequency, vertical sets lift.
2. Pick a modulator shape: **SINE / TRI / S&H / CHAOS / STEPS / ENV**.
3. **BI / UNI** decides whether the band swings both ways around centre or
   travels one way from it.
4. **CHAOS** (top bar) re-rolls everything random. **Click any knob's label or a
   section title to lock** what you want it to leave alone.

## Worth trying

- **Serial vs Parallel** (top-right of OUTPUT). Serial chains the bands —
  overlapping bands hand off to each other. Parallel sums their displacements,
  so two bands on the same frequency both act on it. Switching crossfades.
- **Step sequencer** (STEPS mode): alt-click a step to split it, shift-click to
  glue steps into one longer step.
- **Drag the MOD dot** onto almost any knob or fader to modulate it. A small
  coloured dot appears on the destination — click it any time to reopen the
  amount dial.

## Reporting bugs

Include: DAW + version, macOS version, Apple Silicon or Intel, and the build
stamp shown under the Entropan logo (e.g. `V0.7.0 · B106`). Session/preset file
attached helps a lot.

Known rough edges are listed with each beta build.
